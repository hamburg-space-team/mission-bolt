#pragma once

#include <array>
#include <cstdint>
#include <expected>

/// Failure cause for the flight stack. Driver/bus/storage calls return
/// `std::expected<T, Error>`; the Error record carries one of these
/// codes plus the step trace of how it propagated (ADR-012). See
/// ADR-005 for how failures latch via DeviceBase.
///
/// @ingroup utils
enum class ErrorCode : uint8_t {
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

/// Step dictionary for the error trace (ADR-012). One value per
/// SEMANTIC level of a driver operation - not per bus transaction -
/// so a full origin-to-caller chain fits into TRACE_DEPTH entries.
///
/// Values go over the wire in FAULT packets and are decoded on ground
/// via docs/handbook/fault-trace-codes.md. They are therefore
/// WIRE-STABLE: never renumber or reuse a value, only append inside
/// the module's range (or start a new range).
///
/// Ranges: 0x1x I2C bus, 0x2x MS5611, 0x3x TMP117, 0x4x AS7265X,
/// 0x5x LP5810, 0x6x ICM4268x, 0x7x storage, 0x8x comms.
///
/// @ingroup utils
enum class Step : uint8_t {
    NONE = 0x00,

    // CmsisI2CBus. Pass-through helpers (write_reg8/16, read_reg8/16)
    // are transparent: they add no level of their own.
    I2C_INIT = 0x10,          ///< init(): Initialize/PowerControl
    I2C_RESET = 0x11,         ///< reset()
    I2C_WRITE = 0x12,         ///< write(): MasterTransmit + completion
    I2C_READ = 0x13,          ///< read(): MasterReceive + completion
    I2C_WRITE_READ = 0x14,    ///< write_read(): TX phase (RX phase = I2C_READ)
    I2C_WAIT_COMPLETE = 0x15, ///< wait_complete(): timeout / NACK / bus error

    // MS5611
    BARO_RESET = 0x20,      ///< reset command
    BARO_PROM_READ = 0x21,  ///< PROM coefficient readout
    BARO_PROM_CRC = 0x22,   ///< PROM CRC mismatch
    BARO_START_CONV = 0x23, ///< D1/D2 conversion start
    BARO_READ_ADC = 0x24,   ///< ADC result readout
    BARO_COLLECT = 0x25,    ///< collect_pending(): ADC=0 -> conversion unfinished
    BARO_READ = 0x26,       ///< read(): DISABLED / pipeline priming

    // TMP117
    TMP_INIT = 0x30,     ///< init(): device-ID readout
    TMP_ID_CHECK = 0x31, ///< DEV_ID mismatch
    TMP_CONFIG = 0x32,   ///< continuous-mode config write
    TMP_READ = 0x33,     ///< temperature register readout

    // AS7265X
    SPEC_INIT = 0x40,            ///< init(): integration time + control setup
    SPEC_START_MEAS = 0x41,      ///< start_measurement()
    SPEC_SET_INTEGRATION = 0x42, ///< set_integration()
    SPEC_WAIT_STATUS = 0x43,     ///< wait_status(): mailbox TX/RX poll
    SPEC_VREG_WRITE = 0x44,      ///< write_virtual()
    SPEC_VREG_READ = 0x45,       ///< read_virtual()
    SPEC_DEV_SEL = 0x46,         ///< die select via VREG_DEV_SEL
    SPEC_READ_DIES = 0x47,       ///< read_channels_dies(): channel readout

    // LP5810
    LED_INIT = 0x50,         ///< init()
    LED_CONFIGURE = 0x51,    ///< configure(): power-on register sequence
    LED_ENABLE_CHIP = 0x52,  ///< enable_chip(): Chip_EN retries exhausted
    LED_SET_CHANNELS = 0x53, ///< set_channels() incl. failed recovery
    LED_APPLY = 0x54,        ///< apply_channels(): PWM0-3 + LED_EN writes
    LED_DISABLE_ALL = 0x55,  ///< disable_all()
    LED_RECOVER = 0x56,      ///< recover(): bus reset + re-init + re-apply

    // ICM42686 / ICM42688 (shared step set)
    IMU_WHOAMI = 0x60,       ///< WHO_AM_I readout / mismatch
    IMU_CONFIG_ACCEL = 0x61, ///< accel config write
    IMU_CONFIG_GYRO = 0x62,  ///< gyro config write
    IMU_POWER_ON = 0x63,     ///< PWR_MGMT0 write
    IMU_READ = 0x64,         ///< read_sample(): burst data readout
    IMU_CONFIG_INT = 0x65,   ///< INT1 routing/config writes (data-ready)

