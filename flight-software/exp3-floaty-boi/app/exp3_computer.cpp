#include "exp3_computer.hpp"
#include "imu_self_test.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "wcet.hpp"
#include <bolt/wire/payloads.hpp>

extern CAN_HandleTypeDef hcan1;

namespace {
    Exp3Computer* instance_g = nullptr;
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};

    std::array<uint8_t, 8U> data{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data.data()) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && hdr.DLC >= CanProtocol::SYNC_DLC && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        // data[2] = mission mode, data[3] = self-test target; the DLC check
        // above guarantees both are present
        const auto mode = (data[2] == static_cast<uint8_t>(BootState::Mode::FLIGHT)) ? BootState::Mode::FLIGHT
                                                                                    : BootState::Mode::TEST;
        instance_g->notify_sync(tick, mode, data[3]);
    }
}

Exp3Computer::Exp3Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, storage, can) {
    instance_g = this;
}

void Exp3Computer::send_env_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    PayloadExpEnv env{};

    if (auto result = baro.read()) {
        env.ms_pressure_raw = result->d1;
        env.ms_temperature_raw = result->d2;
        env.valid_mask |= 0x01U;
    }

    if (auto temp = tmp.read()) {
        env.temp_raw = *temp;
        env.valid_mask |= 0x02U;
    }

    if (auto len = pkt.build(tx_buf.data(), exp_env_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &env,
                             static_cast<uint8_t>(sizeof(env)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

void Exp3Computer::init_extra_sensors() {
    if (auto r = imu.init(&i2c); !r) {
        imu.disable(r.error());
        leds.set_fault(StatusLeds::Fault::IMU);
    }
}

void Exp3Computer::retry_extra_devices() {
    if (imu.retry_due()) {
        platform.kick_wdg(); // WHO_AM_I retries can be a run of I2C timeouts on a sick bus
        ICM42686 fresh;
        if (fresh.init(&i2c)) {
            imu = fresh;
        } else {
            imu.arm_retry();
            report_fault(StatusLeds::Fault::IMU, imu.last_error());
        }
    }
}

void Exp3Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    // EXP3 controller IMU -> its own EXP3_IMU packet (mirrors BTC_IMU).
    auto sample = [this] {
        BOLT_TIME(timings, READ);
        return imu.read_sample();
    }();

    if (sample) {
        PayloadExp3Imu p{};
        p.accel_x_raw = sample->accel_x;
        p.accel_y_raw = sample->accel_y;
        p.accel_z_raw = sample->accel_z;
        p.gyro_x_raw = sample->gyro_x;
        p.gyro_y_raw = sample->gyro_y;
        p.gyro_z_raw = sample->gyro_z;
        BOLT_TIME(timings, SEND);
        if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP3_IMU, Tick{can_tick}, TimestampUs{timestamp_us}, &p,
                                 static_cast<uint8_t>(sizeof(p)))) {
            can.send(exp_can_id(), tx_buf.data(), *len);
            (void)storage.write(tx_buf.data(), *len);
        }
    }

    // TODO: PayloadExp3Mag - send wired + wireless MMC5983MA readings once driver is ready
}

void Exp3Computer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;
    PayloadExp3Status status{};
    status.sd_status = static_cast<uint8_t>(storage.is_mounted() ? 0x01U : 0x00U);

    if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP3_STATUS, Tick{can_tick}, TimestampUs{timestamp_us},
                             &status, static_cast<uint8_t>(sizeof(status)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

std::optional<PacketProtocol::TestResult> Exp3Computer::step_imu_whoami(NodeComputer& node, bool /*first*/,
                                                                        uint32_t& data) noexcept {
    return ImuSelfTest::whoami(static_cast<Exp3Computer&>(node).imu, data); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

std::optional<PacketProtocol::TestResult> Exp3Computer::step_imu_read(NodeComputer& node, bool /*first*/,
                                                                      uint32_t& data) noexcept {
    return ImuSelfTest::read(static_cast<Exp3Computer&>(node).imu, data); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

std::span<const SelfTest::Step> Exp3Computer::self_test_steps() const noexcept {
    static constexpr std::array<SelfTest::Step, 5U> STEPS = {{
        {&NodeComputer::step_tmp_whoami}, // 0: TMP117 device ID
        {&NodeComputer::step_tmp_read},   // 1: TMP117 raw temperature
        {&NodeComputer::step_baro_prom},  // 2: MS5611 PROM CRC + C1
        {&Exp3Computer::step_imu_whoami}, // 3: ICM-42686 WHO_AM_I
        {&Exp3Computer::step_imu_read},   // 4: ICM-42686 accel/gyro Z
    }};
    return STEPS;
}
