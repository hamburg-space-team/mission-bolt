/// Wire metadata annotations for the downlink packet contract
///
/// Two annotation value types, read back by the host schema generator
/// (tools/schemagen, C++26 reflection - P2996 + P3394):
///
///   WIRE(...)   on a payload field OR an enumerator - unit, raw->engineering
///               scale/offset, validity gate, one-line description.
///   PACKET(...) on a payload struct - the wire type (the PayloadType value
///               itself), emitting node, nominal rate, description.
///
/// The generator turns these into the ground codec, the calibration tables and
/// the ICD, so packet_payloads/types stay the single source of truth.
///
/// @ingroup comms

#pragma once

#include <cstdint>

namespace PacketProtocol {

    enum class PayloadType : uint8_t;

    /// Per-field wire metadata
    struct wire {
        /// Engineering unit: "degC", "g", "dps", "Pa", "count", "us", "V",
        /// "bitmask", "enum", "raw" (raw = needs further on-ground calc).
        // NOLINTNEXTLINE(modernize-avoid-c-arrays) - char array so a string literal is a valid NTTP
        char unit[16] = "";
        /// engineering = raw * scale + offset (scale 1 / offset 0 = pass-through).
        double scale = 1.0;
        double offset = 0.0;
        /// Validity source, so ground never plots stale/garbage values:
        ///   ""                  always valid
        ///   "valid_mask:1"      valid only when bit 1 of valid_mask is set
        ///   "measurement_valid" valid only when that whole flag byte is != 0
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        char gate[24] = "";
        /// Short human label for the ground UI / ICD.
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        char desc[96] = "";
        /// Optional display label for the ground UI (uplink command buttons).
        /// Empty = the UI falls back to the canonical enum name (e.g. RESET_TICK).
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        char label[32] = "";
        /// Marks a dangerous/irreversible action - only meaningful on an uplink
        /// command enumerator (CommandOpcode). The ground UI gates these behind an
        /// arm/confirm step; the ICD flags them.
        bool danger = false;
    };

    /// Per-payload wire metadata, annotated on the payload struct
    // NOLINTNEXTLINE(readability-identifier-naming) - lowercase reads as packet{...}
    struct packet {
        /// Wire type, passed as the enum directly: `.type = PayloadType::BTC_ENV`.
        /// The generator casts it to the byte value; types.hpp stays the single
        /// owner of the numbers.
        PayloadType type = {};
        /// Emitting node: "BTC", "EXP1", "EXP2", "EXP3", "SYSTEM".
        // NOLINTNEXTLINE(modernize-avoid-c-arrays) - char array so a string literal is a valid NTTP
        char node[8] = "";
        /// Nominal downlink rate in Hz; 0 = event-driven (fault/boot/ack/gap).
        int rate_hz = 0;
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        char desc[64] = "";
    };

} // namespace PacketProtocol

#if defined(BOLT_SCHEMAGEN)
#define WIRE(...) [[= ::PacketProtocol::wire{__VA_ARGS__}]]
#define PACKET(...) [[= ::PacketProtocol::packet{__VA_ARGS__}]]
#else
#define WIRE(...)
#define PACKET(...)
#endif
