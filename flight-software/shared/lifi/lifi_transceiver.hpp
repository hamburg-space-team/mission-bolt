#pragma once

#include "device_base.hpp"
#include "errors.hpp"
#include "lifi_protocol.hpp"
#include "uart_link.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// Host driver for one STM32U031 LiFi transceiver on its own UART
/// (ICD-008). Never blocks: commands are enqueued on the UartLink as one
/// script, the replies are consumed by poll() once per tick. A chain of
/// commands is tracked as "N ACKs outstanding"; the transceiver executes
/// scripts in order and answers in order, so counting is enough.
///
/// Failure policy (ADR-005): a NAK, a stray byte where an ACK was due, or
/// no reply within ACK_TIMEOUT_MS after the script reached the transceiver
/// drops the chain and counts one strike. Three strikes latch the device;
/// the owning computer retries via the DeviceBase cooldown.
///
/// The two roles of the EXP2 measurement are the subclasses LifiSender and
/// LifiReceiver. EXP3's feed-through streams build on this base directly.
///
/// @ingroup lifi
class LifiTransceiver : public DeviceBase {
  public:
    using tick_fn = uint32_t (*)();

    LifiTransceiver(const LifiTransceiver&) = delete;
    LifiTransceiver& operator=(const LifiTransceiver&) = delete;
    LifiTransceiver(LifiTransceiver&&) = delete;
    LifiTransceiver& operator=(LifiTransceiver&&) = delete;
    virtual ~LifiTransceiver() = default;

    /// True while a command chain still waits for ACKs
    [[nodiscard]] bool pending() const noexcept {
        return this->expected_acks > 0U;
    }

    /// Consume host-UART replies. Call once per tick, before deciding on
    /// new commands. Bytes that arrive outside a command chain are handed
    /// to on_data() (feed-through payload). The error is the reason the
    /// outstanding chain died; the chain is dropped and a strike counted.
    [[nodiscard]] Result<void> poll();

    /// Drop an outstanding chain and everything received so far. For mode
    /// transitions and before a self-test probe; clears no strikes
    void reset() noexcept;

    /// '\P' alone, one ACK: does the transceiver answer at all? Used by the
    /// self-test. Flushes the RX side first (boot banner, stale data)
    [[nodiscard]] Result<void> ping();

  protected:
    /// tick_ms drives the reply deadline; tick_us stamps telemetry
    LifiTransceiver(UartLink& link, tick_fn tick_ms, tick_fn tick_us) noexcept;

    /// Enqueue `script` and expect `acks` ACKs for it. Fails with
    /// DISABLED when latched, BAD_ARGUMENT while a chain is still
    /// outstanding (the caller must wait for pending() to clear) and
    /// BUS_ERROR when the link's TX ring cannot take the script
    [[nodiscard]] Result<void> submit(std::span<const uint8_t> script, uint8_t acks, Step step);

    /// Bytes received while no ACK was due. Default: discarded
    virtual void on_data(std::span<const uint8_t> bytes) noexcept;

    /// Whether a byte that is neither ACK nor NAK, arriving where an ACK
    /// is due, is payload (feed-through role) or a fault (polling role)
    [[nodiscard]] virtual bool stray_is_data() const noexcept;

    [[nodiscard]] uint32_t now_ms() const noexcept {
        return this->tick_ms();
    }
    [[nodiscard]] uint32_t now_us() const noexcept {
        return this->tick_us != nullptr ? this->tick_us() : 0U;
    }
    [[nodiscard]] const UartLink& uart() const noexcept {
        return this->link;
    }

  private:
    static constexpr std::size_t DRAIN_CHUNK = 64U;
    /// poll() drains at most this many chunks: bounded work per tick even
    /// if the wire never goes quiet (I-2)
    static constexpr uint8_t DRAIN_ROUNDS = 16U;

    /// One byte where an ACK is due
    [[nodiscard]] Result<void> judge(uint8_t byte) noexcept;
    /// Chain died: forget it, count the strike, return the marked error
    [[nodiscard]] Result<void> fail_chain(const Error& e) noexcept;

    UartLink& link; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    tick_fn tick_ms;
    tick_fn tick_us;
    uint8_t expected_acks = 0U;
    uint32_t deadline_ms = 0U;
    Step chain_step = Step::NONE;
};

/// The transmitting transceiver of the EXP2 BER round: polling mode,
/// reception off, one block per tick via '\s' + '\S' (ICD-008 roles).
///
/// @ingroup lifi
class LifiSender final : public LifiTransceiver {
  public:
    LifiSender(UartLink& link, tick_fn tick_ms, tick_fn tick_us) noexcept : LifiTransceiver(link, tick_ms, tick_us) {
    }

    /// '\P \X \f <baud> \N <len>', 4 ACKs. Drops any outstanding chain
    /// first. block_len 1..MAX_BLOCK_LEN (limitation #1), baud within the
    /// optical front-end range
    [[nodiscard]] Result<void> configure(uint16_t block_len, uint32_t optical_baud);

    /// '\f <baud> \N <len>', 2 ACKs: the rate sweep. '\N' resizes the TX
    /// buffer to the new block and clears it; the next load refills it
    [[nodiscard]] Result<void> set_rate(uint32_t optical_baud, uint16_t block_len);

    /// '\s <block> \S', 1 ACK (transmission STARTED). block.size() must
    /// equal the configured block_len: 's' streams exactly N bytes, a
    /// short block would swallow the '\S' as payload. Only when ready()
    [[nodiscard]] Result<void> load_and_send(std::span<const uint8_t> block);

