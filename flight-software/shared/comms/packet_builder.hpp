#pragma once

#include "errors.hpp"
#include <bolt/wire/header.hpp>
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/types.hpp>
#include <bolt/wire/uplink.hpp>

#include <array>
#include <cstdint>
#include <cstring>

#include "crc16_hw.hpp"
#include "wcet.hpp"

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian required");

namespace PacketProtocol {

    /// Strong-type wrapper for a 25 Hz tick. Forces callers of
    /// PacketBuilder::build to disambiguate tick from timestamp.
    ///
    /// @ingroup comms
    struct Tick {
        uint16_t value;
        explicit Tick(uint16_t v) noexcept : value(v) {
        }
    };

    /// Strong-type wrapper for a microsecond timestamp.
    ///
    /// @ingroup comms
    struct TimestampUs {
        uint32_t value;
        explicit TimestampUs(uint32_t v) noexcept : value(v) {
        }
    };

    /// Builds downlink frames into a caller-owned buffer. One instance
    /// per producer. Keeps independent per-type sequence counters so
    /// ground can detect gaps in any payload stream.
    ///
    /// @ingroup comms
    class PacketBuilder {
      private:
        std::array<uint8_t, 256> seq = {};

      public:
        /// Reset every sequence counter to 0 and bring up the hardware
        /// CRC (ADR-011). Call once at boot. Not idempotent mid-flight:
        /// ground would read the sequence reset as a gap.
        void init() noexcept {
            seq.fill(0U);
            Crc16Hw::init();
        }

        /// Write a full packet (header + payload + CRC) into buf.
        /// Layout: SYNC_0, SYNC_1, version, type, sequence, length,
        /// tick(2), timestamp_us(4), payload, CRC16 big-endian.
        /// buf must be at least MAX_PACKET_SIZE bytes.
        /// Returns the number of bytes written, or Error::OUTPUT_TOO_LARGE
        /// if payload_len exceeds MAX_PAYLOAD.
        [[nodiscard]] Result<uint8_t> build(uint8_t* buf, PayloadType type, Tick tick, TimestampUs ts_us,
                                            const void* payload, uint8_t payload_len) noexcept {
            const uint16_t total = static_cast<uint16_t>(HEADER_SIZE) + payload_len + CRC_SIZE;

            if (payload_len > MAX_PAYLOAD || total > MAX_PACKET_SIZE) {
                return fail(ErrorCode::OUTPUT_TOO_LARGE, Step::PKT_BUILD, __LINE__);
            }

            uint8_t offset = 0U;

            buf[offset++] = SYNC_0;
            buf[offset++] = SYNC_1;
            buf[offset++] = PROTOCOL_VERSION;
            buf[offset++] = static_cast<uint8_t>(type);

            const auto type_idx = static_cast<uint8_t>(type);
            buf[offset++] = seq[type_idx]++;

            buf[offset++] = payload_len;

            buf[offset++] = static_cast<uint8_t>(tick.value);
            buf[offset++] = static_cast<uint8_t>(tick.value >> 8U);

            buf[offset++] = static_cast<uint8_t>(ts_us.value);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 8U);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 16U);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 24U);

            std::memcpy(buf + offset, payload, payload_len);

            // CRC over everything after the 2 sync bytes. Hardware CRC
            // peripheral (ADR-011); byte-identical to Crc::compute, verified
            // by the boot self-test with automatic software fallback.
            const auto crc_len = static_cast<std::size_t>(HEADER_SIZE - 2U + payload_len);
            const uint16_t crc = Crc16Hw::compute(buf + 2U, crc_len);

            const auto crc_offset = static_cast<uint16_t>(HEADER_SIZE + payload_len);
            buf[crc_offset] = static_cast<uint8_t>(crc >> 8U);
            buf[crc_offset + 1U] = static_cast<uint8_t>(crc);

            return static_cast<uint8_t>(total);
        }

        /// Shortcut for a PayloadGapMarker frame.
        [[nodiscard]] Result<uint8_t> build_gap(uint8_t* buf, Tick tick, TimestampUs ts_us,
                                                PayloadGapMarker gap) noexcept {
            return build(buf, PayloadType::GAP_MARKER, tick, ts_us, &gap, sizeof(gap));
        }

        /// Shortcut for a PayloadFault frame (ADR-012). The header
        /// timestamp is taken from the Error itself - the moment the
        /// failure occurred at its origin - while `tick` is the CAN tick
        /// current at REPORT time (pass Tick{0} during init).
        [[nodiscard]] Result<uint8_t> build_fault(uint8_t* buf, Tick tick, uint8_t fault_code, const Error& err,
                                                  NodeId source_node) noexcept {
            PayloadFault fault{};
            fault.fault_code = fault_code;
            fault.error_code = static_cast<uint8_t>(err.code);
            fault.flags = err.truncated ? 0x01U : 0x00U;
            fault.depth = err.depth;
            fault.line = err.line;
            static_assert(sizeof(fault.steps) == TRACE_DEPTH);
            for (uint8_t i = 0U; i < TRACE_DEPTH; i++) {
                fault.steps[i] = static_cast<uint8_t>(err.trace[i]);
            }
            fault.source_node = source_node;
            return build(buf, PayloadType::FAULT, tick, TimestampUs{err.timestamp_us}, &fault, sizeof(fault));
        }

        /// Shortcut for a PayloadCmdAck frame
        [[nodiscard]] Result<uint8_t> build_cmd_ack(uint8_t* buf, Tick tick, TimestampUs ts_us, uint8_t opcode,
                                                    uint8_t seq, CommandAckStatus status) noexcept {
            PayloadCmdAck ack{};
            ack.opcode = opcode;
            ack.seq = seq;
            ack.status = static_cast<uint8_t>(status);
            return build(buf, PayloadType::CMD_ACK, tick, ts_us, &ack, sizeof(ack));
        }

        /// Shortcut for a PayloadTiming frame - the per-scope worst-case
        /// durations for WCET analysis. Caller resets the table after sending.
        [[nodiscard]] Result<uint8_t> build_timing(uint8_t* buf, Tick tick, TimestampUs ts_us, NodeId source_node,
                                                   const ::Wcet::Timing& t) noexcept {
            static_assert(::Wcet::POINT_COUNT == 6U, "PayloadTiming.max_us size must match Wcet::POINT_COUNT");
            PayloadTiming pt{};
            pt.source_node = source_node;
            for (uint8_t i = 0U; i < ::Wcet::POINT_COUNT; i++) {
                pt.max_us[i] = t.max_us[i];
            }
            return build(buf, PayloadType::TIMING, tick, ts_us, &pt, sizeof(pt));
        }

        /// Shortcut for a PayloadBoot frame. tick and timestamp default
        /// to 0 (LO has not yet been received at boot).
        [[nodiscard]] Result<uint8_t> build_boot(uint8_t* buf, BootReason reason, uint16_t reboot_count,
                                                 NodeId source_node) noexcept {
            PayloadBoot boot{};
            boot.reason = reason;
            boot.reboot_count = reboot_count;
            boot.source_node = source_node;
            return build(buf, PayloadType::BOOT, Tick{0U}, TimestampUs{0U}, &boot, sizeof(boot));
        }
    };

} // namespace PacketProtocol
