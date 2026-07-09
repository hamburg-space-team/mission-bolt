#include "btc_computer.hpp"
#include "can_protocol.hpp"
#include "main.h"
#include "packet_builder.hpp"
#include "packet_header.hpp"
#include "packet_payloads.hpp"
#include "packet_types.hpp"
#include "timing.hpp"

#include <array>

extern CAN_HandleTypeDef hcan1;
extern SD_HandleTypeDef hsd1;
extern RTC_HandleTypeDef hrtc;

namespace {

    // EXP data IDs the reassembler tracks
    constexpr std::array<uint32_t, 3U> EXP_DATA_IDS = {
        CanProtocol::EXP1_DATA_ID,
        CanProtocol::EXP2_DATA_ID,
        CanProtocol::EXP3_DATA_ID,
    };

    BtcComputer* instance_g = nullptr;

    // SAFETY: written in EXTI ISR, read in main thread. bool read/write
    // is atomic on Cortex-M4.
    volatile bool lo_pending_g = false;

    // IMU data-ready pipeline
    volatile bool imu_drdy_g = false;
    volatile uint32_t imu_drdy_ts_g = 0U;

    // PB11 = IMU_INT1 (CubeMX label pending; EXTI11 on EXTI15_10_IRQn)
    constexpr uint16_t IMU_INT1_PIN = GPIO_PIN_11;

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

} // namespace

// HAL callback: pulls one bxCAN frame from FIFO1 and forwards it to
// the reassembler. The reassembly state lives in CanReassembler.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};
    std::array<uint8_t, 8U> raw{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &hdr, raw.data()) != HAL_OK) {
        return;
    }
    if (instance_g != nullptr) {
        instance_g->notify_can_frame(hdr.StdId, raw.data());
    }
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LO_Pin) {
        lo_pending_g = true;
    }
    if (GPIO_Pin == IMU_INT1_PIN) {
        imu_drdy_ts_g = Timing::us_now();
        imu_drdy_g = true;
    }
}

BtcComputer::BtcComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, ARM_DRIVER_USART& usart) noexcept
    : NodeComputer(platform, i2c, storage), downlink(usart), reassembler(EXP_DATA_IDS), sync_tx(hcan1) {
    instance_g = this;
}

void BtcComputer::notify_can_frame(uint32_t can_id, const uint8_t* raw) noexcept {
    reassembler.on_frame(can_id, raw);
}

