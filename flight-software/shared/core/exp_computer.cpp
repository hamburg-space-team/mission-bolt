#include "exp_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "packet_payloads.hpp"

extern CAN_HandleTypeDef hcan1;
extern SD_HandleTypeDef hsd1;

ExpComputer::ExpComputer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept
    : NodeComputer(platform, i2c), can(can) {
}

void ExpComputer::notify_sync(uint16_t tick) noexcept {
    sync_tick = tick;
    sync_pending = true;
}

bool ExpComputer::poll_sync(uint16_t& tick_out) noexcept {
    if (!sync_pending) {
        return false;
    }
    tick_out = sync_tick;
    sync_pending = false;
    return true;
}

void ExpComputer::on_init() {
    CAN_FilterTypeDef filter{};
    filter.FilterIdHigh = static_cast<uint16_t>((CanProtocol::SYNC_ID << 5U) & 0xFFFFU);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = 0xFFFFU;
    filter.FilterMaskIdLow = 0xFFFFU;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterBank = 0U;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &filter);

    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    init_sd(&hsd1);
    init_sensors();
    on_experiment_init();

    if (auto len = pkt.build_boot(tx_buf.data(), boot.reason, boot.reboot_count)) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void ExpComputer::on_tick(uint32_t tick_start_us, uint16_t /*missed_periods*/) {
    uint16_t can_tick = 0U;
    if (!poll_sync(can_tick)) {
        leds.can_lost();
        return;
    }
    leds.can_tick();

    if (last_can_tick != NO_LAST_TICK && can_tick != static_cast<uint16_t>(last_can_tick + 1U)) {
        const auto first = static_cast<uint16_t>(last_can_tick + 1U);
        const auto missed = static_cast<uint8_t>(can_tick - last_can_tick - 1U);
        send_gap(first, missed, PacketProtocol::GapReason::NO_DATA, tick_start_us);
    }
    last_can_tick = can_tick;

    send_env_packet(can_tick, tick_start_us);
    send_status_packet(can_tick, tick_start_us);
    on_experiment_tick(can_tick, tick_start_us);

    (void)sd.flush();
}

void ExpComputer::send_env_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    PayloadExpEnv env{};

    if (auto result = baro.read()) {
        env.ms_pressure = result->d1;
        env.ms_temperature = result->d2;
        env.valid_mask |= 0x01U;
    }

    if (auto temp = tmp.read()) {
        env.temp_raw = *temp;
        env.valid_mask |= 0x02U;
    }

    if (auto len = pkt.build(tx_buf.data(), exp_env_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &env,
                             static_cast<uint8_t>(sizeof(env)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void ExpComputer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    if ((can_tick % STATUS_INTERVAL) != 0U) {
        return;
    }
    using namespace PacketProtocol;
    PayloadExpStatus status{};
    status.uptime_s = platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(sd.is_mounted() ? 0x01U : 0x00U);

    if (auto len = pkt.build(tx_buf.data(), exp_status_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &status,
                             static_cast<uint8_t>(sizeof(status)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void ExpComputer::on_sensor_failed() {
    leds.error_set();
    send_gap(0U, 1U, PacketProtocol::GapReason::SENSOR_FAILED, 0U);
}

void ExpComputer::send_gap(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason,
                           uint32_t timestamp_us) {
    PacketProtocol::PayloadGapMarker gap{};
    gap.first_missing_tick = first_tick;
    gap.count = count;
    gap.reason = reason;
    if (auto len = pkt.build_gap(tx_buf.data(), PacketProtocol::Tick{first_tick},
                                 PacketProtocol::TimestampUs{timestamp_us}, gap)) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}
