#include "btc_computer.hpp"
#include "can_protocol.hpp"
#include "main.h"
#include "packet_builder.hpp"
#include "packet_payloads.hpp"
#include "packet_types.hpp"

#include <array>
#include <cstdio>
#include <cstring>

extern CAN_HandleTypeDef hcan1;
extern SD_HandleTypeDef hsd1;
extern RTC_HandleTypeDef hrtc;

namespace {

    void configure_exact_filter(uint32_t stid, uint8_t bank, uint32_t fifo) {
        CAN_FilterTypeDef f{};
        // In 32-bit filter register: STID[10:0] lives in bits [31:21].
        // FilterIdHigh = register[31:16], so STID maps to bits [15:5] of FilterIdHigh.
        f.FilterIdHigh = static_cast<uint16_t>((stid << 5U) & 0xFFFFU);
        f.FilterIdLow = 0U;
        f.FilterMaskIdHigh = 0xFFFFU; // all bits must match (IDE=0, RTR=0, EXID=0 included)
        f.FilterMaskIdLow = 0xFFFFU;
        f.FilterFIFOAssignment = fifo;
        f.FilterBank = bank;
        f.FilterMode = CAN_FILTERMODE_IDMASK;
        f.FilterScale = CAN_FILTERSCALE_32BIT;
        f.FilterActivation = ENABLE;
        HAL_CAN_ConfigFilter(&hcan1, &f);
    }

    uint32_t read_rtc_s() {
        RTC_TimeTypeDef t{};
        RTC_DateTypeDef d{};
        HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
        return static_cast<uint32_t>(t.Hours) * 3600U + static_cast<uint32_t>(t.Minutes) * 60U +
               static_cast<uint32_t>(t.Seconds);
    }

    // ---- SYNC TX ----
    void send_can_sync(uint16_t tick) {
        CAN_TxHeaderTypeDef hdr{};
        hdr.StdId = CanProtocol::SYNC_ID;
        // hdr.IDE = 0 (CAN_ID_STD), hdr.RTR = 0 (CAN_RTR_DATA) - set by zero-init
        hdr.DLC = CanProtocol::SYNC_DLC;
        hdr.TransmitGlobalTime = DISABLE;

        const std::array<uint8_t, CanProtocol::SYNC_DLC> data = {
            static_cast<uint8_t>(tick),
            static_cast<uint8_t>(tick >> 8U),
        };
        uint32_t mailbox = 0U;
        HAL_CAN_AddTxMessage(&hcan1, &hdr, data.data(), &mailbox);
    }

    // ---- Per-EXP reassembly state (fragment -> complete packet) ----
    struct ReassemblySlot {
        std::array<uint8_t, CanProtocol::EXP_DATA_LEN> buf{};
        uint8_t frames_received = 0U;
        uint8_t frames_expected = 0U;
    };

    constexpr uint8_t EXP_COUNT = 3U;
    static std::array<ReassemblySlot, EXP_COUNT> reassembly_g{};

    // ---- Forwarding ring buffer (complete reassembled packets -> UART/SD) ----
    struct RxPacket {
        std::array<uint8_t, CanProtocol::EXP_DATA_LEN> data{};
    };

    // SAFETY: rx_head_g written in ISR, rx_tail_g written in main thread only.
    // uint8_t read/write is atomic on Cortex-M4 -- no mutex needed.
    // Ring buffer drop-on-full strategy: ISR drops if (next == rx_tail_g).
    constexpr uint8_t RX_RING_SIZE = 8U;
    static std::array<RxPacket, RX_RING_SIZE> rx_ring_g{};
    static volatile uint8_t rx_head_g = 0U;
    static volatile uint8_t rx_tail_g = 0U;

    constexpr uint32_t TX_TIMEOUT_MS = 100U;

    // SAFETY: written in EXTI ISR, read in main thread only. bool is atomic on Cortex-M4.
    static volatile bool lo_pending_g = false;

    uint8_t id_to_exp_idx(uint32_t id) {
        if (id == CanProtocol::EXP1_DATA_ID) {
            return 0U;
        }
        if (id == CanProtocol::EXP2_DATA_ID) {
            return 1U;
        }
        if (id == CanProtocol::EXP3_DATA_ID) {
            return 2U;
        }
        return 0xFFU;
    }

} // namespace