    // Storage (SdStore)
    SD_INIT = 0x70,  ///< init(): card info readout
    SD_MOUNT = 0x71, ///< littlefs mount/format
    SD_OPEN = 0x72,  ///< log file open
    SD_WRITE = 0x73, ///< write(): ring-buffer producer
    SD_FLUSH = 0x74, ///< flush(): lfs_file_sync

    // Comms
    PKT_BUILD = 0x80,    ///< PacketBuilder::build(): payload too large
    CAN_TX_RING = 0x81,  ///< CAN TX frame ring latched (never drains)
    UART_TX_RING = 0x82, ///< RS-422 TX byte ring latched (never drains)
};

/// Trace depth: fixed so the error path stays no-alloc and WCET-safe.
/// Deepest real chain is origin -> bus level -> driver helper ->
/// driver op -> caller (~5); 6 leaves headroom. Overflow sets
/// Error::truncated instead of writing past the end.
inline constexpr uint8_t TRACE_DEPTH = 6U;

/// Microsecond clock used to stamp an Error at its origin (fail()).
/// FlightComputer sets this once at boot to Platform::tick_us; on the
/// host (unit tests) it stays nullptr and timestamps read 0.
///
/// @ingroup utils
struct ErrorClock {
    using clock_fn = uint32_t (*)();
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static inline clock_fn now_us = nullptr;
};

/// The error value inside every Result<T>: what failed, where it was
/// born (step + source line + timestamp) and the chain of semantic
/// levels it propagated through (trace[0] = origin, outward from
/// there). Built by fail() at the origin; every forwarding level
/// appends itself with mark(). Fixed-size by design - see ADR-012.
///
/// @ingroup utils
struct Error {
    uint32_t timestamp_us = 0U;            ///< time of occurrence (ErrorClock at fail())
    uint16_t line = 0U;                    ///< __LINE__ of the origin
    ErrorCode code = ErrorCode::BUS_ERROR; ///< what went wrong
    uint8_t depth = 0U;                    ///< valid entries in trace
    bool truncated = false;                ///< chain was longer than TRACE_DEPTH
    std::array<Step, TRACE_DEPTH> trace{}; ///< [0] = origin, then outward

    /// Errors compare by cause: `if (r.error() == ErrorCode::TIMEOUT)`.
    constexpr bool operator==(ErrorCode c) const noexcept {
        return this->code == c;
    }
};
static_assert(sizeof(Error) <= 16U, "Error rides in every Result<T> - keep it small");

/// Build an Error outside an expected chain (e.g. synthesized for a
/// latched TX ring). Timestamped like fail(); line is optional since
/// there is often no meaningful origin line.
[[nodiscard]] inline Error make_error(ErrorCode code, Step origin, uint16_t line = 0U) noexcept {
    Error e{};
    e.code = code;
    e.line = line;
    e.trace[0] = origin;
    e.depth = 1U;
    if (ErrorClock::now_us != nullptr) {
        e.timestamp_us = ErrorClock::now_us();
    }
    return e;
}

/// Origin of a failure. Use exactly where the first unexpected is
/// created, passing the module-local step and __LINE__:
///
///   return fail(ErrorCode::TIMEOUT, Step::SPEC_WAIT_STATUS, __LINE__);
///
/// __LINE__ is passed explicitly (no macro, and no std::source_location,
/// which would embed file/function strings into flash).
[[nodiscard]] inline std::unexpected<Error> fail(ErrorCode code, Step origin, uint16_t line) noexcept {
    return std::unexpected(make_error(code, origin, line));
}

/// Forwarding level of a failure. Appends this level's step to the
/// trace (or just sets `truncated` when full - never writes past the
/// fixed array) and re-wraps for the return:
///
///   if (auto r = read_regs(...); !r) {
///       return mark(r.error(), Step::SPEC_READ_DIES);
///   }
///
/// Pure pass-through helpers should NOT mark - one entry per semantic
/// level keeps the deepest chains inside TRACE_DEPTH.
[[nodiscard]] constexpr std::unexpected<Error> mark(Error e, Step here) noexcept {
    if (e.depth < TRACE_DEPTH) {
        e.trace[e.depth] = here;
        e.depth++;
    } else {
        e.truncated = true;
    }
    return std::unexpected(e);
}

/// Project-wide expected alias: what every fallible flight-code call
/// returns; `Result<void>` is the status-only variant.
///
/// @ingroup utils
template <typename T = void> using Result = std::expected<T, Error>;
