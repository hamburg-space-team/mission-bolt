#pragma once

#include "icm42686.hpp"
#include <bolt/wire/types.hpp>

#include <cstdint>

/// Shared ICM-42686 self-test bodies. Templated on the front because the IMU
/// sits behind ImuSupervisor on the BTC and the raw driver on EXP3, which
/// expose the same diagnostic surface
///
/// @ingroup core
namespace ImuSelfTest {

    /// WHOAMI: `data` = raw register, PASS iff it matches the ICM-42686 ID
    template <typename Imu> [[nodiscard]] PacketProtocol::TestResult whoami(Imu& imu, uint32_t& data) noexcept {
        if (imu.is_failed()) {
            return PacketProtocol::TestResult::SKIPPED;
        }
        const auto id = imu.read_who_am_i();
        if (!id) {
            return PacketProtocol::TestResult::FAIL;
        }
        data = *id;
        return (*id == ICM42686::EXPECTED_WHO_AM_I) ? PacketProtocol::TestResult::PASS
                                                    : PacketProtocol::TestResult::FAIL;
    }

    /// READ: one sample over the real flight path. `data` = accel Z (low
    /// half) | gyro Z (high half), raw i16; on the pad ~1 g / ~0 expected
    template <typename Imu> [[nodiscard]] PacketProtocol::TestResult read(Imu& imu, uint32_t& data) noexcept {
        if (imu.is_failed()) {
            return PacketProtocol::TestResult::SKIPPED;
        }
        const auto sample = imu.read_sample();
        if (!sample) {
            return PacketProtocol::TestResult::FAIL;
        }
        data = static_cast<uint16_t>(sample->accel_z) |
               (static_cast<uint32_t>(static_cast<uint16_t>(sample->gyro_z)) << 16U);
        return PacketProtocol::TestResult::PASS;
    }

} // namespace ImuSelfTest
