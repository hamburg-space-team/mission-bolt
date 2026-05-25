#include "exp1_computer.hpp"
#include "can_protocol.hpp"
#include "main.h"
#include "packet_payloads.hpp"

extern CAN_HandleTypeDef hcan1;

static Exp1Computer* instance_g = nullptr;

static bool spec_int_read() {
    return HAL_GPIO_ReadPin(SPEC_INT_GPIO_Port, SPEC_INT_Pin) == GPIO_PIN_RESET;
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

Exp1Computer::Exp1Computer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, can) {
    instance_g = this;
}

void Exp1Computer::sensor_init() noexcept {
    if (lp5810_rgb.init(&i2c, LP5810C_ADDR) < 0) {
        on_sensor_failed();
        return;
    }
    if (lp5810_uv_ir.init(&i2c, LP5810D_ADDR) < 0) {
        on_sensor_failed();
        return;
    }
    if (spec.init(&i2c, platform.tick_ms, AS7265X_ADDR, AS7265X_INT_25_CYCLES, AS7265XGain::GAIN_1X, spec_int_read) <
        0) {
        on_sensor_failed();
        return;
    }
    spec_reset_attempted = false;
    cycle_active = true;
    meas_phase = 0U;
}

void Exp1Computer::on_experiment_init() noexcept {
    HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_RESET);
    platform.delay_ms(2U);
    HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_SET);
    platform.delay_ms(100U);
    sensor_init();
}

void Exp1Computer::try_spec_recovery() {
    switch (recovery_step) {
    case RecoveryStep::IDLE:
        static_cast<void>(lp5810_rgb.disable_all());
        static_cast<void>(lp5810_uv_ir.disable_all());
        if (spec_reset_attempted) {
            cycle_active = false;
            return;
        }
        spec_reset_attempted = true;
        HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_RESET);
        recovery_step = RecoveryStep::ASSERTING;
        break;

    case RecoveryStep::ASSERTING:
        // RESET held for 1 tick
        HAL_GPIO_WritePin(SPEC_RESET_GPIO_Port, SPEC_RESET_Pin, GPIO_PIN_SET);
        recovery_ticks = 3U; // 3 × 40ms = 120ms >> 100ms boot time
        recovery_step = RecoveryStep::WAITING;
        break;

    case RecoveryStep::WAITING:
        if (--recovery_ticks > 0U) {
            break;
        }
        spec = AS7265X{};
        lp5810_rgb = LP5810{};
        lp5810_uv_ir = LP5810{};
        sensor_init();
        recovery_step = RecoveryStep::IDLE;
        break;
    }
}

