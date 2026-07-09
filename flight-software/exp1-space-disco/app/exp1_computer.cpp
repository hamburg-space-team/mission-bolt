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
        uint8_t integration_cycles; // one-shot (mode 3 = mode 2 timing) converts
                                    // BOTH channel banks: real time = 2 x IT x 2.8 ms
        uint8_t block_ticks;        // see IT_CONFIGS: readout first, then 2*IT*2.8ms integration
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
    // Block layout (see on_experiment_tick): tick 0 reads the completed
    // row's dies 0+1, tick 1 reads die 2 + sends, THEN starts the next
    // measurement (~tick 1 + 20 ms). The RAW registers are LIVE on this
    // chip - starting a measurement resets them - so ALL reading must
    // happen before the next start (seen on hardware: start-then-read
    // returned ~zero counts of the fresh integration). Block length: the
    // integration (2*IT*2.8 ms, starts ~60 ms into the block) must be
    // complete before the NEXT block's tick-0 readout:
    //   IT25: 60 + 140 = 200 ms -> 6 ticks (240 ms)
    //   IT50: 60 + 280 = 340 ms -> 10 ticks (400 ms)
    constexpr std::array<ItCfg, 2> IT_CONFIGS = {{{25U, 6U}, {50U, 10U}}};

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
    if (auto r = lp5810_rgb.init(&i2c, LP5810C_ADDR, LP5810_DOT_CURRENT, platform.delay_ms, LP5810_HIGH_CURRENT); !r) {
        report_fault(StatusLeds::Fault::LED_RGB, r.error());
    }
    if (auto r = lp5810_uv_ir.init(&i2c, LP5810D_ADDR, LP5810_DOT_CURRENT, platform.delay_ms, LP5810_HIGH_CURRENT);
        !r) {
        report_fault(StatusLeds::Fault::LED_UVIR, r.error());
    }

    // The spectrometer IS the experiment. On success, clear the recovery
    // counter so a future failure gets a fresh set of retries. On failure,
    // leave the counter intact: cycle_active stays true so the recovery state
    // machine engages on the next tick (spec.is_failed()) and keeps retrying
    // until SPEC_MAX_RECOVERIES is exhausted.
    if (auto r = spec.init(&i2c, platform.tick_ms, AS7265X_ADDR, AS7265X_INT_25_CYCLES, SPEC_GAIN, spec_int_read,
                           platform.delay_ms);
        !r) {
        report_fault(StatusLeds::Fault::SPEC, r.error());
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
    wait_spec_boot();
    sensor_init();
}

// Poll until the AS72651 actually SERVES the virtual-register protocol.
void Exp1Computer::wait_spec_boot() noexcept {
    const uint32_t deadline = platform.tick_ms() + SPEC_BOOT_TIMEOUT_MS;
    while (platform.tick_ms() < deadline) {
        platform.kick_wdg();
        if (spec.probe(&i2c, platform.tick_ms, platform.delay_ms)) {
            return;
        }
        platform.delay_ms(SPEC_BOOT_POLL_MS);
    }
    // Timed out: fall through to sensor_init(), which reports the fault
    // with the full trace.
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

void Exp1Computer::fill_status(PacketProtocol::PayloadExpStatus& status) noexcept {
    status.led_write_fails = led_write_fails;
    status.spec_start_fails = spec_start_fails;
    status.data_ready_fails = data_ready_fails;
}

void Exp1Computer::report_led_fault_once() noexcept {
    if (led_fault_reported) {
        return;
    }

    if (lp5810_rgb.is_failed()) {
        led_fault_reported = true;
        report_fault(StatusLeds::Fault::LED_RGB, lp5810_rgb.last_error());
    }

    if (lp5810_uv_ir.is_failed()) {
        led_fault_reported = true;
        report_fault(StatusLeds::Fault::LED_UVIR, lp5810_uv_ir.last_error());
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
        // The measurement started last block has completed (the block length
        // guarantees it) - promote it and read dies 0+1 while the chip is
        // idle. The RAW registers are LIVE: they reset the moment the next
        // measurement starts, so the whole readout happens BEFORE the next
        // start (tick 1).
        prev_ready = cur_started;
        prev_valid = cur_valid;
        prev_step_idx = cur_step_idx;
        prev_start_tick = cur_start_tick;
        prev_start_us = cur_start_us;

        if (prev_ready) {
            // DATA_RDY (hardware INT pin, SPEC_INT) must be asserted before
            // touching the RAW registers: the block length guarantees the
            // timing, but a mid-run chip reboot would otherwise deliver a
            // phantom readout with measurement_valid = 1.
            const bool rdy = spec.data_ready();
            if (!rdy && data_ready_fails != 0xFFU) {
                data_ready_fails++;
            }
            const bool ok = spec.read_channels_dies(&result, 0U, 2U).has_value();
            prev_valid = prev_valid && rdy && ok;
        }
    } else if (block_tick == 1U) {
        // Rest of the readout (die 2), ship the pair - only THEN touch the
        // chip again for the next row.
        if (prev_ready) {
            const bool ok = spec.read_channels_dies(&result, 2U, 1U).has_value();
            prev_valid = prev_valid && ok;

            send_spectrum_pair(prev_step_idx, prev_valid, prev_start_tick, prev_start_us);
            prev_ready = false;
        }

        // LEDs for the new row: PWM brightness + enable mask on both chips
        // (dark rows carry mask 0 on both), then integration time (no-op
        // unless the row changes it) and the one-shot start. Destroys the
        // RAW window - all reads for this row happen next block.
        bool led_ok = lp5810_rgb.set_channels(step.rgb_mask, step.pwm).has_value();
        led_ok = lp5810_uv_ir.set_channels(step.uvir_mask, step.pwm).has_value() && led_ok;
        const bool it_ok = spec.set_integration(step.integration_cycles).has_value();
        const bool start_ok = spec.start_measurement().has_value();

        // Transient diagnostics: count which link failed (EXP1_STATUS).
        if (!led_ok && led_write_fails != 0xFFU) {
            led_write_fails++;
        }
        if ((!it_ok || !start_ok) && spec_start_fails != 0xFFU) {
            spec_start_fails++;
        }

        cur_started = true;
        cur_valid = led_ok && it_ok && start_ok;
        cur_step_idx = step_idx;
        cur_start_tick = can_tick;
        cur_start_us = timestamp_us;
    }
    // block_tick >= 2: the integration runs; the LEDs stay on for the row.

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
    spec_a.gain = static_cast<uint8_t>(SPEC_GAIN);
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
