#include "exp1_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "wcet.hpp"
#include <bolt/wire/payloads.hpp>

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

    // {integration_cycles, block_ticks}. One-shot mode 3 integrates both banks:
    // real time = 2 * cycles * 2.8 ms. The measurement starts at block_tick 1
    // and is read at block_tick 0 of the NEXT block, so it must finish within
    // (block_ticks - 1) * 40 ms:
    //   block_ticks >= ceil(2*cycles*2.8 / 40) + 1 + ~2-3 ticks jitter margin.
    //   IT80 : 448 ms -> 12 + 1 = 13, use 15 (~2.8 ticks margin)
    //   IT160: 896 ms -> 23 + 1 = 24, use 26 (~2.6 ticks margin)
    // Paired with SPEC_GAIN = 3.7x: bright rows stay off the 16-bit ceiling
    // while the long integration lifts SNR on the faint UV/NIR + dark rows.
    // Cost is measurement rate (rows now 0.6 s / 1.04 s); downlink is unaffected
    // (fewer, larger-spaced packets). Raise cycles further for still-cleaner
    // dark/faint data if the slower matrix cycle is acceptable.
    constexpr std::array<ItCfg, 2> IT_CONFIGS = {{{80U, 15U}, {160U, 26U}}};

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
        // data[2] = mission mode, data[3] = self-test target; the DLC check
        // above guarantees both are present
        const auto mode = (data[2] == static_cast<uint8_t>(BootState::Mode::FLIGHT)) ? BootState::Mode::FLIGHT
                                                                                     : BootState::Mode::TEST;
        instance_g->notify_sync(tick, mode, data[3]);
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