void BtcComputer::on_init() {
    sync_count = boot.tick_valid ? boot.recovered_tick : 0U;

    // One filter bank per EXP - exact ID match into FIFO1 (bxCAN has no range filter)
    configure_exact_filter(CanProtocol::EXP1_DATA_ID, 0U, CAN_RX_FIFO1);
    configure_exact_filter(CanProtocol::EXP2_DATA_ID, 1U, CAN_RX_FIFO1);
    configure_exact_filter(CanProtocol::EXP3_DATA_ID, 2U, CAN_RX_FIFO1);

    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

    downlink.init(DOWNLINK_BAUD);

    init_storage();
    init_sensors();

    if (auto len = pkt.build_boot(tx_buf.data(), boot.reason, boot.reboot_count)) {
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
        // Critical event: BOOT must hit durable storage before any
        // reset could lose it.
        (void)storage.flush();
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
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

void BtcComputer::report_fault(StatusLeds::Fault code, const Error& err) {
    leds.set_fault(code);
    // Header: timestamp = when the error occurred (from the Error
    // itself), tick = current BTC tick at report time (0 during init).
    if (auto len = pkt.build_fault(tx_buf.data(), PacketProtocol::Tick{sync_count}, static_cast<uint8_t>(code), err)) {
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

void BtcComputer::init_extra_sensors() {
    if (auto r = imu.init(&i2c, ICM42686_ADDR, ICM42686Odr::ODR_200HZ, true); !r) {
        imu_fault_reported = true; // reported here, not by the tick poll
        report_fault(StatusLeds::Fault::IMU, r.error());
    }
}

void BtcComputer::send_imu_packet(uint32_t timestamp_us) {
    using namespace PacketProtocol;

    auto sample = imu.read_sample();
    if (!sample) {
        return; // runtime latch is reported once by poll_device_fault()
    }

    PayloadBtcImu p{};
    p.accel_x_raw = sample->accel_x;
    p.accel_y_raw = sample->accel_y;
    p.accel_z_raw = sample->accel_z;
    p.gyro_x_raw = sample->gyro_x;
    p.gyro_y_raw = sample->gyro_y;
    p.gyro_z_raw = sample->gyro_z;

    if (auto len = pkt.build(tx_buf.data(), PayloadType::BTC_IMU, Tick{sync_count}, TimestampUs{timestamp_us}, &p,
                             static_cast<uint8_t>(sizeof(p)))) {
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

// Overrides NodeComputer::on_drain: instead of returning to run()'s
// blocking sleep once the SD ring is empty, keep polling the DRDY flag
// until the tick deadline - a 200 Hz sample must not wait 40 ms for the
// next tick.
void BtcComputer::on_drain(uint32_t deadline_ms) {
    while (true) {
        const uint32_t now = platform.tick_ms();
        if ((now + MIN_TIME_FOR_WRITE_MS) >= deadline_ms) {
            return;
        }
        if (imu_drdy_g) {
            const uint32_t drdy_ts = imu_drdy_ts_g;
            imu_drdy_g = false;
            send_imu_packet(drdy_ts);
            continue;
        }
        (void)storage.drain_one(); // no-op once the ring is empty
    }
}

void BtcComputer::drain_exp_frames() {
    std::array<uint8_t, PacketProtocol::MAX_PACKET_SIZE> buf{};
    uint8_t len = 0U;
    while (reassembler.pop(buf.data(), len)) {
        downlink.send(buf.data(), len);
        (void)storage.write(buf.data(), len);
    }
}

void BtcComputer::send_env_packet(uint32_t tick_start_us) {
    using namespace PacketProtocol;

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
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
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
    status.sd_status = static_cast<uint8_t>(storage.is_mounted() ? 0x01U : 0x00U);

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
        downlink.send(tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

void BtcComputer::on_tick(uint32_t tick_start_us, uint16_t missed_periods) {
    // Error-LED pattern is pure tick counting - run it every tick.
    leds.error_tick();

    // Downlink health: Rs422Downlink latches after 10 consecutive dropped frames.
    if (!uart_fault_reported && downlink.is_failed()) {
        uart_fault_reported = true;
        report_fault(StatusLeds::Fault::UART, make_error(ErrorCode::TIMEOUT, Step::UART_TX_RING));
    }

    // Runtime latches of the sensors: ship the death trace once.
    poll_device_fault(baro, StatusLeds::Fault::BARO, baro_fault_reported);
    poll_device_fault(tmp, StatusLeds::Fault::TMP, tmp_fault_reported);
    poll_device_fault(imu, StatusLeds::Fault::IMU, imu_fault_reported);

    if (lo_pending_g) {
        lo_pending_g = false;
        sync_count = 0U;
        lo_rtc_s = read_rtc_s();
        lo_received = true;
    } else if (!lo_received && HAL_GPIO_ReadPin(LO_GPIO_Port, LO_Pin) == GPIO_PIN_RESET) {
        if (++lo_level_ticks >= LO_LEVEL_DEBOUNCE_TICKS) {
            sync_count = 0U;
            lo_rtc_s = read_rtc_s();
            lo_received = true;
        }
    } else {
        lo_level_ticks = 0U;
    }

    if (missed_periods > 0U) {
        send_gap_to_uart(sync_count, missed_periods, PacketProtocol::GapReason::NO_DATA, tick_start_us);
        sync_count += missed_periods;
    }

    // LED phase derives from the exact tick value broadcast below, so the
    // BTC and EXP CAN LEDs blink in lockstep.
    leds.can_tick(sync_count);
    sync_tx.send(sync_count);

    drain_exp_frames();
    send_env_packet(tick_start_us);
    send_status_packet(tick_start_us);

    if ((sync_count % SAVE_INTERVAL) == 0U) {
        BootState::save_tick(sync_count);
        (void)storage.flush();
    }
    sync_count++;
}
