#pragma once

#include "as7265x.hpp"
#include "can_protocol.hpp"
#include "exp_computer.hpp"
#include "lp5810.hpp"
#include "packet_types.hpp"

#include <cstdint>

// Measurement cycle (10 ticks × 40ms = 400ms):
// Phase 0:  start DARK
// Phase 1:  wait
// Phase 2:  read DARK -> send dark -> start RGB
// Phase 3:  wait
// Phase 4:  read RGB -> send RGB -> start WHITE
// Phase 5:  wait
// Phase 6:  read WHITE -> send white -> start IR
// Phase 7:  wait
// Phase 8:  read IR -> send IR -> start UV
// Phase 9:  wait
// Phase 10: read UV -> send UV -> disable LEDs

class Exp1Computer final : public ExpComputer {
  public:
    explicit Exp1Computer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept;

  protected:
    void on_experiment_init() noexcept override;
    void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) override;

    [[nodiscard]] uint32_t exp_can_id() const noexcept override {
        return CanProtocol::EXP1_DATA_ID;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_env_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP1_ENV;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_status_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP1_STATUS;
    }

  private:
    // Hardware addresses
    static constexpr uint8_t LP5810C_ADDR = 0x14U; // RGB LED driver
    static constexpr uint8_t LP5810D_ADDR = 0x15U; // UV/IR LED driver
    // LP5810C channel mask: OUT0=R, OUT1=G, OUT2=B
    static constexpr uint8_t RGB_CHANNELS = 0x07U;
    // LP5810D channel masks: OUT0=white, OUT1=IR 940nm, OUT2=UV 400nm
    static constexpr uint8_t WHITE_CHANNEL = 0x01U;
    static constexpr uint8_t IR_CHANNEL = 0x02U;
    static constexpr uint8_t UV_CHANNEL = 0x04U;
    // led_mask values (matches PayloadExp1SpectrumA.led_mask)
    static constexpr uint8_t LED_DARK = 0x00U;
    static constexpr uint8_t LED_RGB = 0x01U;
    static constexpr uint8_t LED_WHITE = 0x02U;
    static constexpr uint8_t LED_IR = 0x03U;
    static constexpr uint8_t LED_UV = 0x04U;

    static constexpr uint8_t MEAS_COUNT = 5U;   // dark, rgb, white, ir, uv
    static constexpr uint8_t PHASE_COUNT = 11U; // ticks per cycle

    void send_spectrum_pair(uint8_t meas_idx);
    void sensor_init() noexcept;
    void try_spec_recovery();

    enum class RecoveryStep : uint8_t { IDLE, ASSERTING, WAITING };

    AS7265X spec{};
    LP5810 lp5810_rgb{};
    LP5810 lp5810_uv_ir{};

    bool cycle_active = false;
    bool spec_reset_attempted = false;
    uint8_t meas_phase = 0U;
    RecoveryStep recovery_step = RecoveryStep::IDLE;
    uint8_t recovery_ticks = 0U;

    AS7265XResult result{};
    uint32_t start_timestamp_us = 0U;
    uint16_t start_tick = 0U;
    bool result_valid = false;
};
