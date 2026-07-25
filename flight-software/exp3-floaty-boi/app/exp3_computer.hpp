#pragma once

#include "can_protocol.hpp"
#include "exp_computer.hpp"
#include "icm42686.hpp"
#include <bolt/wire/types.hpp>

#include <cstdint>

/// EXP3 "Floaty Boi" controller.
///
/// The wired magnetometer for the EXP3 stack (F-60) and the burst
/// pipeline producing PayloadExp3StackA / PayloadExp3StackB is still
/// under development; tick body is a placeholder.
///
/// @ingroup apps
class Exp3Computer final : public ExpComputer {
  public:
    explicit Exp3Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept;

  protected:
    void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) override;

    [[nodiscard]] uint32_t exp_can_id() const noexcept override {
        return CanProtocol::EXP3_DATA_ID;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_env_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP3_ENV;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_status_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP3_STATUS;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_timing_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP3_TIMING;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_test_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP3_TEST;
    }

    void send_env_packet(uint16_t can_tick, uint32_t timestamp_us) override;
    void send_status_packet(uint16_t can_tick, uint32_t timestamp_us) override;
    [[nodiscard]] std::span<const SelfTest::Step> self_test_steps() const noexcept override;
    void init_extra_sensors() override;
    void retry_extra_devices() override;

  private:
    ICM42686 imu{};

    static std::optional<PacketProtocol::TestResult> step_imu_whoami(NodeComputer& node, bool first,
                                                                     uint32_t& data) noexcept;
    static std::optional<PacketProtocol::TestResult> step_imu_read(NodeComputer& node, bool first,
                                                                   uint32_t& data) noexcept;
};
