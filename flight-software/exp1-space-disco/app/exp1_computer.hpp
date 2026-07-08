#pragma once

#include "as7265x.hpp"
#include "can_protocol.hpp"
#include "exp_computer.hpp"
#include "lp5810.hpp"
#include "packet_types.hpp"

#include <cstdint>

/// EXP1 "Space Disco" controller. F-10/F-20: measures UV, IR and
/// visible-light wavelengths and records timestamped sent/received
/// intensities for post-flight correlation.
///
/// Runs a measurement MATRIX (table in exp1_computer.cpp): every row
/// combines an LED configuration (each LED alone, all together, dark
/// reference) with a PWM brightness (25/50/75/100 %) and an integration
/// time (25/50 cycles).A full matrix pass is 58 rows / 464 ticks
/// (~19 s; the one-shot mode converts both channel banks, so integration
/// = 2 x IT x 2.8 ms). led_mask in the spectrum packets carries the
/// MATRIX ROW INDEX (ICD-007).
///
/// @ingroup apps
class Exp1Computer final : public ExpComputer {
  public:
    explicit Exp1Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept;

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
    // LP5810 page-0 base address = 0b101 | variant-bits | page(00). Per the
    // datasheet (Table 7-4): variant C = 0x58, variant D = 0x5C.
    static constexpr uint8_t LP5810C_ADDR = 0x58U; // RGB LED driver   (0x58-0x5B)
    static constexpr uint8_t LP5810D_ADDR = 0x5CU; // UV/IR LED driver (0x5C-0x5F)
    // Per-channel dot current (0xFF = full scale, 51 mA at max_current=1).
    static constexpr uint8_t LP5810_DOT_CURRENT = 0xFFU;
    static constexpr bool LP5810_HIGH_CURRENT = false;
    // AS7265X analog gain. 1x used <0.05 % of the ADC range on the bench;
    // 16x brings the matrix into a usable SNR region. Goes out in the
    // spectrum packets' gain field.
    static constexpr AS7265XGain SPEC_GAIN = AS7265XGain::GAIN_16X;
    // LP5810C channel mask: OUT0=R, OUT1=G, OUT2=B
    static constexpr uint8_t RGB_CHANNELS = 0x07U;
    // LP5810D channel masks: OUT0=white, OUT1=IR 940nm, OUT2=UV 400nm
    static constexpr uint8_t WHITE_CHANNEL = 0x01U;
    static constexpr uint8_t IR_CHANNEL = 0x02U;
    static constexpr uint8_t UV_CHANNEL = 0x04U;

    static constexpr uint32_t SPEC_BOOT_TIMEOUT_MS = 1500U;
    static constexpr uint32_t SPEC_BOOT_POLL_MS = 25U;

    /// Ship the AS7265XResult in `result` as an EXP1_SPECTRUM_A/B pair.
    /// matrix_idx = MATRIX row of the measurement (goes out as led_mask).
    void send_spectrum_pair(uint8_t matrix_idx, bool valid, uint16_t start_tick, uint32_t start_us);
    void sensor_init() noexcept;
    void try_spec_recovery();
    void wait_spec_boot() noexcept;
    void report_led_fault_once() noexcept;

    enum class RecoveryStep : uint8_t { IDLE, ASSERTING, WAITING };

    AS7265X spec{};
    LP5810 lp5810_rgb{};
    LP5810 lp5810_uv_ir{};

    // Spectrometer recovery: a full reset (I2C bus + AS7265X) is retried up
    // to SPEC_MAX_RECOVERIES times; after that the cycle is given up.
    static constexpr uint8_t SPEC_MAX_RECOVERIES = 3U;

    bool cycle_active = false;
    // One-shot guard: a runtime LP5810 latch is reported (error LED + gap
    // packet) exactly once; reset on re-init so a fresh latch reports again.
    bool led_fault_reported = false;
    uint8_t spec_recovery_attempts = 0U;
    RecoveryStep recovery_step = RecoveryStep::IDLE;
    uint8_t recovery_ticks = 0U;

    // Measurement-matrix runtime state.
    uint8_t step_idx = 0U;      // MATRIX row currently integrating
    uint8_t block_tick = 0U;    // tick within the current row's block
    uint8_t cur_step_idx = 0U;  // row of the in-flight measurement
    uint8_t prev_step_idx = 0U; // row whose data is being read out/sent
    bool cur_started = false;   // an in-flight measurement exists
    bool prev_ready = false;    // prev row awaits readout/send
    bool cur_valid = false;
    bool prev_valid = false;
    uint16_t cur_start_tick = 0U;
    uint32_t cur_start_us = 0U;
    uint16_t prev_start_tick = 0U;
    uint32_t prev_start_us = 0U;

    AS7265XResult result{};
};
