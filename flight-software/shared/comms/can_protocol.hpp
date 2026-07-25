#pragma once

#include <cstdint>

/// @defgroup comms Communications

/// Wire constants for the internal CAN bus (BTC <-> EXP1/2/3). Standard
/// 11-bit IDs throughout. Lower ID = higher arbitration priority; EXP1
/// wins over EXP2/3 on collision.
///
/// @ingroup comms
namespace CanProtocol {

    /// SYNC frame: BTC -> EXPs, every 40ms. Payload is 4 bytes:
    ///   [0..1] little-endian uint16_t tick counter
    ///   [2]    BootState::Mode (0 = TEST, 1 = FLIGHT)
    ///   [3]    self-test target: NodeId whose turn it is, or SELF_TEST_NONE
    ///
    /// Mode and target ride every SYNC as levels, not events - an EXP that
    /// misses a frame converges within one tick
    constexpr uint32_t SYNC_ID = 0x001U;
    constexpr uint8_t SYNC_DLC = 4U;

    /// SYNC byte [3] when no self-test run is in progress (not a NodeId -
    /// 0xFF already means "origin unknown")
    constexpr uint8_t SELF_TEST_NONE = 0xFEU;

    constexpr uint32_t STORAGE_START_ID = 0x002U;
    constexpr uint8_t STORAGE_START_DLC = 0U;

    constexpr uint32_t EXP1_DATA_ID = 0x010U;
    constexpr uint32_t EXP2_DATA_ID = 0x020U;
    constexpr uint32_t EXP3_DATA_ID = 0x030U;
    constexpr uint32_t EXP_DATA_ID_MIN = EXP1_DATA_ID;
    constexpr uint32_t EXP_DATA_ID_MAX = EXP3_DATA_ID;

    /// Max reassembled EXP packet; trailing bytes zero-padded.
    constexpr uint8_t EXP_DATA_LEN = 64U;

    /// bxCAN fragmentation: each 8-byte frame carries 1 header byte
    /// + 7 payload bytes. Header byte = (frame_index << 4) | frame_count,
    /// 4 bits each, max 15 frames = 105 bytes (capped by EXP_DATA_LEN).
    constexpr uint8_t BXCAN_BYTES_PER_FRAME = 7U;

} // namespace CanProtocol
