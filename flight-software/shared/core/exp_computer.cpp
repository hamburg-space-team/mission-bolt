#include "exp_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "platform.hpp"
#include <bolt/wire/payloads.hpp>

extern CAN_HandleTypeDef hcan1;

ExpComputer::ExpComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept
    : NodeComputer(platform, i2c, storage), can(can) {
}

void ExpComputer::notify_sync(uint16_t tick, BootState::Mode mode_in, uint8_t test_target_in) noexcept {
    this->sync_tick = tick;
    this->sync_mode = mode_in;
    this->sync_test_target = test_target_in;
    this->sync_pending = true; // publish last: the reader tests this flag first
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
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY);

    init_storage();
    init_sensors();
    on_experiment_init();

    if (auto len =
            this->pkt.build_boot(this->tx_buf.data(), this->boot.reason, this->boot.reboot_count, source_node())) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
        (void)this->storage.flush();
    }

    // Start the autonomous-fallback grace window from boot-complete, so a
    // node that boots before the CAN master is up still waits the full
    // AUTONOMOUS_TIMEOUT_MS before self-ticking.
    this->last_sync_ms = this->platform.tick_ms();

    this->platform.kick_wdg();
}

void ExpComputer::on_tick(uint32_t tick_start_us, uint16_t /*missed_periods*/) {
    this->leds.error_tick();

    if (this->can.is_failed()) {
        report_fault(StatusLeds::Fault::CAN_BUS, make_error(ErrorCode::TIMEOUT, Step::CAN_TX_RING));
    }

    retry_failed_devices();

    const uint32_t now_ms = this->platform.tick_ms();
    uint16_t can_tick = 0U;

    if (poll_sync(can_tick)) {
        on_sync_received(can_tick, now_ms, tick_start_us);
    } else {
        if (!this->autonomous && (now_ms - this->last_sync_ms) < AUTONOMOUS_TIMEOUT_MS) {
            return;
        }

        this->autonomous = true;
        can_tick = static_cast<uint16_t>(this->last_can_tick + 1U);
        this->leds.can_lost();
    }

    this->last_can_tick = can_tick;

    // Internal sequencing tick
    this->local_tick++;

    send_timing_packet(can_tick, tick_start_us);
    BOLT_TIME(this->timings, TICK);

    dispatch_mode_tick(can_tick, tick_start_us);

    BOLT_TIME(this->timings, SEND);
    send_env_packet(can_tick, tick_start_us);
    if ((this->local_tick % STATUS_INTERVAL) == 0U) {
        send_status_packet(can_tick, tick_start_us);
    }
}

void ExpComputer::on_sync_received(uint16_t can_tick, uint32_t now_ms, uint32_t tick_start_us) {
    // the BTC owns the mode; a transition resets the experiment
    if (this->mode != this->sync_mode) {
        this->mode = this->sync_mode;
        on_experiment_reset();
    }
    this->test_target = this->sync_test_target;
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
    }
}

void ExpComputer::dispatch_mode_tick(uint16_t can_tick, uint32_t timestamp_us) {
    // In autonomous fallback `mode` keeps its last value (cold start: TEST) -
    // an EXP that never hears the BTC must never decide it is flying
    if (this->mode == BootState::Mode::FLIGHT) {
        on_experiment_tick(can_tick, timestamp_us);
    } else {
        service_self_test(can_tick, timestamp_us);
        on_test_tick(can_tick, timestamp_us);
    }
}

void ExpComputer::service_self_test(uint16_t can_tick, uint32_t timestamp_us) {
    if (this->test_target != static_cast<uint8_t>(source_node())) {
        // not our turn: drop report + half-finished run, so the next turn
        // starts at step 0; the hook parks what the aborted step left engaged
        this->last_report.reset();
        if (this->tester.active()) {
            this->tester.abort();
            on_self_test_abort();
        }
        return;
    }

    // first tick of our turn; last_report outlives the runner on the final step
    if (!this->tester.active() && !this->last_report) {
        this->tester.start(self_test_steps());
    }

    if (this->tester.active()) {
        if (const auto report = this->tester.step()) {
            this->last_report = report;
            send_test_packet(can_tick, timestamp_us, *report);
        }
    } else if (this->last_report) {
        // run done but the turn not moved on yet - re-send so a dropped
        // upstream frame self-heals within a tick
        send_test_packet(can_tick, timestamp_us, *this->last_report);
    }
}

void ExpComputer::send_test_packet(uint16_t can_tick, uint32_t timestamp_us, const SelfTest::Report& report) {
    using namespace PacketProtocol;

    PayloadTest p{};
    p.test_id = report.test_id;
    p.result = report.result;
    p.last = report.last ? 1U : 0U;
    p.data = report.data;

    // normal EXP data path: the BTC forwards to ground and reads `last` off
    // the same frame
    auto built = this->pkt.build(this->tx_buf.data(), exp_test_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &p,
                                 static_cast<uint8_t>(sizeof(p)));
    if (built) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *built);
    }
}

void ExpComputer::send_timing_packet(uint16_t can_tick, uint32_t timestamp_us) {
    this->timings.tick_end();
    send_timing(can_tick, timestamp_us);
    this->timings.reset();
}

void ExpComputer::send_timing(uint16_t can_tick, uint32_t timestamp_us) {
    if (auto len = this->pkt.build_timing(this->tx_buf.data(), exp_timing_type(), PacketProtocol::Tick{can_tick},
                                          PacketProtocol::TimestampUs{timestamp_us}, this->timings)) {
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

void ExpComputer::report_fault(StatusLeds::Fault code, const Error& err) {
    this->leds.set_fault(code);

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
