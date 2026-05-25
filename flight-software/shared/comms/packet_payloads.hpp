// THHOR-BOLT REXUS 37 -- Downlink Packet Payload Definitions

#pragma once

#include "packet_header.hpp"
#include "packet_types.hpp"

#include <cstdint>
#include <type_traits>

#define PACKET_TYPE_CHECK(T)                                                                                           \
    static_assert(std::is_trivially_copyable<T>::value);                                                               \
    static_assert(std::is_standard_layout<T>::value);                                                                  \
    static_assert(sizeof(T) <= MAX_PAYLOAD)

namespace PacketProtocol {

    // BTC_ENV -- sensors + IMU
    struct __attribute__((packed)) PayloadBtcEnv {
        // bit 0 = ms5611 valid
        // bit 1 = tmp117 valid
        // bit 2 = icm42686 valid
        uint8_t valid_mask;

        uint8_t reserved[1]; // NOLINT(modernize-avoid-c-arrays)

        // TMP117 -- raw register value, 1 LSB = 1/128 degC (16-bit signed)
        int16_t temp_raw;

        // MS5611 -- D1 (pressure) and D2 (temperature) raw values
        uint32_t ms_pressure;
        uint32_t ms_temperature;

        // ICM-42686-P accelerometer -- raw 16-bit register values
        int16_t accel_x_raw;
        int16_t accel_y_raw;
        int16_t accel_z_raw;

        // ICM-42686-P gyroscope -- raw 16-bit register values
        int16_t gyro_x_raw;
        int16_t gyro_y_raw;
        int16_t gyro_z_raw;
    };
    PACKET_TYPE_CHECK(PayloadBtcEnv);

    // BTC_STATUS -- system health, sent every 25 ticks (1 Hz)
    struct __attribute__((packed)) PayloadBtcStatus {
        // Seconds since MCU boot (not since LO)
        uint32_t uptime_s;

        // RTC seconds-since-midnight when LO was first detected.
        // 0 = LO not yet received.
        // Ground use: absolute_time_s = lo_rtc_s + tick * 0.04
        uint32_t lo_rtc_s;

        // SD card status:
        // bit 0 = mounted
        // bit 1 = failed (3+ consecutive errors, writes suppressed)
        uint8_t sd_status;

        // REXUS discrete signal states:
        // bit 0 = LO received (latched)
        // bit 1 = SOE currently active
        // bit 2 = SODS currently active
        uint8_t signal_mask;

        uint8_t reserved[2]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadBtcStatus);

    // EXP1_SPECTRUM_A -- AS7265X channels 1-9
    // Always paired with SPECTRUM_B (split because 18 channels exceed the 50 B limit).
    struct __attribute__((packed)) PayloadExp1SpectrumA {
        // AS7265X channels 1-9 (visible/UV range):
        // [0]=410nm  [1]=435nm  [2]=460nm  [3]=485nm  [4]=510nm
        // [5]=535nm  [6]=560nm  [7]=UV-A   [8]=UV-B
        uint32_t channels[SPECTRUM_CHANNELS]; // NOLINT

        // Number of integration cycles used (AS7265X register value).
        uint8_t integration_cycles;

        // Gain: 0=1x  1=3.7x  2=16x  3=64x
        uint8_t gain;

        // LED illumination condition during this measurement:
        // 0=dark  1=RGB  2=white  3=IR (940 nm)  4=UV (400 nm)
        uint8_t led_mask;

        // 1 if DATA_READY was set when result was collected; 0 = may be incomplete.
        uint8_t measurement_valid;
    };
    PACKET_TYPE_CHECK(PayloadExp1SpectrumA);

    // EXP1_SPECTRUM_B -- AS7265X channels 10-18, sent in the same tick as SPECTRUM_A.
    struct __attribute__((packed)) PayloadExp1SpectrumB {
        // AS7265X channels 10-18 (visible/NIR range):
        // [0]=585nm  [1]=610nm  [2]=645nm  [3]=680nm  [4]=705nm
        // [5]=730nm  [6]=760nm  [7]=810nm  [8]=NIR
        uint32_t channels[SPECTRUM_CHANNELS]; // NOLINT