// Reassembles bxCAN fragments back into complete PacketProtocol frames, then pushes
// to the ring buffer for on_tick() to forward.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};
    std::array<uint8_t, 8U> raw{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &hdr, raw.data()) != HAL_OK) {
        return;
    }

    auto exp_idx = id_to_exp_idx(hdr.StdId);
    if (exp_idx >= EXP_COUNT) {
        return;
    }

    auto frame_idx = static_cast<uint8_t>(raw[0] >> 4U);
    auto frame_count = static_cast<uint8_t>(raw[0] & 0x0FU);

    ReassemblySlot& slot = reassembly_g[exp_idx];

    // Reset slot on new sequence start; discard mid-sequence if first frame is missing.
    if (frame_idx == 0U) {
        slot = {};
        slot.frames_expected = frame_count;
    } else if (frame_idx != slot.frames_received) {
        slot = {}; // reset
        return;
    }

    auto offset = static_cast<uint8_t>(frame_idx * CanProtocol::BXCAN_BYTES_PER_FRAME);
    if (offset < CanProtocol::EXP_DATA_LEN) {
        auto space = static_cast<uint8_t>(CanProtocol::EXP_DATA_LEN - offset);
        auto chunk = (space < CanProtocol::BXCAN_BYTES_PER_FRAME) ? space : CanProtocol::BXCAN_BYTES_PER_FRAME;
        std::memcpy(slot.buf.data() + offset, raw.data() + 1U, chunk);
    }
    slot.frames_received++;

    if (slot.frames_received == slot.frames_expected) {
        const auto next = static_cast<uint8_t>((rx_head_g + 1U) % RX_RING_SIZE);
        if (next != rx_tail_g) { // drop if ring full
            rx_ring_g[rx_head_g].data = slot.buf;
            rx_head_g = next;
        }
        slot = {};
    }
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LO_Pin) {
        lo_pending_g = true;
    }
}

BtcComputer::BtcComputer(const Platform& platform, CmsisI2CBus& i2c, ARM_DRIVER_USART& usart) noexcept
    : NodeComputer(platform, i2c), usart(usart) {
}

