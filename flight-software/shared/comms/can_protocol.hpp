#pragma once

#include <cstdint>

namespace CanProtocol {

    // Payload: 2 bytes, little-endian uint16_t tick count
    constexpr uint32_t SYNC_ID = 0x001U;
    constexpr uint8_t SYNC_DLC = 2U;

    // Lower ID = higher arbitration priority; EXP1 wins over EXP2/3 on collision.
    constexpr uint32_t EXP1_DATA_ID = 0x010U;
    constexpr uint32_t EXP2_DATA_ID = 0x020U;
    constexpr uint32_t EXP3_DATA_ID = 0x030U;
    constexpr uint32_t EXP_DATA_ID_MIN = EXP1_DATA_ID;
    constexpr uint32_t EXP_DATA_ID_MAX = EXP3_DATA_ID;
    constexpr uint8_t EXP_DATA_LEN = 64U; // max reassembled packet size; trailing bytes zero

    // bxCAN fragmentation: each 8-byte frame carries 1 header byte + 7 payload bytes.
    // Header byte: (frame_index << 4) | frame_count  - 4 bits each, max 15 frames = 105 bytes.
    constexpr uint8_t BXCAN_BYTES_PER_FRAME = 7U;

} // namespace CanProtocol