        // Timestamp when the measurement was STARTED. Unit: us since LO.
        uint32_t start_timestamp_us;
    };
    PACKET_TYPE_CHECK(PayloadExp1SpectrumB);

    // EXP_ENV -- shared between EXP1 (0x22) and EXP2 (0x32).
    // Environmental context sampled alongside the controller's primary science.
    struct __attribute__((packed)) PayloadExpEnv {
        // bit 0 = ms5611 valid
        // bit 1 = tmp117 valid
        uint8_t valid_mask;

        uint8_t reserved[1]; // NOLINT(modernize-avoid-c-arrays)

        // TMP117 -- raw register value, 1 LSB = 1/128 degC (16-bit signed)
        int16_t temp_raw;

        // MS5611 -- D1 (pressure) and D2 (temperature) raw values
        uint32_t ms_pressure;
        uint32_t ms_temperature;
    };
    PACKET_TYPE_CHECK(PayloadExpEnv);

    // EXP_STATUS -- shared between EXP1 (0x23) and EXP2 (0x31).
    // Sent every 25 ticks (1 Hz).
    struct __attribute__((packed)) PayloadExpStatus {
        // Seconds since MCU boot.
        uint32_t uptime_s;

        // SD card status:
        // bit 0 = mounted
        // bit 1 = failed (3+ consecutive errors, writes suppressed)
        uint8_t sd_status;

        uint8_t reserved[3]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExpStatus);

    // EXP2_BER -- bit error rate measurement for one transmission round.
    // Core EXP2 science payload, sent every tick. EXP_ENV is sent in parallel.
    struct __attribute__((packed)) PayloadExp2Ber {
        // Index into the LiFi RATE_TABLE selected for this round.
        uint8_t rate_index;

        // Timestamp when SEND command went out -- us since LO.
        uint32_t timestamp_send_us;

        // Timestamp when the BUFR response was fully received -- us since LO.
        uint32_t timestamp_recv_us;

        // Bits transmitted in this round.
        uint16_t bits_sent;

        // Total bit errors detected (received bit != sent bit).
        uint16_t bit_errors;

        // Byte index of the first errored byte. 0xFF if no error.
        uint8_t first_error_byte;

        // Byte index of the last errored byte. 0xFF if no error.
        uint8_t last_error_byte;

        // 1 = measurement valid; 0 = invalid (other fields must not be interpreted).
        uint8_t measurement_valid;
    };
    PACKET_TYPE_CHECK(PayloadExp2Ber);

    // EXP3_STACK_A -- one sample from the WIRED stack (STM32U0 + Molex cable + LiFi A).
    // Ground reconstructs each burst from burst_index.
    struct __attribute__((packed)) PayloadExp3StackA {
        // MMC5983MA magnetometer -- raw 18-bit register values (sign-extended).
        int32_t mag_x_raw;
        int32_t mag_y_raw;
        int32_t mag_z_raw;

        // ICM-42688-P accelerometer -- raw 16-bit register values.
        int16_t accel_x_raw;
        int16_t accel_y_raw;
        int16_t accel_z_raw;

        // ICM-42688-P gyroscope -- raw 16-bit register values.
        int16_t gyro_x_raw;
        int16_t gyro_y_raw;
        int16_t gyro_z_raw;

        // TMP117 -- raw register value, 1 LSB = 1/128 degC (16-bit signed).
        int16_t tmp_raw;

        // Timestamp (LiFi-domain) when the wired-stack MCU latched this sample -- us since LO.
        uint32_t lifi_a_timestamp_us;

        // STM32-measured wakeup-to-response latency for this sample -- us.
        uint32_t latency_a_us;

        // Position of this sample inside the current burst.
        uint8_t burst_index;

        // bit 0 = mag_valid
        // bit 1 = imu_valid
        // bit 2 = tmp_valid
        // bit 3 = cable_check_ok (cable-LiFi A handshake passed)
        uint8_t valid_mask;

        uint8_t reserved[3]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExp3StackA);