void Exp1Computer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;
    PayloadExpStatus status{};
    status.uptime_s = platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(storage.is_mounted() ? 0x01U : 0x00U);
    status.led_write_fails = led_write_fails;
    status.spec_start_fails = spec_start_fails;
    status.data_ready_fails = data_ready_fails;

    if (auto len = pkt.build(tx_buf.data(), exp_status_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &status,
                             static_cast<uint8_t>(sizeof(status)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

void Exp1Computer::report_experiment_faults() noexcept {
    poll_device_fault(spec, StatusLeds::Fault::SPEC);
}

void Exp1Computer::retry_led(LP5810& led, uint8_t addr, StatusLeds::Fault code) {
    if (!led.retry_due()) {
        return;
    }
    platform.kick_wdg();

    LP5810 fresh;
    if (fresh.init(&i2c, addr, LP5810_DOT_CURRENT, platform.delay_ms, LP5810_HIGH_CURRENT)) {
        led = fresh;
    } else {
        led.arm_retry();
        report_fault(code, led.last_error());
    }
}

void Exp1Computer::retry_extra_devices() {
    retry_led(lp5810_rgb, LP5810C_ADDR, StatusLeds::Fault::LED_RGB);
    retry_led(lp5810_uv_ir, LP5810D_ADDR, StatusLeds::Fault::LED_UVIR);
}

void Exp1Computer::on_experiment_reset() noexcept {
    // LEDs off, matrix back to row 0 with an empty pipeline; an in-flight
    // integration finishes into nowhere and the next start overwrites it
    (void)lp5810_rgb.disable_all();
    (void)lp5810_uv_ir.disable_all();
    step_idx = 0U;
    block_tick = 0U;
    cur_started = false;
    cur_valid = false;
    prev_ready = false;
    prev_valid = false;
    // recovery state stays - device health is orthogonal to experiment state
}

void Exp1Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    report_experiment_faults();
    if (!cycle_active) {
        return;
    }
    if (spec.is_failed()) {
        try_spec_recovery();
        return;
    }

    if (block_tick == 0U) {
        latch_prev_and_read_first();
    } else if (block_tick == 1U) {
        finish_prev_read_and_send();
        start_row(can_tick, timestamp_us);
    }
    // block_tick >= 2: the integration runs; the LEDs stay on for the row.

    block_tick++;
    if (block_tick >= MATRIX[step_idx].block_ticks) {
        block_tick = 0U;
        step_idx = static_cast<uint8_t>((step_idx + 1U) % MATRIX_SIZE);
    }
}

void Exp1Computer::latch_prev_and_read_first() {
    prev_ready = cur_started;
    prev_valid = cur_valid;
    prev_step_idx = cur_step_idx;
    prev_start_tick = cur_start_tick;
    prev_start_us = cur_start_us;

    if (prev_ready) {
        const bool rdy = spec.data_ready();
        if (!rdy && data_ready_fails != 0xFFU) {
            data_ready_fails++;
        }
        bool ok = false;
        {
            BOLT_TIME(timings, READ);
            ok = spec.read_channels_dies(&result, 0U, 2U).has_value();
        }
        prev_valid = prev_valid && rdy && ok;
    }
}

void Exp1Computer::finish_prev_read_and_send() {
    if (!prev_ready) {
        return;
    }
    bool ok = false;
    {
        BOLT_TIME(timings, READ);
        ok = spec.read_channels_dies(&result, 2U, 1U).has_value();
    }
    prev_valid = prev_valid && ok;

    {
        BOLT_TIME(timings, SEND);
        send_spectrum(prev_step_idx, prev_valid, prev_start_tick, prev_start_us);
    }
    prev_ready = false;
}

void Exp1Computer::start_row(uint16_t can_tick, uint32_t timestamp_us) {
    const MeasStep& step = MATRIX[step_idx];

    bool led_ok = false;
    {
        BOLT_TIME(timings, DRIVE);
        led_ok = lp5810_rgb.set_channels(step.rgb_mask, step.pwm).has_value();
        led_ok = lp5810_uv_ir.set_channels(step.uvir_mask, step.pwm).has_value() && led_ok;
    }
    bool it_ok = false;
    bool start_ok = false;
    {
        BOLT_TIME(timings, CFG);
        it_ok = spec.set_integration(step.integration_cycles).has_value();
        start_ok = spec.start_measurement().has_value();
    }

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

void Exp1Computer::send_spectrum(uint8_t matrix_idx, bool valid, uint16_t start_tick, uint32_t start_us) {
    using namespace PacketProtocol;

    PayloadExp1Spectrum spec{};

    // Only copy channel data when valid; invalid packets send zeros so the ground station
    // doesn't misinterpret stale values from the previous row.
    if (valid) {
        for (uint8_t i = 0U; i < SPECTRUM_CHANNELS; i++) {
            spec.channels[i] = result.channels[i]; // already 16-bit raw counts
        }
    }

    spec.start_timestamp_us = start_us;
    spec.integration_cycles = MATRIX[matrix_idx].integration_cycles;
    spec.gain = static_cast<uint8_t>(SPEC_GAIN);
    spec.led_mask = matrix_idx;
    spec.measurement_valid = valid ? 1U : 0U;

    if (auto len = pkt.build(tx_buf.data(), PayloadType::EXP1_SPECTRUM, Tick{start_tick}, TimestampUs{start_us}, &spec,
                             static_cast<uint8_t>(sizeof(spec)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}

// --- Self-test --------------------------------------------------------------

void Exp1Computer::on_self_test_abort() noexcept {
    // the interrupted step may have left LEDs on - park them
    (void)lp5810_rgb.disable_all();
    (void)lp5810_uv_ir.disable_all();
}

std::optional<PacketProtocol::TestResult> Exp1Computer::spec_test_measure(bool first, uint8_t rgb_mask,
                                                                          uint8_t uvir_mask, uint32_t& sum_out) {
    if (first) {
        spec_test_phase = SpecTestPhase::CONFIGURE;
    }
    if (spec_test_phase == SpecTestPhase::CONFIGURE) {
        return spec_test_configure(rgb_mask, uvir_mask);
    }
    return spec_test_collect(sum_out);
}

std::optional<PacketProtocol::TestResult> Exp1Computer::spec_test_configure(uint8_t rgb_mask, uint8_t uvir_mask) {
    using PacketProtocol::TestResult;

    // only meaningful if we control the light - latched driver or
    // spectrometer voids the premise
    if (spec.is_failed() || lp5810_rgb.is_failed() || lp5810_uv_ir.is_failed()) {
        return TestResult::SKIPPED;
    }
    const bool rgb_ok = (rgb_mask != 0U) ? lp5810_rgb.set_channels(rgb_mask, SELF_TEST_PWM).has_value()
                                         : lp5810_rgb.disable_all().has_value();
    const bool uvir_ok = (uvir_mask != 0U) ? lp5810_uv_ir.set_channels(uvir_mask, SELF_TEST_PWM).has_value()
                                           : lp5810_uv_ir.disable_all().has_value();
    if (!rgb_ok || !uvir_ok) {
        return TestResult::FAIL;
    }
    if (!spec.set_integration(AS7265X_INT_25_CYCLES) || !spec.start_measurement()) {
        return TestResult::FAIL;
    }
    spec_test_phase = SpecTestPhase::WAIT;
    return std::nullopt;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::spec_test_collect(uint32_t& sum_out) {
    using PacketProtocol::TestResult;

    // integration runs; the Runner's step cap bounds a lost DATA_RDY
    if (!spec.data_ready()) {
        return std::nullopt;
    }
    // all 18 channels in one go - unlike the flight matrix the test
    // tick has the budget for it
    if (!spec.read_channels(&result)) {
        return TestResult::FAIL;
    }
    uint32_t sum = 0U;
    for (const uint16_t ch : result.channels) {
        sum += ch;
    }
    sum_out = sum;
    return TestResult::PASS;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::step_led_rgb(NodeComputer& node, bool /*first*/,
                                                                     uint32_t& data) noexcept {
    using PacketProtocol::TestResult;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp1Computer&>(node);
    if (self.lp5810_rgb.is_failed()) {
        return TestResult::SKIPPED;
    }
    // electrical check only - whether light comes out is the lit steps' call
    const bool ok = self.lp5810_rgb.set_channels(RGB_CHANNELS, SELF_TEST_PWM).has_value() &&
                    self.lp5810_rgb.disable_all().has_value();
    data = RGB_CHANNELS;
    return ok ? TestResult::PASS : TestResult::FAIL;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::step_led_uvir(NodeComputer& node, bool /*first*/,
                                                                      uint32_t& data) noexcept {
    using PacketProtocol::TestResult;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp1Computer&>(node);
    if (self.lp5810_uv_ir.is_failed()) {
        return TestResult::SKIPPED;
    }
    const bool ok = self.lp5810_uv_ir.set_channels(SELF_TEST_UVIR_CHANNELS, SELF_TEST_PWM).has_value() &&
                    self.lp5810_uv_ir.disable_all().has_value();
    data = SELF_TEST_UVIR_CHANNELS;
    return ok ? TestResult::PASS : TestResult::FAIL;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::step_spec_dark(NodeComputer& node, bool first,
                                                                       uint32_t& data) noexcept {
    using PacketProtocol::TestResult;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp1Computer&>(node);
    uint32_t sum = 0U;
    const auto verdict = self.spec_test_measure(first, 0U, 0U, sum);
    if (verdict == TestResult::PASS) {
        // reference point for the lit steps
        self.spec_test_dark_sum = sum;
        data = sum;
    }
    return verdict;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::step_spec_lit_rgb(NodeComputer& node, bool first,
                                                                          uint32_t& data) noexcept {
    using PacketProtocol::TestResult;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp1Computer&>(node);
    const auto verdict = self.spec_test_measure(first, RGB_CHANNELS, 0U, data);
    if (!verdict) {
        return std::nullopt;
    }
    (void)self.lp5810_rgb.disable_all();
    if (*verdict != TestResult::PASS) {
        return verdict;
    }
    // lit must read brighter than dark, or the optical path is dead
    return (data > self.spec_test_dark_sum) ? TestResult::PASS : TestResult::FAIL;
}

std::optional<PacketProtocol::TestResult> Exp1Computer::step_spec_lit_uvir(NodeComputer& node, bool first,
                                                                           uint32_t& data) noexcept {
    using PacketProtocol::TestResult;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp1Computer&>(node);
    const auto verdict = self.spec_test_measure(first, 0U, SELF_TEST_UVIR_CHANNELS, data);
    if (!verdict) {
        return std::nullopt;
    }
    // last step: leave the payload dark, whatever the verdict
    (void)self.lp5810_rgb.disable_all();
    (void)self.lp5810_uv_ir.disable_all();
    if (*verdict != TestResult::PASS) {
        return verdict;
    }
    return (data > self.spec_test_dark_sum) ? TestResult::PASS : TestResult::FAIL;
}

std::span<const SelfTest::Step> Exp1Computer::self_test_steps() const noexcept {
    static constexpr std::array<SelfTest::Step, 8U> steps = {{
        {&NodeComputer::step_tmp_whoami},    // 0: TMP117 device ID
        {&NodeComputer::step_tmp_read},      // 1: TMP117 raw temperature
        {&NodeComputer::step_baro_prom},     // 2: MS5611 PROM CRC + C1
        {&Exp1Computer::step_led_rgb},       // 3: LP5810C write path (RGB)
        {&Exp1Computer::step_led_uvir},      // 4: LP5810D write path (white/IR/UV)
        {&Exp1Computer::step_spec_dark},     // 5: spectrum, all LEDs off (reference)
        {&Exp1Computer::step_spec_lit_rgb},  // 6: spectrum, RGB lit vs. dark
        {&Exp1Computer::step_spec_lit_uvir}, // 7: spectrum, white/IR/UV lit vs. dark
    }};
    return steps;
}
