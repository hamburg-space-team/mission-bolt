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

    std::array<uint8_t, 8U> data{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data.data()) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && hdr.DLC >= CanProtocol::SYNC_DLC && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        instance_g->notify_sync(tick);
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
        (void)storage.write(tx_buf.data(), *len);
    }
}

void Exp3Computer::init_extra_sensors() {
    if (auto r = imu.init(&i2c); !r) {
        imu_fault_reported = true; // reported here, not by the tick poll
        report_fault(StatusLeds::Fault::IMU, r.error());
    }
}

void Exp3Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;

    poll_device_fault(imu, StatusLeds::Fault::IMU, imu_fault_reported);

    // EXP3 controller IMU -> its own EXP3_IMU packet (mirrors BTC_IMU).
    if (auto sample = imu.read_sample()) {
        PayloadExp3Imu p{};
        p.accel_x_raw = sample->accel_x;
        p.accel_y_raw = sample->accel_y;
        p.accel_z_raw = sample->accel_z;
        p.gyro_x_raw = sample->gyro_x;
        p.gyro_y_raw = sample->gyro_y;
        p.gyro_z_raw = sample->gyro_z;
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