void BtcComputer::on_init() {
    sync_count = boot.tick_valid ? boot.recovered_tick : 0U;

    // One filter bank per EXP - exact ID match into FIFO1 (bxCAN has no range filter)
    configure_exact_filter(CanProtocol::EXP1_DATA_ID, 0U, CAN_RX_FIFO1);
    configure_exact_filter(CanProtocol::EXP2_DATA_ID, 1U, CAN_RX_FIFO1);
    configure_exact_filter(CanProtocol::EXP3_DATA_ID, 2U, CAN_RX_FIFO1);

    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

    usart.Initialize(nullptr);
    usart.PowerControl(ARM_POWER_FULL);
    usart.Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 |
                      ARM_USART_FLOW_CONTROL_NONE,
                  DOWNLINK_BAUD);
    usart.Control(ARM_USART_CONTROL_TX, 1U);

    init_sd(&hsd1);
    init_sensors();

    if (auto len = pkt.build_boot(tx_buf.data(), boot.reason, boot.reboot_count)) {
        usart.Send(tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void BtcComputer::send_gap_to_uart(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason,
                                   uint32_t timestamp_us) {
    PacketProtocol::PayloadGapMarker gap{};
    gap.first_missing_tick = first_tick;
    gap.count = count;
    gap.reason = reason;
    if (auto len = pkt.build_gap(tx_buf.data(), PacketProtocol::Tick{first_tick},
                                 PacketProtocol::TimestampUs{timestamp_us}, gap)) {
        usart_wait();
        usart.Send(tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void BtcComputer::on_sensor_failed() {
    send_gap_to_uart(0U, 1U, PacketProtocol::GapReason::SENSOR_FAILED, 0U);
}

void BtcComputer::init_extra_sensors() {
    if (!imu.init(&i2c)) {
        on_sensor_failed();
    }
}

void BtcComputer::usart_wait() {
    const uint32_t deadline = platform.tick_ms() + TX_TIMEOUT_MS;
    while (usart.GetStatus().tx_busy != 0U) {
        if (platform.tick_ms() > deadline) {
            break;
        }
    }
}

void BtcComputer::drain_exp_frames() {
    while (rx_tail_g != rx_head_g) {
        usart_wait();
        const uint8_t* raw = rx_ring_g[rx_tail_g].data.data();
        const uint8_t payload_len = raw[PacketProtocol::HEADER_LENGTH_OFFSET];
        const uint8_t actual_len = static_cast<uint8_t>(PacketProtocol::HEADER_SIZE) + payload_len +
                                   static_cast<uint8_t>(PacketProtocol::CRC_SIZE);
        usart.Send(raw, actual_len);
        (void)sd.write(raw, actual_len);
        rx_tail_g = static_cast<uint8_t>((rx_tail_g + 1U) % RX_RING_SIZE);
    }
}

void BtcComputer::send_env_packet(uint32_t tick_start_us) {
    using namespace PacketProtocol;

    if (baro.is_failed()) {
        send_gap_to_uart(sync_count, 1U, GapReason::SENSOR_FAILED, tick_start_us);
        return;
    }

    PayloadBtcEnv env{};

    if (auto result = baro.read()) {
        env.ms_pressure = result->d1;
        env.ms_temperature = result->d2;
        env.valid_mask |= 0x01U;
    }

    if (auto temp = tmp.read()) {
        env.temp_raw = *temp;
        env.valid_mask |= 0x02U;
    }

    if (auto imu_result = imu.read_sample()) {
        env.accel_x_raw = imu_result->accel_x;
        env.accel_y_raw = imu_result->accel_y;
        env.accel_z_raw = imu_result->accel_z;
        env.gyro_x_raw = imu_result->gyro_x;
        env.gyro_y_raw = imu_result->gyro_y;
        env.gyro_z_raw = imu_result->gyro_z;
        env.valid_mask |= 0x04U;
    }

    if (auto len = pkt.build(tx_buf.data(), PayloadType::BTC_ENV, Tick{sync_count}, TimestampUs{tick_start_us}, &env,
                             static_cast<uint8_t>(sizeof(env)))) {
        usart_wait();
        usart.Send(tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void BtcComputer::send_status_packet(uint32_t tick_start_us) {
    if ((sync_count % SAVE_INTERVAL) != 0U) {
        return;
    }
    using namespace PacketProtocol;
    PayloadBtcStatus status{};
    status.uptime_s = platform.tick_ms() / 1000U;
    // TODO bit 1 (SD failed) once SdStore tracks consecutive write failures
    status.sd_status = static_cast<uint8_t>(sd.is_mounted() ? 0x01U : 0x00U);

    if (lo_received) {
        status.signal_mask |= 0x01U;
        status.lo_rtc_s = lo_rtc_s;
    }
    if (HAL_GPIO_ReadPin(SOE_GPIO_Port, SOE_Pin) == GPIO_PIN_RESET) {
        status.signal_mask |= 0x02U;
    }
    if (HAL_GPIO_ReadPin(SODS_GPIO_Port, SODS_Pin) == GPIO_PIN_RESET) {
        status.signal_mask |= 0x04U;
    }

    if (auto len = pkt.build(tx_buf.data(), PayloadType::BTC_STATUS, Tick{sync_count}, TimestampUs{tick_start_us},
                             &status, static_cast<uint8_t>(sizeof(status)))) {
        usart_wait();
        usart.Send(tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void BtcComputer::on_tick(uint32_t tick_start_us, uint16_t missed_periods) {
    leds.can_tick(); // blinks every tick; stops if scheduler starves

    if (lo_pending_g) {
        lo_pending_g = false;
        sync_count = 0U;
        lo_rtc_s = read_rtc_s();
        lo_received = true;
    }

    if (missed_periods > 0U) {
        send_gap_to_uart(sync_count, missed_periods, PacketProtocol::GapReason::NO_DATA, tick_start_us);
        sync_count += missed_periods;
    }

    send_can_sync(sync_count);

    drain_exp_frames();
    send_env_packet(tick_start_us);
    send_status_packet(tick_start_us);

    if ((sync_count % SAVE_INTERVAL) == 0U) {
        BootState::save_tick(sync_count);
    }
    (void)sd.flush();
    sync_count++;
}
