/// THHOR-BOLT REXUS 37 - full-system self-test contract
///
/// test_id in a *_TEST packet is the INDEX into the sending node's step
/// table; these enums pin index -> meaning per node. The firmware
/// static_asserts its table length against the *_SELF_TEST_COUNT
/// constants, the ground tool takes names and labels from the schema -
/// neither side can drift from this file.
///
/// Append new steps at the END of a node's enum: existing test_ids are
/// wire-stable, captures already carry them.
///
/// @ingroup comms

#pragma once

#include <bolt/wire/annotations.hpp>

#include <cstdint>

namespace PacketProtocol {

    /// BTC self-test steps, in run order
    /// @ingroup comms
    enum class BtcSelfTest : uint8_t {
        TMP_WHOAMI WIRE(.desc = "TMP117 device ID register", .label = "TMP117 WHO_AM_I") = 0x00,
        TMP_READ WIRE(.desc = "TMP117 raw temperature over the flight read path", .label = "TMP117 read") = 0x01,
        BARO_PROM WIRE(.desc = "MS5611 PROM CRC-4 + C1 fingerprint", .label = "MS5611 PROM CRC") = 0x02,
        IMU_WHOAMI WIRE(.desc = "ICM-42686 WHO_AM_I", .label = "ICM-42686 WHO_AM_I") = 0x03,
        IMU_READ WIRE(.desc = "ICM-42686 accel/gyro Z sample", .label = "ICM-42686 read") = 0x04,
        SD_MOUNTED WIRE(.desc = "SD card mounted", .label = "SD mounted") = 0x05,
    };
    inline constexpr uint8_t BTC_SELF_TEST_COUNT = 6U;

    /// EXP1 "Space Disco" self-test steps, in run order. Presence first,
    /// then one lit measurement per LED, so a failure names the colour
    /// @ingroup comms
    enum class Exp1SelfTest : uint8_t {
        TMP_WHOAMI WIRE(.desc = "TMP117 device ID register", .label = "TMP117 WHO_AM_I") = 0x00,
        TMP_READ WIRE(.desc = "TMP117 raw temperature over the flight read path", .label = "TMP117 read") = 0x01,
        BARO_PROM WIRE(.desc = "MS5611 PROM CRC-4 + C1 fingerprint", .label = "MS5611 PROM CRC") = 0x02,
        SPEC_WHOAMI WIRE(.desc = "AS7265x answers its HW version virtual register",
                         .label = "AS7265x HW version") = 0x03,
        LED_RGB_INIT WIRE(.desc = "LP5810C accepts its power-on sequence; write-only part, so "
                                  "configuring it is the test",
                          .label = "LP5810C configured") = 0x04,
        LED_UVIR_INIT WIRE(.desc = "LP5810D accepts its power-on sequence; write-only part, so "
                                   "configuring it is the test",
                           .label = "LP5810D configured") = 0x05,
        SPEC_DARK WIRE(.desc = "AS7265x spectrum with all LEDs off; the reference for every lit step",
                       .label = "Spectrum dark") = 0x06,
        SPEC_RED WIRE(.desc = "spectrum with the red LED lit, vs. dark", .label = "Spectrum red") = 0x07,
        SPEC_GREEN WIRE(.desc = "spectrum with the green LED lit, vs. dark", .label = "Spectrum green") = 0x08,
        SPEC_BLUE WIRE(.desc = "spectrum with the blue LED lit, vs. dark", .label = "Spectrum blue") = 0x09,
        SPEC_WHITE WIRE(.desc = "spectrum with the white LED lit, vs. dark", .label = "Spectrum white") = 0x0A,
        SPEC_IR WIRE(.desc = "spectrum with the IR 940 nm LED lit, vs. dark", .label = "Spectrum IR 940nm") = 0x0B,
        SPEC_UV WIRE(.desc = "spectrum with the UV 400 nm LED lit, vs. dark", .label = "Spectrum UV 400nm") = 0x0C,
        SD_MOUNTED WIRE(.desc = "SD card mounted", .label = "SD mounted") = 0x0D,
    };
    inline constexpr uint8_t EXP1_SELF_TEST_COUNT = 14U;

    /// EXP2 "Bouncy Castle" self-test steps, in run order. Li-Fi front-end
    /// steps append here once their drivers exist
    /// @ingroup comms
    enum class Exp2SelfTest : uint8_t {
        TMP_WHOAMI WIRE(.desc = "TMP117 device ID register", .label = "TMP117 WHO_AM_I") = 0x00,
        TMP_READ WIRE(.desc = "TMP117 raw temperature over the flight read path", .label = "TMP117 read") = 0x01,
        BARO_PROM WIRE(.desc = "MS5611 PROM CRC-4 + C1 fingerprint", .label = "MS5611 PROM CRC") = 0x02,
        SD_MOUNTED WIRE(.desc = "SD card mounted", .label = "SD mounted") = 0x03,
    };
    inline constexpr uint8_t EXP2_SELF_TEST_COUNT = 4U;

    /// EXP3 "Floaty Boi" self-test steps, in run order. Stack-link steps
    /// append here once the burst pipeline exists
    /// @ingroup comms
    enum class Exp3SelfTest : uint8_t {
        TMP_WHOAMI WIRE(.desc = "TMP117 device ID register", .label = "TMP117 WHO_AM_I") = 0x00,
        TMP_READ WIRE(.desc = "TMP117 raw temperature over the flight read path", .label = "TMP117 read") = 0x01,
        BARO_PROM WIRE(.desc = "MS5611 PROM CRC-4 + C1 fingerprint", .label = "MS5611 PROM CRC") = 0x02,
        IMU_WHOAMI WIRE(.desc = "ICM-42686 WHO_AM_I", .label = "ICM-42686 WHO_AM_I") = 0x03,
        IMU_READ WIRE(.desc = "ICM-42686 accel/gyro Z sample", .label = "ICM-42686 read") = 0x04,
        SD_MOUNTED WIRE(.desc = "SD card mounted", .label = "SD mounted") = 0x05,
    };
    inline constexpr uint8_t EXP3_SELF_TEST_COUNT = 6U;

} // namespace PacketProtocol
