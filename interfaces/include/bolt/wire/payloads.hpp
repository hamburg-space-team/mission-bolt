/// THHOR-BOLT REXUS 37 - Downlink packet payload definitions
///
/// @ingroup comms

#pragma once

#include <bolt/wire/annotations.hpp>
#include <bolt/wire/header.hpp>
#include <bolt/wire/types.hpp>

#include <cstdint>
#include <type_traits>

#define PACKET_TYPE_CHECK(T)                                                                                           \
    static_assert(std::is_trivially_copyable<T>::value);                                                               \
    static_assert(std::is_standard_layout<T>::value);                                                                  \
    static_assert(sizeof(T) <= MAX_PAYLOAD)

namespace PacketProtocol {

    /// BTC_ENV - common sensors. Sent every tick (25 Hz). Raw register
    /// values; calibration happens on ground (ADR-009).
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BTC_ENV, .node = "BTC", .rate_hz = 25,
                                          .desc = "common env sensors") PayloadBtcEnv {
        WIRE(.desc = "valid bits: 0=ms5611, 1=tmp117")
        uint8_t valid_mask;

        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:1", .desc = "TMP117 board temperature")
        int16_t temp_raw;

        WIRE(.unit = "raw", .gate = "valid_mask:0", .desc = "MS5611 D1 pressure ADC (compensate with D2 + PROM coeffs)")
        uint32_t ms_pressure_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0",
             .desc = "MS5611 D2 temperature ADC (drives dT/TEMP for pressure comp)")
        uint32_t ms_temperature_raw;
    };
    PACKET_TYPE_CHECK(PayloadBtcEnv);

    /// BTC_STATUS - system health, sent every 25 ticks (1 Hz).
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BTC_STATUS, .node = "BTC", .rate_hz = 1,
                                          .desc = "system health") PayloadBtcStatus {
        WIRE(.unit = "s", .desc = "uptime since MCU boot")
        uint32_t uptime_s;

        WIRE(.unit = "s", .desc = "RTC s-since-midnight at LO (0=none); abs_time = lo_rtc_s + tick*0.04")
        uint32_t lo_rtc_s;

        WIRE(.desc = "SD: 0=mounted, 1=failed")
        uint8_t sd_status;

        WIRE(.desc = "REXUS: 0=LO latched, 1=SOE, 2=SODS")
        uint8_t signal_mask;

        WIRE(.desc = "MissionMode: TEST / FLIGHT")
        MissionMode mode;
    };
    PACKET_TYPE_CHECK(PayloadBtcStatus);

    /// BTC_IMU - one full ICM42686 sample per data-ready interrupt
    /// (200 Hz). +-32 g / +-2000 dps full scale over 16 bit.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BTC_IMU, .node = "BTC", .rate_hz = 200,
                                          .desc = "IMU sample per data-ready") PayloadBtcImu {
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel X")
        int16_t accel_x_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel Y")
        int16_t accel_y_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel Z")
        int16_t accel_z_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro X")
        int16_t gyro_x_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro Y")
        int16_t gyro_y_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro Z")
        int16_t gyro_z_raw;
    };
    PACKET_TYPE_CHECK(PayloadBtcImu);

    /// EXP1_SPECTRUM - all 18 AS7265X channels in one atomic packet. 16-bit raw
    /// counts (was uint32 A/B split; merged so a measurement is whole-or-nothing
    /// on the wire - no more half-spectra on frame loss). 18*2 + 8 = 44 B.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP1_SPECTRUM, .node = "EXP1", .rate_hz = 2,
                                          .desc = "18-channel spectrum (matrix)") PayloadExp1Spectrum {
        // channels 0-8 = 410,435,460,485,510,535,560,UV-A,UV-B nm;
        // 9-17 = 585,610,645,680,705,730,760,810,NIR nm
        WIRE(.unit = "count", .gate = "measurement_valid", .desc = "AS7265x 18-channel spectrum (410nm..NIR)")
        uint16_t channels[SPECTRUM_CHANNELS]; // NOLINT(modernize-avoid-c-arrays)

        WIRE(.unit = "us", .desc = "measurement start, us since LO")
        uint32_t start_timestamp_us;

        WIRE(.unit = "count", .desc = "integration cycles (real time = 2*cycles*2.8ms)")
        uint8_t integration_cycles;

        WIRE(.desc = "gain: 0=1x, 1=3.7x, 2=16x, 3=64x")
        uint8_t gain;

        WIRE(.desc = "MATRIX row index -> LED/PWM/IT config (ICD-007)")
        uint8_t led_mask;

        WIRE(.desc = "1 = DATA_RDY asserted and readout ok")
        uint8_t measurement_valid;
    };
    PACKET_TYPE_CHECK(PayloadExp1Spectrum);

    /// *_ENV - the env layout emitted by EXP1/EXP2/EXP3 under their own type
    /// bytes (0x22/0x32/0x42). Three structs so each carries its own PACKET;
    /// static_asserts below guard against layout drift. Sensors as PayloadBtcEnv
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP1_ENV, .node = "EXP1", .rate_hz = 25,
                                          .desc = "common env sensors") PayloadExp1Env {
        WIRE(.desc = "valid bits: 0=ms5611, 1=tmp117")
        uint8_t valid_mask;
        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:1", .desc = "TMP117 board temperature")
        int16_t temp_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0", .desc = "MS5611 D1 pressure ADC (compensate with D2 + PROM coeffs)")
        uint32_t ms_pressure_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0",
             .desc = "MS5611 D2 temperature ADC (drives dT/TEMP for pressure comp)")
        uint32_t ms_temperature_raw;
    };
    PACKET_TYPE_CHECK(PayloadExp1Env);

    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP2_ENV, .node = "EXP2", .rate_hz = 25,
                                          .desc = "common env sensors") PayloadExp2Env {
        WIRE(.desc = "valid bits: 0=ms5611, 1=tmp117")
        uint8_t valid_mask;
        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:1", .desc = "TMP117 board temperature")
        int16_t temp_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0", .desc = "MS5611 D1 pressure ADC (compensate with D2 + PROM coeffs)")
        uint32_t ms_pressure_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0",
             .desc = "MS5611 D2 temperature ADC (drives dT/TEMP for pressure comp)")
        uint32_t ms_temperature_raw;
    };
    PACKET_TYPE_CHECK(PayloadExp2Env);

    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_ENV, .node = "EXP3", .rate_hz = 25,
                                          .desc = "common env sensors") PayloadExp3Env {
        WIRE(.desc = "valid bits: 0=ms5611, 1=tmp117")
        uint8_t valid_mask;
        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:1", .desc = "TMP117 board temperature")
        int16_t temp_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0", .desc = "MS5611 D1 pressure ADC (compensate with D2 + PROM coeffs)")
        uint32_t ms_pressure_raw;
        WIRE(.unit = "raw", .gate = "valid_mask:0",
             .desc = "MS5611 D2 temperature ADC (drives dT/TEMP for pressure comp)")
        uint32_t ms_temperature_raw;
    };
    PACKET_TYPE_CHECK(PayloadExp3Env);
    static_assert(sizeof(PayloadExp2Env) == sizeof(PayloadExp1Env), "EXP env layouts drifted");
    static_assert(sizeof(PayloadExp3Env) == sizeof(PayloadExp1Env), "EXP env layouts drifted");
    // Firmware builds one struct for all three EXPs (identical layout).
    using PayloadExpEnv = PayloadExp1Env;

    /// EXP_STATUS - the same status layout emitted by EXP1 (0x23) and EXP2
    /// (0x31). Sent every 25 ticks (1 Hz). The failure counters pinpoint which
    /// link of a spectrometer row's chain failed (EXP1 fills them, EXP2 zeros).
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP1_STATUS, .node = "EXP1", .rate_hz = 1,
                                          .desc = "system health") PayloadExp1Status {
        WIRE(.unit = "s", .desc = "uptime since MCU boot")
        uint32_t uptime_s;
        WIRE(.desc = "SD: 0=mounted, 1=failed")
        uint8_t sd_status;
        WIRE(.unit = "count", .desc = "LP5810 set_channels failures (saturating)")
        uint8_t led_write_fails;
        WIRE(.unit = "count", .desc = "set_integration/start_measurement failures (saturating)")
        uint8_t spec_start_fails;
        WIRE(.unit = "count", .desc = "DATA_RDY not asserted at readout (saturating)")
        uint8_t data_ready_fails;

        WIRE(.desc = "MissionMode: TEST / FLIGHT")
        MissionMode mode;
    };
    PACKET_TYPE_CHECK(PayloadExp1Status);

    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP2_STATUS, .node = "EXP2", .rate_hz = 1,
                                          .desc = "system health") PayloadExp2Status {
        WIRE(.unit = "s", .desc = "uptime since MCU boot")
        uint32_t uptime_s;
        WIRE(.desc = "SD: 0=mounted, 1=failed")
        uint8_t sd_status;
        WIRE(.unit = "count", .desc = "LP5810 set_channels failures (saturating)")
        uint8_t led_write_fails;
        WIRE(.unit = "count", .desc = "set_integration/start_measurement failures (saturating)")
        uint8_t spec_start_fails;
        WIRE(.unit = "count", .desc = "DATA_RDY not asserted at readout (saturating)")
        uint8_t data_ready_fails;

        WIRE(.desc = "MissionMode: TEST / FLIGHT")
        MissionMode mode;
    };
    PACKET_TYPE_CHECK(PayloadExp2Status);
    static_assert(sizeof(PayloadExp2Status) == sizeof(PayloadExp1Status), "EXP status layouts drifted");
    using PayloadExpStatus = PayloadExp1Status;

    /// EXP2_BER - bit error rate measurement for one transmission round.
    /// Core EXP2 science payload, sent every tick. EXP_ENV is sent in
    /// parallel.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP2_BER, .node = "EXP2", .rate_hz = 25,
                                          .desc = "LiFi bit-error round") PayloadExp2Ber {
        WIRE(.desc = "LiFi RATE_TABLE index for this round")
        uint8_t rate_index;

        WIRE(.unit = "us", .gate = "measurement_valid", .desc = "SEND command sent, us since LO")
        uint32_t timestamp_send_us;

        WIRE(.unit = "us", .gate = "measurement_valid", .desc = "response fully received, us since LO")
        uint32_t timestamp_recv_us;

        WIRE(.unit = "count", .gate = "measurement_valid", .desc = "bits transmitted this round")
        uint16_t bits_sent;

        WIRE(.unit = "count", .gate = "measurement_valid", .desc = "bit errors detected this round")
        uint16_t bit_errors;

        WIRE(.gate = "measurement_valid", .desc = "first errored byte (0xFF = none)")
        uint8_t first_error_byte;

        WIRE(.gate = "measurement_valid", .desc = "last errored byte (0xFF = none)")
        uint8_t last_error_byte;

        WIRE(.desc = "1 = round valid; 0 = ignore other fields")
        uint8_t measurement_valid;
    };
    PACKET_TYPE_CHECK(PayloadExp2Ber);

    /// EXP3_STACK_A - one sample from the WIRED stack (STM32U0 + Molex
    /// cable + LiFi A). Ground reconstructs each burst from burst_index.
    /// Mag sensitivity 16384 LSB/Gauss (18-bit); IMU +-32 g / +-2000 dps.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_STACK_A, .node = "EXP3", .rate_hz = 25,
                                          .desc = "wired stack sample") PayloadExp3StackA {
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wired MMC5983MA field X")
        int32_t mag_x_raw;
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wired MMC5983MA field Y")
        int32_t mag_y_raw;
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wired MMC5983MA field Z")
        int32_t mag_z_raw;

        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired accel X")
        int16_t accel_x_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired accel Y")
        int16_t accel_y_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired accel Z")
        int16_t accel_z_raw;

        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired gyro X")
        int16_t gyro_x_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired gyro Y")
        int16_t gyro_y_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wired gyro Z")
        int16_t gyro_z_raw;

        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:2", .desc = "wired-stack TMP117")
        int16_t temp_raw;

        WIRE(.unit = "us", .desc = "wired-stack sample latch, us since LO (LiFi domain)")
        uint32_t lifi_a_timestamp_us;

        WIRE(.unit = "us", .desc = "wired-stack wakeup-to-response latency")
        uint32_t latency_a_us;

        WIRE(.desc = "sample position within the current burst")
        uint8_t burst_index;

        WIRE(.desc = "valid: 0=mag, 1=imu, 2=tmp, 3=cable-LiFi handshake")
        uint8_t valid_mask;
    };
    PACKET_TYPE_CHECK(PayloadExp3StackA);

    /// EXP3_STACK_B - one sample from the WIRELESS stack (STM32U0 + LED
    /// power + LiFi B). Same sensors as STACK_A plus the storage-cap voltage.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_STACK_B, .node = "EXP3", .rate_hz = 25,
                                          .desc = "wireless stack sample") PayloadExp3StackB {
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wireless MMC5983MA field X")
        int32_t mag_x_raw;
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wireless MMC5983MA field Y")
        int32_t mag_y_raw;
        WIRE(.unit = "G", .scale = 1.0 / 16384.0, .gate = "valid_mask:0", .desc = "wireless MMC5983MA field Z")
        int32_t mag_z_raw;

        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless accel X")
        int16_t accel_x_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless accel Y")
        int16_t accel_y_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless accel Z")
        int16_t accel_z_raw;

        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless gyro X")
        int16_t gyro_x_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless gyro Y")
        int16_t gyro_y_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .gate = "valid_mask:1", .desc = "wireless gyro Z")
        int16_t gyro_z_raw;

        WIRE(.unit = "degC", .scale = 1.0 / 128.0, .gate = "valid_mask:2", .desc = "wireless-stack TMP117")
        int16_t temp_raw;

        WIRE(.unit = "raw", .gate = "valid_mask:3", .desc = "wireless-stack storage-cap voltage (raw ADC)")
        uint16_t cap_voltage_raw;

        WIRE(.unit = "us", .desc = "wireless-stack sample latch, us since LO (LiFi domain)")
        uint32_t lifi_b_timestamp_us;

        WIRE(.unit = "us", .desc = "wireless-stack wakeup-to-response latency")
        uint32_t latency_b_us;

        WIRE(.desc = "sample position within the current burst")
        uint8_t burst_index;

        WIRE(.desc = "valid: 0=mag, 1=imu, 2=tmp, 3=cap")
        uint8_t valid_mask;
    };
    PACKET_TYPE_CHECK(PayloadExp3StackB);

    /// EXP3_STATUS - EXP3 control-loop diagnostics, sent every 25 ticks
    /// (1 Hz).
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_STATUS, .node = "EXP3", .rate_hz = 1,
                                          .desc = "control-loop diagnostics") PayloadExp3Status {
        WIRE(.unit = "us", .desc = "wired-stack rolling-average latency estimate")
        uint32_t latency_a_estimated_us;

        WIRE(.unit = "us", .desc = "wireless-stack rolling-average latency estimate")
        uint32_t latency_b_estimated_us;

        WIRE(.unit = "us", .desc = "sync delay applied this cycle")
        uint32_t wait_a_used_us;

        WIRE(.desc = "SD: 0=mounted, 1=failed")
        uint8_t sd_status;

        WIRE(.desc = "MissionMode: TEST / FLIGHT")
        MissionMode mode;
    };
    PACKET_TYPE_CHECK(PayloadExp3Status);

    /// EXP3_IMU - controller IMU, mirrors BTC_IMU. +-32 g / +-2000 dps.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_IMU, .node = "EXP3", .rate_hz = 25,
                                          .desc = "controller ICM sample") PayloadExp3Imu {
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel X")
        int16_t accel_x_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel Y")
        int16_t accel_y_raw;
        WIRE(.unit = "g", .scale = 32.0 / 32768.0, .desc = "accel Z")
        int16_t accel_z_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro X")
        int16_t gyro_x_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro Y")
        int16_t gyro_y_raw;
        WIRE(.unit = "dps", .scale = 2000.0 / 32768.0, .desc = "gyro Z")
        int16_t gyro_z_raw;
    };
    PACKET_TYPE_CHECK(PayloadExp3Imu);

    // ---------------------------------------------------------------------------
    // System payloads
    // ---------------------------------------------------------------------------

    /// GAP_MARKER - explicit gap notification, emitted by the BTC or EXPs.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::GAP_MARKER, .node = "SYSTEM", .rate_hz = 0,
                                          .desc = "gap notification") PayloadGapMarker {
        WIRE(.unit = "tick", .desc = "first missing tick number")
        uint16_t first_missing_tick;

        WIRE(.unit = "count", .desc = "consecutive missing ticks")
        uint8_t count;

        WIRE(.desc = "why the data was missing (GapReason)")
        GapReason reason;

        WIRE(.desc = "origin node (NodeId)")
        NodeId source_node;
    };
    PACKET_TYPE_CHECK(PayloadGapMarker);

    /// FAULT - latched fault notification with error step trace (ADR-012).
    /// Emitted once per fault source. Header semantics differ from data
    /// packets: timestamp_us is when the error occurred at its origin,
    /// tick is the CAN tick current when it was reported (0 during init).
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::FAULT, .node = "SYSTEM", .rate_hz = 0,
                                          .desc = "latched fault + trace") PayloadFault {
        WIRE(.desc = "fault source = LED blink code (StatusLeds::Fault 1-10)")
        uint8_t fault_code;

        WIRE(.desc = "ErrorCode (TIMEOUT, BUS_ERROR)")
        uint8_t error_code;

        WIRE(.desc = "bit 0 = trace truncated (chain deeper than 6)")
        uint8_t flags;

        WIRE(.unit = "count", .desc = "valid entries in steps[]")
        uint8_t depth;

        WIRE(.desc = "origin source line (only with the exact build)")
        uint16_t line;

        WIRE(.desc = "Step trace [0]=origin outward (fault-trace-codes.md)")
        uint8_t steps[6]; // NOLINT(modernize-avoid-c-arrays)

        WIRE(.desc = "origin node (NodeId)")
        NodeId source_node;
    };
    PACKET_TYPE_CHECK(PayloadFault);

    /// BOOT - MCU startup notification. Sent at power-on before LO, tick=0
    /// and timestamp_us=0. Received during flight -> watchdog or soft reset.
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BOOT, .node = "SYSTEM", .rate_hz = 0,
                                          .desc = "MCU startup") PayloadBoot {
        WIRE(.desc = "BootReason: power-on / watchdog / soft reset")
        BootReason reason;

        WIRE(.unit = "count", .desc = "total boots since first power-on (0=first)")
        uint16_t reboot_count;

        WIRE(.desc = "origin node (NodeId)")
        NodeId source_node;
    };
    PACKET_TYPE_CHECK(PayloadBoot);

    /// CMD_ACK - acknowledges a received uplink command. Node BTC, not
    /// SYSTEM: the uplink terminates at the BTC, so no source_node needed
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::CMD_ACK, .node = "BTC", .rate_hz = 0,
                                          .desc = "uplink command ack") PayloadCmdAck {
        WIRE(.desc = "echoed uplink CommandOpcode")
        uint8_t opcode;
        WIRE(.desc = "echoed uplink sequence number")
        uint8_t seq;
        WIRE(.desc = "CommandAckStatus (ACCEPTED, UNKNOWN_OPCODE)")
        uint8_t status;
    };
    PACKET_TYPE_CHECK(PayloadCmdAck);

    /// *_TIMING - per-scope worst-case durations since the last send (WCET /
    /// tick budget); slots map to Wcet::Point. One type per node like
    /// ENV/STATUS, so the type byte names the origin and no source_node is needed
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BTC_TIMING, .node = "BTC", .rate_hz = 25,
                                          .desc = "per-scope tick timing") PayloadBtcTiming {
        WIRE(.unit = "us", .desc = "this tick's duration per Wcet::Point: TICK,READ,CFG,DRIVE,SEND,STORE")
        uint16_t max_us[6]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadBtcTiming);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP1_TIMING, .node = "EXP1", .rate_hz = 25,
                                          .desc = "per-scope tick timing") PayloadExp1Timing {
        WIRE(.unit = "us", .desc = "this tick's duration per Wcet::Point: TICK,READ,CFG,DRIVE,SEND,STORE")
        uint16_t max_us[6]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExp1Timing);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP2_TIMING, .node = "EXP2", .rate_hz = 25,
                                          .desc = "per-scope tick timing") PayloadExp2Timing {
        WIRE(.unit = "us", .desc = "this tick's duration per Wcet::Point: TICK,READ,CFG,DRIVE,SEND,STORE")
        uint16_t max_us[6]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExp2Timing);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_TIMING, .node = "EXP3", .rate_hz = 25,
                                          .desc = "per-scope tick timing") PayloadExp3Timing {
        WIRE(.unit = "us", .desc = "this tick's duration per Wcet::Point: TICK,READ,CFG,DRIVE,SEND,STORE")
        uint16_t max_us[6]; // NOLINT(modernize-avoid-c-arrays)
    };
    PACKET_TYPE_CHECK(PayloadExp3Timing);

    /// Layout shared by every *_TIMING type; the caller picks the per-node type
    using PayloadTiming = PayloadBtcTiming;

    /// *_TEST - one self-test step result (FULL_SYSTEM_TEST, TEST mode only).
    /// One type per node like ENV/STATUS/TIMING. `last` also drives the BTC's
    /// sequencer: it advances to the next node when it sees last=1 pass by
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::BTC_TEST, .node = "BTC", .rate_hz = 0,
                                          .desc = "self-test step result") PayloadBtcTest {
        WIRE(.desc = "which step of this node's self-test ran")
        uint8_t test_id;
        WIRE(.desc = "TestResult: PASS / FAIL / SKIPPED")
        TestResult result;
        WIRE(.desc = "1 = last step of this node's run (the sequencer's go-ahead)")
        uint8_t last;
        WIRE(.desc = "raw diagnostic value of the step (meaning depends on test_id); the ground judges it")
        uint32_t data;
    };
    PACKET_TYPE_CHECK(PayloadBtcTest);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP1_TEST, .node = "EXP1", .rate_hz = 0,
                                          .desc = "self-test step result") PayloadExp1Test {
        WIRE(.desc = "which step of this node's self-test ran")
        uint8_t test_id;
        WIRE(.desc = "TestResult: PASS / FAIL / SKIPPED")
        TestResult result;
        WIRE(.desc = "1 = last step of this node's run (the sequencer's go-ahead)")
        uint8_t last;
        WIRE(.desc = "raw diagnostic value of the step (meaning depends on test_id); the ground judges it")
        uint32_t data;
    };
    PACKET_TYPE_CHECK(PayloadExp1Test);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP2_TEST, .node = "EXP2", .rate_hz = 0,
                                          .desc = "self-test step result") PayloadExp2Test {
        WIRE(.desc = "which step of this node's self-test ran")
        uint8_t test_id;
        WIRE(.desc = "TestResult: PASS / FAIL / SKIPPED")
        TestResult result;
        WIRE(.desc = "1 = last step of this node's run (the sequencer's go-ahead)")
        uint8_t last;
        WIRE(.desc = "raw diagnostic value of the step (meaning depends on test_id); the ground judges it")
        uint32_t data;
    };
    PACKET_TYPE_CHECK(PayloadExp2Test);

    /// @ingroup comms
    struct __attribute__((packed)) PACKET(.type = PayloadType::EXP3_TEST, .node = "EXP3", .rate_hz = 0,
                                          .desc = "self-test step result") PayloadExp3Test {
        WIRE(.desc = "which step of this node's self-test ran")
        uint8_t test_id;
        WIRE(.desc = "TestResult: PASS / FAIL / SKIPPED")
        TestResult result;
        WIRE(.desc = "1 = last step of this node's run (the sequencer's go-ahead)")
        uint8_t last;
        WIRE(.desc = "raw diagnostic value of the step (meaning depends on test_id); the ground judges it")
        uint32_t data;
    };
    PACKET_TYPE_CHECK(PayloadExp3Test);

    /// Layout shared by every *_TEST type; the caller picks the per-node type
    using PayloadTest = PayloadBtcTest;

} // namespace PacketProtocol