    // EXP3_STACK_B -- one sample from the WIRELESS stack (STM32U0 + laser power + LiFi B).
    struct __attribute__((packed)) PayloadExp3StackB {
        // MMC5983MA magnetometer -- raw 18-bit register values.
        int32_t mag_x_raw;
        int32_t mag_y_raw;
        int32_t mag_z_raw;

        // ICM-42688-P accelerometer -- raw 16-bit register values.
        int16_t accel_x_raw;
        int16_t accel_y_raw;
        int16_t accel_z_raw;

        // ICM-42688-P gyroscope -- raw 16-bit register values.
        int16_t gyro_x_raw;
        int16_t gyro_y_raw;
        int16_t gyro_z_raw;

        // TMP117 -- raw register value, 1 LSB = 1/128 degC (16-bit signed).
        int16_t tmp_raw;

        // Wireless-stack storage capacitor voltage.
        uint16_t cap_voltage;

        // Timestamp (LiFi-domain) reported by the wireless-stack MCU at sample latch -- us since LO.
        uint32_t lifi_b_timestamp_us;

        // STM32-measured wakeup-to-response latency -- us.
        uint32_t latency_b_us;

        // Position of this sample inside the current burst.
        uint8_t burst_index;

        // bit 0 = mag_valid
        // bit 1 = imu_valid
        // bit 2 = tmp_valid
        // bit 3 = cap_valid
        uint8_t valid_mask;

        uint8_t reserved[2]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExp3StackB);

    // EXP3_ENV -- EXP3 controller environmental + IMU data, sent every tick.
    struct __attribute__((packed)) PayloadExp3Env {
        // TMP117 -- raw register value, 1 LSB = 1/128 degC (16-bit signed)
        int16_t tmp_raw;

        // MS5611 -- D1 (pressure) and D2 (temperature) raw values
        uint32_t ms_pressure;
        uint32_t ms_temperature;

        // ICM-42686-P accelerometer -- raw 16-bit register values
        int16_t imu_accel_x_raw;
        int16_t imu_accel_y_raw;
        int16_t imu_accel_z_raw;

        // ICM-42686-P gyroscope -- raw 16-bit register values
        int16_t imu_gyro_x_raw;
        int16_t imu_gyro_y_raw;
        int16_t imu_gyro_z_raw;
    };
    PACKET_TYPE_CHECK(PayloadExp3Env);

    // EXP3_STATUS -- EXP3 control-loop diagnostics, sent every 25 ticks (1 Hz).
    struct __attribute__((packed)) PayloadExp3Status {
        // Rolling-average latency estimate of the wired stack -- us.
        uint32_t latency_a_estimated_us;

        // Rolling-average latency estimate of the wireless stack -- us.
        uint32_t latency_b_estimated_us;

        // Sync delay actually applied at the start of this cycle -- us.
        uint32_t wait_a_used_us;

        // SD card status:
        // bit 0 = mounted
        // bit 1 = failed (3+ consecutive errors, writes suppressed)
        uint8_t sd_status;
    };
    PACKET_TYPE_CHECK(PayloadExp3Status);

    // ---------------------------------------------------------------------------
    // System payloads
    // ---------------------------------------------------------------------------

    // GAP_MARKER -- explicit gap notification, emitted by the BTC.
    struct __attribute__((packed)) PayloadGapMarker {
        // First missing tick number.
        uint16_t first_missing_tick;

        // Number of consecutive missing ticks.
        uint8_t count;

        // Reason for the gap - see GapReason in packet_types.hpp
        GapReason reason;
    };
    PACKET_TYPE_CHECK(PayloadGapMarker);

    // BOOT -- MCU startup notification.
    // Sent at power-on before LO. tick=0 and timestamp_us=0 in header.
    // If received during flight: watchdog or soft reset recovered.
    struct __attribute__((packed)) PayloadBoot {
        BootReason reason;

        uint8_t reserved[1]; // NOLINT(modernize-avoid-c-arrays)

        // Total boots since first power-on. 0 = first ever boot.
        uint16_t reboot_count;
    };
    PACKET_TYPE_CHECK(PayloadBoot);

} // namespace PacketProtocol