void Exp1Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    if (!cycle_active) {
        return;
    }
    if (spec.is_failed()) {
        try_spec_recovery();
        return;
    }

    switch (meas_phase) {
    case 0: { // Verify LEDs off -> start dark measurement
        const bool rgb_off = (lp5810_rgb.disable_all() == 0);
        const bool ir_off = (lp5810_uv_ir.disable_all() == 0);
        start_tick = can_tick;
        start_timestamp_us = timestamp_us;
        const bool meas_ok = (spec.start_measurement() == 0);
        result_valid = rgb_off && ir_off && meas_ok;
        break;
    }

    case 2: { // Read dark -> send dark -> enable RGB -> start RGB measurement
        result_valid = result_valid && (spec.read_channels(&result) == 0);
        send_spectrum_pair(0);
        start_tick = can_tick;
        start_timestamp_us = timestamp_us;
        const bool led_ok = (lp5810_rgb.set_channels(RGB_CHANNELS) == 0);
        const bool meas_ok = (spec.start_measurement() == 0);
        result_valid = led_ok && meas_ok;
        break;
    }

    case 4: { // Read RGB -> send RGB -> disable RGB, enable white -> start white measurement
        result_valid = result_valid && (spec.read_channels(&result) == 0);
        send_spectrum_pair(1);
        start_tick = can_tick;
        start_timestamp_us = timestamp_us;
        const bool rgb_off = (lp5810_rgb.disable_all() == 0);
        const bool white_on = (lp5810_uv_ir.set_channels(WHITE_CHANNEL) == 0);
        const bool meas_ok = (spec.start_measurement() == 0);
        result_valid = rgb_off && white_on && meas_ok;
        break;
    }

    case 6: { // Read white -> send white -> switch to IR -> start IR measurement
        result_valid = result_valid && (spec.read_channels(&result) == 0);
        send_spectrum_pair(2);
        start_tick = can_tick;
        start_timestamp_us = timestamp_us;
        const bool ir_on = (lp5810_uv_ir.set_channels(IR_CHANNEL) == 0);
        const bool meas_ok = (spec.start_measurement() == 0);
        result_valid = ir_on && meas_ok;
        break;
    }

    case 8: { // Read IR -> send IR -> switch to UV -> start UV measurement
        result_valid = result_valid && (spec.read_channels(&result) == 0);
        send_spectrum_pair(3);
        start_tick = can_tick;
        start_timestamp_us = timestamp_us;
        const bool uv_on = (lp5810_uv_ir.set_channels(UV_CHANNEL) == 0);
        const bool meas_ok = (spec.start_measurement() == 0);
        result_valid = uv_on && meas_ok;
        break;
    }

    case 10: // Read UV -> send UV -> disable all LEDs
        result_valid = result_valid && (spec.read_channels(&result) == 0);
        send_spectrum_pair(4);
        static_cast<void>(lp5810_uv_ir.disable_all());
        break;

    default: // phases 1, 3, 5, 7, 9: wait for integration to complete
        break;
    }

    meas_phase = (meas_phase < PHASE_COUNT - 1U) ? static_cast<uint8_t>(meas_phase + 1U) : 0U;
}

void Exp1Computer::send_spectrum_pair(uint8_t idx) {
    using namespace PacketProtocol;

    static constexpr uint8_t LED_MASKS[MEAS_COUNT] = {LED_DARK, LED_RGB, LED_WHITE, LED_IR, LED_UV}; // NOLINT

    PayloadExp1SpectrumA spec_a{};
    PayloadExp1SpectrumB spec_b{};

    // Only copy channel data when valid; invalid packets send zeros so the ground station
    // doesn't misinterpret stale values from the previous cycle.
    if (result_valid) {
        for (uint8_t i = 0U; i < SPECTRUM_CHANNELS; i++) {
            spec_a.channels[i] = static_cast<uint32_t>(result.channels[i]);
            spec_b.channels[i] = static_cast<uint32_t>(result.channels[SPECTRUM_CHANNELS + i]);
        }
    }

    spec_a.integration_cycles = AS7265X_INT_25_CYCLES;
    spec_a.gain = static_cast<uint8_t>(AS7265XGain::GAIN_1X);
    spec_a.led_mask = LED_MASKS[idx];
    spec_a.measurement_valid = result_valid ? 1U : 0U;
    spec_b.start_timestamp_us = start_timestamp_us;

    uint8_t len = pkt.build(tx_buf.data(), PayloadType::EXP1_SPECTRUM_A, Tick{start_tick},
                            TimestampUs{start_timestamp_us}, &spec_a, static_cast<uint8_t>(sizeof(spec_a)));
    if (len > 0U) {
        can.send(exp_can_id(), tx_buf.data(), len);
        sd.write(tx_buf.data(), len);
    }

    len = pkt.build(tx_buf.data(), PayloadType::EXP1_SPECTRUM_B, Tick{start_tick}, TimestampUs{start_timestamp_us},
                    &spec_b, static_cast<uint8_t>(sizeof(spec_b)));
    if (len > 0U) {
        can.send(exp_can_id(), tx_buf.data(), len);
        sd.write(tx_buf.data(), len);
    }
}
