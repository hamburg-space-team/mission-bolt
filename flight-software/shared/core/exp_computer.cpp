#include "exp_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include <bolt/wire/payloads.hpp>
#include "platform.hpp"

extern CAN_HandleTypeDef hcan1;

ExpComputer::ExpComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept
    : NodeComputer(platform, i2c, storage), can(can) {
}

void ExpComputer::notify_sync(uint16_t tick) noexcept {
    this->sync_tick = tick;
    this->sync_pending = true;
}

bool ExpComputer::poll_sync(uint16_t& tick_out) noexcept {
    if (!this->sync_pending) {
        return false;
    }
    tick_out = this->sync_tick;
    this->sync_pending = false;
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
    // TX-complete interrupts refill BxcanTransport's frame ring - the tick
    // body never waits for a mailbox.
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY);

    init_storage();
    init_sensors();
    on_experiment_init();

    if (auto len = this->pkt.build_boot(this->tx_buf.data(), this->boot.reason, this->boot.reboot_count,
                                        source_node())) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
        // Critical event: BOOT packet must hit durable storage before
        // any subsequent reset can lose it.
        (void)this->storage.flush();
    }

    // Start the autonomous-fallback grace window from boot-complete, so a
    // node that boots before the CAN master is up still waits the full
    // AUTONOMOUS_TIMEOUT_MS before self-ticking.
    this->last_sync_ms = this->platform.tick_ms();

    this->platform.kick_wdg();
}

void ExpComputer::on_tick(uint32_t tick_start_us, uint16_t /*missed_periods*/) {
    // Error-LED pattern is pure tick counting - run it every tick, including
    // the early-return path below while waiting for SYNC.
    this->leds.error_tick();

    // CAN-TX health: the transport latches after 10 consecutively dropped
    if (!this->can_fault_reported && this->can.is_failed()) {
        this->can_fault_reported = true;
        report_fault(StatusLeds::Fault::CAN_BUS, make_error(ErrorCode::TIMEOUT, Step::CAN_TX_RING));
    }

    // Runtime latches of the common sensors: ship the death trace once.
    poll_device_fault(this->baro, StatusLeds::Fault::BARO, this->baro_fault_reported);
    poll_device_fault(this->tmp, StatusLeds::Fault::TMP, this->tmp_fault_reported);

    const uint32_t now_ms = this->platform.tick_ms();
    uint16_t can_tick = 0U;

    if (poll_sync(can_tick)) {
        // SYNC received -> CAN healthy. Drive on the master's tick.
        this->last_sync_ms = now_ms;
        this->leds.can_tick(can_tick);

        if (this->autonomous) {
            this->autonomous = false;
        } else if (this->last_can_tick != NO_LAST_TICK) {
            const auto diff = static_cast<uint16_t>(can_tick - this->last_can_tick);
            if (diff > 1U && diff <= MAX_REPORTABLE_GAP) {
                const auto first = static_cast<uint16_t>(this->last_can_tick + 1U);
                send_gap(first, static_cast<uint8_t>(diff - 1U), PacketProtocol::GapReason::NO_DATA, tick_start_us);
            }
            // diff == 0 (duplicate SYNC) is ignored as well.
        }
    } else {
        // No SYNC this iteration. A single dropped frame is normal, so wait
        // up to AUTONOMOUS_TIMEOUT_MS before giving up on CAN.
        if (!this->autonomous && (now_ms - this->last_sync_ms) < AUTONOMOUS_TIMEOUT_MS) {
            return;
        }
        // CAN silent past the timeout -> run autonomously on a self-generated
        // tick that continues the sequence, and slow-blink the CAN LED.
        this->autonomous = true;
        can_tick = static_cast<uint16_t>(this->last_can_tick + 1U);
        this->leds.can_lost();
    }
    this->last_can_tick = can_tick;

    // Internal sequencing tick
    this->local_tick++;

    roll_timing_window(can_tick, tick_start_us);
    BOLT_TIME(this->timings, TICK); // whole-tick scope, runs to end of on_tick

    on_experiment_tick(can_tick, tick_start_us);
    send_periodic(can_tick, tick_start_us);
}

// Fold the finished tick's per-scope sums into the window maxima, then ~1 Hz
// downlink the worst-case timings and start a fresh window.
void ExpComputer::roll_timing_window(uint16_t can_tick, uint32_t timestamp_us) {
    this->timings.tick_end();
    if ((this->local_tick % STATUS_INTERVAL) == 0U) {
        send_timing(can_tick, timestamp_us);
        this->timings.reset();
    }
}

// Env every tick, status + flush on their intervals; timed as SEND / STORE.
void ExpComputer::send_periodic(uint16_t can_tick, uint32_t timestamp_us) {
    {
        BOLT_TIME(this->timings, SEND);
        send_env_packet(can_tick, timestamp_us);
        if ((this->local_tick % STATUS_INTERVAL) == 0U) {
            send_status_packet(can_tick, timestamp_us);
        }
    }
    if ((this->local_tick % FLUSH_INTERVAL) == 0U) {
        BOLT_TIME(this->timings, STORE);
        (void)this->storage.flush();
    }
}

void ExpComputer::send_timing(uint16_t can_tick, uint32_t timestamp_us) {
    if (auto len = this->pkt.build_timing(this->tx_buf.data(), PacketProtocol::Tick{can_tick},
                                          PacketProtocol::TimestampUs{timestamp_us}, source_node(), this->timings)) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

void ExpComputer::send_env_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    PayloadExpEnv env{};

    if (auto result = this->baro.read()) {
        env.ms_pressure_raw = result->d1;
        env.ms_temperature_raw = result->d2;
        env.valid_mask |= 0x01U;
    }

    if (auto temp = this->tmp.read()) {
        env.temp_raw = *temp;
        env.valid_mask |= 0x02U;
    }

    if (auto len = this->pkt.build(this->tx_buf.data(), exp_env_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &env,
                                   static_cast<uint8_t>(sizeof(env)))) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

// Interval gating happens in on_tick() on the gap-free local_tick; can_tick
// here is stamp-only.
void ExpComputer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;
    PayloadExpStatus status{};
    status.uptime_s = this->platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(this->storage.is_mounted() ? 0x01U : 0x00U);
    fill_status(status);

    if (auto len = this->pkt.build(this->tx_buf.data(), exp_status_type(), Tick{can_tick}, TimestampUs{timestamp_us},
                                   &status, static_cast<uint8_t>(sizeof(status)))) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

void ExpComputer::report_fault(StatusLeds::Fault code, const Error& err) {
    this->leds.set_fault(code);
    // Header: timestamp = when the error occurred (from the Error
    // itself), tick = CAN tick at report time (0 before the first SYNC).
    const uint16_t tick = (this->last_can_tick != NO_LAST_TICK) ? this->last_can_tick : 0U;
    if (auto len = this->pkt.build_fault(this->tx_buf.data(), PacketProtocol::Tick{tick}, static_cast<uint8_t>(code),
                                         err, source_node())) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

void ExpComputer::send_gap(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason,
                           uint32_t timestamp_us) {
    PacketProtocol::PayloadGapMarker gap{};
    gap.first_missing_tick = first_tick;
    gap.count = count;
    gap.reason = reason;
    gap.source_node = source_node();
    if (auto len = this->pkt.build_gap(this->tx_buf.data(), PacketProtocol::Tick{first_tick},
                                       PacketProtocol::TimestampUs{timestamp_us}, gap)) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}
