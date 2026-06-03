#include "exp3_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "packet_payloads.hpp"

extern CAN_HandleTypeDef hcan1;

namespace {
    Exp3Computer* instance_g = nullptr;
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};
    uint8_t data[CanProtocol::SYNC_DLC]{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        instance_g->notify_sync(tick);
    }
}

Exp3Computer::Exp3Computer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, can) {
    instance_g = this;
}

void Exp3Computer::send_env_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    PayloadExp3Env env{};

    if (auto result = baro.read()) {
        env.ms_pressure = result->d1;
        env.ms_temperature = result->d2;
    }

    if (auto temp = tmp.read()) {
        env.tmp_raw = *temp;
    }

    if (auto imu_result = imu.read_sample()) {
        env.imu_accel_x_raw = imu_result->accel_x;
        env.imu_accel_y_raw = imu_result->accel_y;
        env.imu_accel_z_raw = imu_result->accel_z;
        env.imu_gyro_x_raw = imu_result->gyro_x;
        env.imu_gyro_y_raw = imu_result->gyro_y;
        env.imu_gyro_z_raw = imu_result->gyro_z;
    }

    if (auto len = pkt.build(tx_buf.data(), exp_env_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &env,
                             static_cast<uint8_t>(sizeof(env)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}

void Exp3Computer::init_extra_sensors() {
    if (!imu.init(&i2c)) {
        on_sensor_failed();
    }
}

void Exp3Computer::on_experiment_tick(uint16_t /*can_tick*/, uint32_t /*timestamp_us*/) {
    // TODO: PayloadExp3Mag - send wired + wireless MMC5983MA readings once driver is ready
}

void Exp3Computer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    if ((can_tick % STATUS_INTERVAL) != 0U) {
        return;
    }
    using namespace PacketProtocol;
    PayloadExp3Status status{};
    // Scheduler/latency fields stay 0 until the duty-cycle pipeline lands.
    status.sd_status = static_cast<uint8_t>(sd.is_mounted() ? 0x01U : 0x00U);

    if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP3_STATUS, Tick{can_tick}, TimestampUs{timestamp_us},
                             &status, static_cast<uint8_t>(sizeof(status)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)sd.write(tx_buf.data(), *len);
    }
}
