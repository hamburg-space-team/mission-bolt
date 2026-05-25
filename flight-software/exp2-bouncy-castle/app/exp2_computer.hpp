#pragma once

#include "can_protocol.hpp"
#include "exp_computer.hpp"
#include "packet_types.hpp"

#include <cstdint>

class Exp2Computer final : public ExpComputer {
  public:
    explicit Exp2Computer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept;

  protected:
    void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) override;

    [[nodiscard]] uint32_t exp_can_id() const noexcept override {
        return CanProtocol::EXP2_DATA_ID;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_env_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_ENV;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_status_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_STATUS;
    }
};
