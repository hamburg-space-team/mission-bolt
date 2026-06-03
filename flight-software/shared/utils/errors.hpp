#pragma once

#include <cstdint>
#include <expected>

/// Single error vocabulary for the flight stack. Driver/bus/storage
/// calls return `std::expected<T, Error>`; on failure the caller gets
/// one of these codes back. See ADR-005 for how failures latch via
/// DeviceBase and how the BTC surfaces them as GAP_MARKER reasons.
///
/// @ingroup utils
enum class Error : uint8_t {
    /// I/O fault on a bus transaction
    BUS_ERROR = 1,

    /// Operation did not complete inside its deadline (I-2).
    TIMEOUT = 2,

    /// Caller-side mistake: null pointer, out-of-range value.
    BAD_ARGUMENT = 3,

    /// Device latched as failed or never initialised.
    DISABLED = 4,

    /// Wire-level mismatch
    PROTOCOL_ERROR = 5,

    /// Storage backend failed
    IO_ERROR = 6,

    /// Output buffer too small for the requested operation.
    OUTPUT_TOO_LARGE = 7,
};

/// Shorthand for the project-wide expected alias. `Result<T>` is what
/// every fallible flight-code call returns; `Result<void>` is the status-only variant.
///
/// @ingroup utils
template <typename T = void> using Result = std::expected<T, Error>;
