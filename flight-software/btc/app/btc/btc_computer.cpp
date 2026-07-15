#include "btc_computer.hpp"
#include "can_protocol.hpp"
#include "exp_forwarder.hpp"
#include "uplink_handler.hpp"
#include "wcet.hpp"
#include <bolt/wire/header.hpp>
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/types.hpp>
#include <bolt/wire/uplink.hpp>

#include <array>

namespace {

    // EXP data IDs the reassembler tracks and the board installs RX filters for.
    constexpr std::array<uint32_t, 3U> EXP_DATA_IDS = {
        CanProtocol::EXP1_DATA_ID,
        CanProtocol::EXP2_DATA_ID,
        CanProtocol::EXP3_DATA_ID,
    };

} // namespace

BtcComputer::BtcComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, ARM_DRIVER_USART& usart,
                         BtcBoard& board_in) noexcept
    : NodeComputer(platform, i2c, storage), downlink(usart), reassembler(EXP_DATA_IDS), board(board_in),
      emitter(pkt, tx_buf.data(), downlink, storage), imu_sup(platform, i2c) {
}

void BtcComputer::notify_can_frame(uint32_t can_id, const uint8_t* raw) noexcept {
    reassembler.on_frame(can_id, raw);
}

void BtcComputer::notify_lo_edge() noexcept {
    lo_pending = true;
}

void BtcComputer::notify_imu_drdy(uint32_t timestamp_us) noexcept {
    imu_drdy_ts = timestamp_us;
    imu_drdy = true;
}

void BtcComputer::on_init() {
    sync_count = boot.tick_valid ? boot.recovered_tick : 0U;

    // Install one exact-match RX filter per EXP, start CAN, enable RX notify.
    board.start_can(EXP_DATA_IDS);

    downlink.init(DOWNLINK_BAUD);

    init_storage();
    init_sensors();

    emitter.emit_boot(boot.reason, boot.reboot_count, PacketProtocol::NodeId::BTC);
}

void BtcComputer::send_gap_to_uart(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason,
                                   uint32_t timestamp_us) {
    PacketProtocol::PayloadGapMarker gap{};
    gap.first_missing_tick = first_tick;
    gap.count = count;
    gap.reason = reason;
    gap.source_node = PacketProtocol::NodeId::BTC;
    emitter.emit_gap(PacketProtocol::Tick{first_tick}, PacketProtocol::TimestampUs{timestamp_us}, gap);
}

void BtcComputer::poll_uplink(uint32_t timestamp_us) {
    uint8_t byte = 0U;
    PacketProtocol::UplinkParser::Frame frame{};

    while (downlink.rx_pop(byte)) {
        if (uplink.push(byte, frame)) {
            handle_uplink(frame.opcode, frame.seq, timestamp_us);
        }
    }
}

void BtcComputer::handle_uplink(uint8_t opcode, uint8_t seq, uint32_t timestamp_us) {
    const auto status = PacketProtocol::UplinkHandler::classify(opcode);
    emitter.emit_cmd_ack(PacketProtocol::Tick{sync_count}, PacketProtocol::TimestampUs{timestamp_us}, opcode, seq,
                         status);
}

void BtcComputer::report_fault(StatusLeds::Fault code, const Error& err) {
    leds.set_fault(code);
    // Header: timestamp = when the error occurred (from the Error
    // itself), tick = current BTC tick at report time (0 during init).
    emitter.emit_fault(PacketProtocol::Tick{sync_count}, static_cast<uint8_t>(code), err, PacketProtocol::NodeId::BTC);
}

void BtcComputer::init_extra_sensors() {
    if (auto r = imu_sup.init(); !r) {
        leds.set_fault(StatusLeds::Fault::IMU);
    }
}

void BtcComputer::retry_extra_devices() {
    if (auto fault = imu_sup.retry()) {
        report_fault(StatusLeds::Fault::IMU, *fault);
    }
}

