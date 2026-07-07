#include "exp1_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "packet_payloads.hpp"

#include <array>

extern CAN_HandleTypeDef hcan1;

namespace {
    Exp1Computer* instance_g = nullptr;

    // Measurement matrix: every row = one spectrometer measurement with a
    // fixed LED configuration, PWM brightness and integration time. led_mask
    // in the spectrum packets is the ROW INDEX into this table; the ground
    // station decodes it with the same table (ICD-007).
    struct MeasStep {
        uint8_t rgb_mask;           // LP5810C: OUT0=R, OUT1=G, OUT2=B
        uint8_t uvir_mask;          // LP5810D: OUT0=white, OUT1=IR, OUT2=UV
        uint8_t pwm;                // Manual_PWM duty for all channels
        uint8_t integration_cycles; // x 2.8 ms
        uint8_t block_ticks;        // smallest n with n*40ms >= IT*2.8ms + ~10ms setup
    };
    struct LedCfg {
        uint8_t rgb;
        uint8_t uvir;
    };
    struct ItCfg {
        uint8_t cycles;
        uint8_t block_ticks;
    };
    // Each LED alone, then all six together; one dark reference per IT group.
    constexpr std::array<LedCfg, 7> LED_CONFIGS = {{
        {0x01U, 0x00U}, // R
        {0x02U, 0x00U}, // G
        {0x04U, 0x00U}, // B
        {0x00U, 0x01U}, // white
        {0x00U, 0x02U}, // IR 940 nm
        {0x00U, 0x04U}, // UV 400 nm
        {0x07U, 0x07U}, // all LEDs
    }};
    constexpr std::array<uint8_t, 4> PWM_LEVELS = {0x40U, 0x80U, 0xC0U, 0xFFU}; // 25/50/75/100 %
    // Pipeline constraint: cycles >= 25. The previous row's second read half
    // ends <= ~64 ms after the in-flight measurement started; the integration
    // must not complete (and overwrite the RAW registers) before that.
    constexpr std::array<ItCfg, 2> IT_CONFIGS = {{{25U, 2U}, {50U, 4U}}};

    constexpr uint8_t MATRIX_SIZE =
        static_cast<uint8_t>(IT_CONFIGS.size() * (1U + (LED_CONFIGS.size() * PWM_LEVELS.size())));

    consteval std::array<MeasStep, MATRIX_SIZE> build_matrix() {
        std::array<MeasStep, MATRIX_SIZE> matrix{};
        std::size_t row = 0U;
        for (const ItCfg& it : IT_CONFIGS) {
            matrix[row] = MeasStep{0U, 0U, 0U, it.cycles, it.block_ticks}; // dark reference
            row++;
            for (const LedCfg& led : LED_CONFIGS) {
                for (const uint8_t pwm : PWM_LEVELS) {
                    matrix[row] = MeasStep{led.rgb, led.uvir, pwm, it.cycles, it.block_ticks};
                    row++;
                }
            }
        }
        return matrix;
    }
    constexpr auto MATRIX = build_matrix();

    static bool spec_int_read() {
        return HAL_GPIO_ReadPin(SPEC_INT_GPIO_Port, SPEC_INT_Pin) == GPIO_PIN_RESET;
    }
} // namespace

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

Exp1Computer::Exp1Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, storage, can) {
    instance_g = this;
}

void Exp1Computer::sensor_init() noexcept {
    // Re-entered from try_spec_recovery() mid-tick: the LP5810 re-inits
    // include reset boot delays and, on a sick bus, a series of I2C
    // timeouts - kick the watchdog so a slow pass cannot trip it.
    platform.kick_wdg();

    // LEDs are best-effort illumination: a failure latches that driver (and
    // lights the error LED) but must not stop the spectrometer cycle.
    if (!lp5810_rgb.init(&i2c, LP5810C_ADDR, LP5810_DOT_CURRENT, platform.delay_ms)) {
        on_sensor_failed(StatusLeds::Fault::LED_RGB);
    }
    if (!lp5810_uv_ir.init(&i2c, LP5810D_ADDR, LP5810_DOT_CURRENT, platform.delay_ms)) {
        on_sensor_failed(StatusLeds::Fault::LED_UVIR);
    }

    // The spectrometer IS the experiment. On success, clear the recovery
    // counter so a future failure gets a fresh set of retries. On failure,
    // leave the counter intact: cycle_active stays true so the recovery state
    // machine engages on the next tick (spec.is_failed()) and keeps retrying
    // until SPEC_MAX_RECOVERIES is exhausted.
    if (!spec.init(&i2c, platform.tick_ms, AS7265X_ADDR, AS7265X_INT_25_CYCLES, AS7265XGain::GAIN_1X, spec_int_read)) {
        on_sensor_failed(StatusLeds::Fault::SPEC);
    } else {
        spec_recovery_attempts = 0U;
    }

    cycle_active = true;
    // Restart the matrix from row 0 with an empty pipeline: after a
    // (re-)init there is no in-flight measurement and no readable result.
    step_idx = 0U;
    block_tick = 0U;
    cur_started = false;
    prev_ready = false;
    led_fault_reported = false;
}

void Exp1Computer::on_experiment_init() noexcept {
    platform.kick_wdg();
    HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_RESET);
    platform.delay_ms(2U);
    HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_SET);
    sensor_init();
}

