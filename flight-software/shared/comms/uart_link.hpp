#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

/// Non-blocking byte link over one UART: the tick loop enqueues and drains,
/// an interrupt moves bytes to and from the wire. Concrete implementation
/// (CmsisUartLink) owns the rings; drivers only see this interface, which
/// keeps them buildable in the host unit tests with a fake link.
///
/// Both queues are single-producer / single-consumer: exactly one context
/// calls write() and read() (the tick loop), the other side is the ISR.
///
/// @ingroup comms
class UartLink {
  public:
    virtual ~UartLink() = default;
    UartLink(const UartLink&) = delete;
    UartLink& operator=(const UartLink&) = delete;
    UartLink(UartLink&&) = delete;
    UartLink& operator=(UartLink&&) = delete;

    /// Enqueue every byte of `data` or none of them (I-2: never waits for
    /// ring space). False = the TX ring cannot hold the whole block.
    [[nodiscard]] virtual bool write(std::span<const uint8_t> data) noexcept = 0;

    /// Move up to out.size() received bytes into `out`; returns how many.
    /// Never blocks: 0 when nothing has arrived.
    [[nodiscard]] virtual std::size_t read(std::span<uint8_t> out) noexcept = 0;

    /// Millisecond stamp of the most recent received byte (taken in the
    /// receive interrupt), 0 before the first byte. Same clock as the
    /// tick_ms the drivers get, so idle gaps can be measured against it
    [[nodiscard]] virtual uint32_t last_rx_ms() const noexcept = 0;

    /// Microsecond stamp of the most recent received byte, for telemetry.
    /// Absolute stamp only (see Timing::us_now): never subtract two of them
    [[nodiscard]] virtual uint32_t last_rx_us() const noexcept = 0;

    /// Bytes the receive interrupt had to drop because the RX ring was full
    /// (saturating). A rising value between two reads means the drained
    /// data has a hole in it
    [[nodiscard]] virtual uint16_t rx_dropped() const noexcept = 0;

    /// Discard everything received so far
    void flush_rx() noexcept {
        uint8_t sink[FLUSH_CHUNK]; // NOLINT(modernize-avoid-c-arrays) - scratch, never escapes
        // bounded: a ring can hold at most FLUSH_ROUNDS * FLUSH_CHUNK bytes
        for (uint16_t round = 0U; round < FLUSH_ROUNDS; round++) {
            if (read(std::span<uint8_t>(sink, FLUSH_CHUNK)) == 0U) {
                return;
            }
        }
    }

  protected:
    UartLink() = default;

  private:
    static constexpr std::size_t FLUSH_CHUNK = 32U;
    static constexpr uint16_t FLUSH_ROUNDS = 256U; // 8 KiB, larger than any ring in the tree
};