void BtcComputer::send_imu_packet(uint32_t timestamp_us) {
    using namespace PacketProtocol;

    auto sample = [this] {
        BOLT_TIME(timings, READ);
        return imu_sup.read_sample();
    }();
    if (!sample) {
        return; // runtime latch is reported once by the on_tick IMU one-shot
    }

    PayloadBtcImu p{};
    p.accel_x_raw = sample->accel_x;
    p.accel_y_raw = sample->accel_y;
    p.accel_z_raw = sample->accel_z;
    p.gyro_x_raw = sample->gyro_x;
    p.gyro_y_raw = sample->gyro_y;
    p.gyro_z_raw = sample->gyro_z;

    emitter.emit(PayloadType::BTC_IMU, Tick{sync_count}, TimestampUs{timestamp_us}, &p,
                 static_cast<uint8_t>(sizeof(p)));
}

void BtcComputer::send_timing(uint32_t timestamp_us) {
    emitter.emit_timing(PacketProtocol::Tick{sync_count}, PacketProtocol::TimestampUs{timestamp_us},
                        PacketProtocol::NodeId::BTC, timings);
}

// Called from main thread to drain the UART ring before a deadline. The
void BtcComputer::on_drain(uint32_t deadline_ms) {
    while (true) {
        const uint32_t now = platform.tick_ms();
        if ((now + MIN_TIME_FOR_WRITE_MS) >= deadline_ms) {
            return;
        }
        if (imu_drdy) {
            const uint32_t drdy_ts = imu_drdy_ts;
            imu_drdy = false;
            send_imu_packet(drdy_ts);
            continue;
        }
        (void)storage.drain_one(); // no-op once the ring is empty
    }
}

void BtcComputer::send_env_packet(uint32_t tick_start_us) {
    using namespace PacketProtocol;

    PayloadBtcEnv env{};

    {
        BOLT_TIME(timings, READ);
        if (auto result = baro.read()) {
            env.ms_pressure_raw = result->d1;
            env.ms_temperature_raw = result->d2;
            env.valid_mask |= 0x01U;
        }

        if (auto temp = tmp.read()) {
            env.temp_raw = *temp;
            env.valid_mask |= 0x02U;
        }
    }

    emitter.emit(PayloadType::BTC_ENV, Tick{sync_count}, TimestampUs{tick_start_us}, &env,
                 static_cast<uint8_t>(sizeof(env)));
}

void BtcComputer::send_status_packet(uint32_t tick_start_us) {
    if ((sync_count % SAVE_INTERVAL) != 0U) {
        return;
    }
    using namespace PacketProtocol;
    PayloadBtcStatus status{};
    status.uptime_s = platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(storage.is_mounted() ? 0x01U : 0x00U);

    if (lo.received()) {
        status.signal_mask |= 0x01U;
        status.lo_rtc_s = lo.rtc_s();
    }
    if (board.soe_asserted()) {
        status.signal_mask |= 0x02U;
    }
    if (board.sods_asserted()) {
        status.signal_mask |= 0x04U;
    }

    emitter.emit(PayloadType::BTC_STATUS, Tick{sync_count}, TimestampUs{tick_start_us}, &status,
                 static_cast<uint8_t>(sizeof(status)));
}

void BtcComputer::on_tick(uint32_t tick_start_us, uint16_t missed_periods) {
    leds.error_tick();

    timings.tick_end();
    send_timing(tick_start_us);
    timings.reset();

    BOLT_TIME(timings, TICK);
    poll_uplink(tick_start_us);

    if (downlink.is_failed()) {
        report_fault(StatusLeds::Fault::UART, make_error(ErrorCode::TIMEOUT, Step::UART_TX_RING));
    }

    retry_failed_devices(); // common sensors + IMU (via retry_extra_devices)

    bool lo_edge = false;
    if (lo_pending) {
        lo_pending = false;
        lo_edge = true;
    }
    if (lo.service(lo_edge, board.lo_asserted(), [this] { return board.rtc_now_s(); })) {
        sync_count = 0U;
    }

    if (missed_periods > 0U) {
        send_gap_to_uart(sync_count, missed_periods, PacketProtocol::GapReason::NO_DATA, tick_start_us);
        sync_count += missed_periods;
    }

    leds.can_tick(sync_count);
    board.send_sync(sync_count);

    {
        BOLT_TIME(timings, SEND);
        exp_forwarder::drain(reassembler, downlink);
        send_env_packet(tick_start_us);
        send_status_packet(tick_start_us);
    }

    if ((sync_count % SAVE_INTERVAL) == 0U) {
        BOLT_TIME(timings, STORE);
        BootState::save_tick(sync_count);
        (void)storage.flush();
    }

    sync_count++;
}