void Exp1Computer::try_spec_recovery() {
    switch (recovery_step) {
    case RecoveryStep::IDLE:
        (void)lp5810_rgb.disable_all();
        (void)lp5810_uv_ir.disable_all();
        if (spec_recovery_attempts >= SPEC_MAX_RECOVERIES) {
            // Retries exhausted: give up and stop the cycle for good.
            cycle_active = false;
            return;
        }
        spec_recovery_attempts++;
        HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_RESET);
        recovery_step = RecoveryStep::ASSERTING;
        break;

    case RecoveryStep::ASSERTING:
        (void)i2c.reset();
        HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_SET);

        // AS72651 reloads its firmware from SPI flash after reset and only
        // ACKs on I2C once that finishes -- worst case ~1s, so wait 30 ticks
        // (1.2s) before re-initialising.
        recovery_ticks = 30U;
        recovery_step = RecoveryStep::WAITING;
        break;

    case RecoveryStep::WAITING:
        if (--recovery_ticks > 0U) {
            break;
        }
        // Spec has booted on a clean bus -- re-initialise all sensors.
        spec = AS7265X{};
        lp5810_rgb = LP5810{};
        lp5810_uv_ir = LP5810{};
        sensor_init();
        recovery_step = RecoveryStep::IDLE;
        break;
    }
}

void Exp1Computer::report_led_fault_once() noexcept {
    if (led_fault_reported) {
        return;
    }

    if (lp5810_rgb.is_failed()) {
        led_fault_reported = true;
        on_sensor_failed(StatusLeds::Fault::LED_RGB);
    }

    if (lp5810_uv_ir.is_failed()) {
        led_fault_reported = true;
        on_sensor_failed(StatusLeds::Fault::LED_UVIR);
    }
}

void Exp1Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    report_led_fault_once();
    if (!cycle_active) {
        return;
    }
    if (spec.is_failed()) {
        try_spec_recovery();
        return;
    }

    const MeasStep& step = MATRIX[step_idx];

    if (block_tick == 0U) {
        // Setup tick. The measurement that integrated during the previous
        // block becomes "prev" and is read out during THIS block while the
        // new one integrates (pipelined readout).
        prev_ready = cur_started;
        prev_valid = cur_valid;
        prev_step_idx = cur_step_idx;
        prev_start_tick = cur_start_tick;
        prev_start_us = cur_start_us;

        // LEDs for the new row: PWM brightness + enable mask on both chips
        // (dark rows carry mask 0 on both).
        bool led_ok = lp5810_rgb.set_channels(step.rgb_mask, step.pwm).has_value();
        led_ok = lp5810_uv_ir.set_channels(step.uvir_mask, step.pwm).has_value() && led_ok;

        // Integration time (no-op unless the row changes it) + start, both
        // EARLY in the tick: the integration must be complete when this
        // row's own readout begins block_ticks later (IT25: ~72 ms vs 80 ms).
        const bool it_ok = spec.set_integration(step.integration_cycles).has_value();
        const bool start_ok = spec.start_measurement().has_value();

        cur_started = true;
        cur_valid = led_ok && it_ok && start_ok;
        cur_step_idx = step_idx;
        cur_start_tick = can_tick;
        cur_start_us = timestamp_us;

        // First readout half of the previous row (dies 0+1, ~23 ms). Runs
        // while the new measurement integrates: the RAW registers keep the
        // previous result until the in-flight integration completes.
        if (prev_ready) {
            const bool ok = spec.read_channels_dies(&result, 0U, 2U).has_value();
            prev_valid = prev_valid && ok;
        }
    } else if (block_tick == 1U) {
        // Second half (die 2, ~12 ms), then ship the pair. Must finish
        // before the in-flight integration completes - guaranteed by the
        // IT >= 25 constraint on the matrix (see IT_CONFIGS).
        if (prev_ready) {
            const bool ok = spec.read_channels_dies(&result, 2U, 1U).has_value();
            prev_valid = prev_valid && ok;

            send_spectrum_pair(prev_step_idx, prev_valid, prev_start_tick, prev_start_us);
            prev_ready = false;
        }
    }
    // block_tick >= 2: wait while long-integration rows finish integrating.

    block_tick++;
    if (block_tick >= step.block_ticks) {
        block_tick = 0U;
        step_idx = static_cast<uint8_t>((step_idx + 1U) % MATRIX_SIZE);
    }
}

void Exp1Computer::send_spectrum_pair(uint8_t matrix_idx, bool valid, uint16_t start_tick, uint32_t start_us) {
    using namespace PacketProtocol;

    PayloadExp1SpectrumA spec_a{};
    PayloadExp1SpectrumB spec_b{};

    // Only copy channel data when valid; invalid packets send zeros so the ground station
    // doesn't misinterpret stale values from the previous row.
    if (valid) {
        for (uint8_t i = 0U; i < SPECTRUM_CHANNELS; i++) {
            spec_a.channels[i] = static_cast<uint32_t>(result.channels[i]);
            spec_b.channels[i] = static_cast<uint32_t>(result.channels[SPECTRUM_CHANNELS + i]);
        }
    }

    spec_a.integration_cycles = MATRIX[matrix_idx].integration_cycles;
    spec_a.gain = static_cast<uint8_t>(AS7265XGain::GAIN_1X);
    // Matrix ROW INDEX, not a bitmask: identifies LED config, brightness and
    // integration setting via the MATRIX table (ICD-007 decodes with the
    // same table).
    spec_a.led_mask = matrix_idx;
    spec_a.measurement_valid = valid ? 1U : 0U;
    spec_b.start_timestamp_us = start_us;

    if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP1_SPECTRUM_A, Tick{start_tick}, TimestampUs{start_us},
                             &spec_a, static_cast<uint8_t>(sizeof(spec_a)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }

    if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP1_SPECTRUM_B, Tick{start_tick}, TimestampUs{start_us},
                             &spec_b, static_cast<uint8_t>(sizeof(spec_b)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}