    /// The last '\S' was acknowledged and its bytes have left the optics
    /// (bytes x 10 / baud after the upload). Loading earlier would
    /// overwrite a buffer the DMA is still reading
    [[nodiscard]] bool optical_done() const noexcept;

    /// Free to take the next load_and_send()
    [[nodiscard]] bool ready() const noexcept {
        return !is_failed() && !pending() && optical_done();
    }

    /// tick_us stamp of the last load_and_send()
    [[nodiscard]] uint32_t last_send_us() const noexcept {
        return this->sent_us;
    }
    [[nodiscard]] uint16_t block_len() const noexcept {
        return this->len;
    }
    [[nodiscard]] uint32_t optical_baud() const noexcept {
        return this->baud;
    }

  private:
    static constexpr uint8_t CONFIGURE_ACKS = 4U;
    static constexpr uint8_t SET_RATE_ACKS = 2U;
    static constexpr uint8_t SEND_ACKS = 1U;
    static constexpr std::size_t LOAD_OVERHEAD = 4U; // '\s' + '\S'

    [[nodiscard]] static Result<void> check(uint16_t block_len, uint32_t optical_baud, Step step) noexcept;

    LifiProtocol::Script<LOAD_OVERHEAD + LifiProtocol::MAX_BLOCK_LEN> load_script{};
    uint16_t len = 0U;
    uint32_t baud = 0U;
    uint32_t sent_us = 0U;
    uint32_t optical_done_ms = 0U;
};

/// The receiving transceiver of the EXP2 BER round: feed-through mode with
/// reception always on, so every optical byte lands on the host UART
/// unsolicited and is collected into one block per tick (ICD-008 roles).
///
/// @ingroup lifi
class LifiReceiver final : public LifiTransceiver {
  public:
    /// State of the block being collected (ICD-008 block completion)
    enum class Block : uint8_t {
        EMPTY,    ///< nothing arrived since take()
        FILLING,  ///< partial and the line was busy within the cooldown
        COOLDOWN, ///< partial and quiet for at least the cooldown: over
        COMPLETE, ///< block_len bytes arrived
    };

    /// Outcome of compare(): what the round looked like on the wire. Bytes
    /// that never arrived count as 8 errors each, so a partial block reads
    /// as a valid measurement with a high error rate (ICD-008 failure
    /// handling), not as a clean one
    struct Ber {
        uint16_t bits_sent;       ///< expected.size() x 8
        uint16_t bit_errors;      ///< mismatching bits + 8 per missing byte
        uint16_t missing_bytes;   ///< expected.size() - received (0 when complete)
        uint16_t overflow_bytes;  ///< received beyond block_len, discarded
        uint8_t first_error_byte; ///< index of the first bad byte, NO_ERROR_BYTE when clean
        uint8_t last_error_byte;  ///< index of the last bad byte, NO_ERROR_BYTE when clean
        bool first_byte_ok;       ///< frame sync: byte 0 arrived and matches
    };
    static constexpr uint8_t NO_ERROR_BYTE = 0xFFU;

    LifiReceiver(UartLink& link, tick_fn tick_ms, tick_fn tick_us) noexcept : LifiTransceiver(link, tick_ms, tick_us) {
    }

    /// '\P \X \N <len> \T <thr> \f <baud> \F \R', 7 ACKs. Drops any
    /// outstanding chain and the collected block first. From the last ACK
    /// on every byte on this UART is optical data. '\N' before '\R' resets
    /// the receive bookkeeping (limitation #2). cooldown_ms: quiet time
    /// that ends a partial block; 5 ms from 38 400 up, 10 ms at 9 600
    [[nodiscard]] Result<void> configure(uint16_t block_len, uint32_t optical_baud, uint16_t threshold,
                                         uint8_t cooldown_ms);

    /// '\f <baud> \R', 2 ACKs ('\R' again because '\f' switches the front
    /// end off, limitation #3). The ACKs share the UART with data: call
    /// only while the sender is idle, and take() the last block before
    /// and again after, since a straggler between the ACKs lands in it
    [[nodiscard]] Result<void> set_rate(uint32_t optical_baud, uint16_t block_len, uint8_t cooldown_ms);

    /// Where the current block stands; evaluated against now
    [[nodiscard]] Block block_state() const noexcept;

    /// Judge the collected block against `expected` (size = block_len)
    [[nodiscard]] Ber compare(std::span<const uint8_t> expected) const noexcept;

    /// The bytes collected so far
    [[nodiscard]] std::span<const uint8_t> block() const noexcept {
        return std::span<const uint8_t>(this->buf.data(), this->count);
    }

    /// tick_us stamp of the last byte of the block (from the RX interrupt)
    [[nodiscard]] uint32_t last_byte_us() const noexcept {
        return this->last_data_us;
    }

    /// Start the next block: forget the current one. Returns how many
    /// bytes it had
    uint16_t take() noexcept;

    [[nodiscard]] uint16_t block_len() const noexcept {
        return this->len;
    }

  protected:
    void on_data(std::span<const uint8_t> bytes) noexcept override;
    [[nodiscard]] bool stray_is_data() const noexcept override;

  private:
    static constexpr uint8_t CONFIGURE_ACKS = 7U;
    static constexpr uint8_t SET_RATE_ACKS = 2U;

    [[nodiscard]] static Result<void> check(uint16_t block_len, uint32_t optical_baud, Step step) noexcept;

    std::array<uint8_t, LifiProtocol::MAX_BLOCK_LEN> buf{};
    uint16_t len = 0U;
    uint16_t count = 0U;
    uint16_t overflow = 0U;
    uint8_t cooldown_ms = 5U;
    uint32_t last_data_ms = 0U;
    uint32_t last_data_us = 0U;
};
