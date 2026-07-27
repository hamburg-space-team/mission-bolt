// Bolt payload schema generator (HOST-ONLY, C++26 reflection).
//
// Reflects over the annotated downlink payload structs in bolt/wire/payloads.hpp
// and emits schema.json - the single source of truth for the ground codec:
// every payload, its wire fields (name, C type, byte offset), and the WIRE(...)
// metadata (unit, raw->engineering scale/offset, valid_mask gate).
//
// This is the "real reflection, host-only" path: it is NOT part of the flight
// build. It needs a C++26 compiler with P2996 (reflection) + P3394 (annotations)
// - the clang-p2996 fork. Just run tools/schemagen/run-schemagen.sh (installs
// via install-clang-p2996.sh), or by hand:
//
//   clang++ -std=c++26 -freflection -fannotation-attributes -fexpansion-statements \
//           -stdlib=libc++ -DBOLT_SCHEMAGEN -I../../include schemagen.cpp -o schemagen
//   ./schemagen > ../generated/schema.json
//
// Verified against clang-bb-p2996 (LLVM 21, 2026-07 nightly). std::meta is still
// pre-standardization, so a newer snapshot may need small tweaks to the calls
// below (identifier_of / offset_of / annotations_of / extract / define_static_array).

#include <bolt/wire/payloads.hpp>
#include <bolt/wire/selftest.hpp>
#include <bolt/wire/uplink.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <experimental/meta>
#include <string>
#include <string_view>
#include <vector>

namespace sm = std::meta;

// The payloads to describe. Adding a struct here is the only manual step; all
// field data (names, types, offsets, units, scales, gates) comes from
// reflection + the WIRE(...) annotations, so it can never drift.
constexpr auto PAYLOADS = std::array{
    ^^PacketProtocol::PayloadBtcEnv,
    ^^PacketProtocol::PayloadBtcStatus,
    ^^PacketProtocol::PayloadBtcImu,
    ^^PacketProtocol::PayloadExp1Spectrum,
    ^^PacketProtocol::PayloadExp1Env,
    ^^PacketProtocol::PayloadExp2Env,
    ^^PacketProtocol::PayloadExp3Env,
    ^^PacketProtocol::PayloadExp1Status,
    ^^PacketProtocol::PayloadExp2Status,
    ^^PacketProtocol::PayloadExp2Ber,
    ^^PacketProtocol::PayloadExp3StackA,
    ^^PacketProtocol::PayloadExp3StackB,
    ^^PacketProtocol::PayloadExp3Status,
    ^^PacketProtocol::PayloadExp3Imu,
    ^^PacketProtocol::PayloadGapMarker,
    ^^PacketProtocol::PayloadFault,
    ^^PacketProtocol::PayloadBoot,
    ^^PacketProtocol::PayloadCmdAck,
    // Per-node timing: list the real structs, not the PayloadTiming alias -
    // reflecting an alias yields no PACKET annotation
    ^^PacketProtocol::PayloadBtcTiming,
    ^^PacketProtocol::PayloadExp1Timing,
    ^^PacketProtocol::PayloadExp2Timing,
    ^^PacketProtocol::PayloadExp3Timing,
    // Per-node self-test step results (aliases carry no PACKET annotation)
    ^^PacketProtocol::PayloadBtcTest,
    ^^PacketProtocol::PayloadExp1Test,
    ^^PacketProtocol::PayloadExp2Test,
    ^^PacketProtocol::PayloadExp3Test,
};

// Pull the WIRE(...) annotation off a member, or a default if it has none.
// Reflection queries are consteval, so this runs in a constant-evaluated context.
consteval PacketProtocol::wire wire_of(sm::info member) {
    for (sm::info a : sm::annotations_of(member)) {
        if (sm::type_of(a) == ^^PacketProtocol::wire) {
            return sm::extract<PacketProtocol::wire>(a);
        }
    }
    return {}; // unit "", scale 1, offset 0, gate ""
}

consteval bool has_wire(sm::info member) {
    for (sm::info a : sm::annotations_of(member)) {
        if (sm::type_of(a) == ^^PacketProtocol::wire) {
            return true;
        }
    }
    return false;
}

// The PACKET(...) annotation on a PayloadType enumerator (node/rate/layout/desc).
consteval PacketProtocol::packet packet_of(sm::info enumerator) {
    for (sm::info a : sm::annotations_of(enumerator)) {
        if (sm::type_of(a) == ^^PacketProtocol::packet) {
            return sm::extract<PacketProtocol::packet>(a);
        }
    }
    return {};
}
consteval bool has_packet(sm::info enumerator) {
    for (sm::info a : sm::annotations_of(enumerator)) {
        if (sm::type_of(a) == ^^PacketProtocol::packet) {
            return true;
        }
    }
    return false;
}

// Enum types that appear as payload field types (GapReason, NodeId, BootReason,
// ...). Auto-collected so every enum used on the wire lands in the ICD - no
// manual list to keep in sync.
consteval std::vector<sm::info> used_enums() {
    std::vector<sm::info> enums;
    for (sm::info P : PAYLOADS) {
        for (sm::info m : sm::nonstatic_data_members_of(P, sm::access_context::current())) {
            sm::info t = sm::type_of(m);
            if (sm::is_array_type(t)) {
                t = sm::remove_extent(t);
            }
            if (sm::is_enum_type(t) && std::find(enums.begin(), enums.end(), t) == enums.end()) {
                enums.push_back(t);
            }
        }
    }
    return enums;
}

// The enums documented in the schema + ICD: those used as payload field types
// (auto-collected) plus the uplink command enums. The uplink opcode/status ride
// in header/payload bytes as plain u8, so they are not field types - but they
// are part of the contract, so the ground tool can decode them from the schema.
consteval std::vector<sm::info> documented_enums() {
    std::vector<sm::info> e = used_enums();
    e.push_back(^^PacketProtocol::CommandOpcode);
    e.push_back(^^PacketProtocol::CommandAckStatus);
    // per-node self-test tables: test_id -> meaning, ground reads the labels
    e.push_back(^^PacketProtocol::BtcSelfTest);
    e.push_back(^^PacketProtocol::Exp1SelfTest);
    e.push_back(^^PacketProtocol::Exp2SelfTest);
    e.push_back(^^PacketProtocol::Exp3SelfTest);
    return e;
}

// Canonical wire type name (u8/i16/u32/...) for a C++ scalar, keyed on the
// reflected display string - the same mapping bolt-codec's build.rs uses, so
// the ICD speaks the ground station's language, not the compiler's.
consteval std::string_view scalar_wire(std::string_view ctype) {
    if (ctype == "unsigned char" || ctype == "char")
        return "u8";
    if (ctype == "signed char")
        return "i8";
    if (ctype == "short" || ctype == "short int")
        return "i16";
    if (ctype == "unsigned short" || ctype == "unsigned short int")
        return "u16";
    if (ctype == "int")
        return "i32";
    if (ctype == "unsigned int")
        return "u32";
    if (ctype == "long" || ctype == "long int")
        return "i64";
    if (ctype == "unsigned long" || ctype == "unsigned long int")
        return "u64";
    return "?";
}

// Enums are all unsigned here; report the underlying wire width. Returns a
// literal (not a string_view) so it can feed printf's %s directly
consteval const char* enum_wire(std::size_t bytes) {
    return bytes == 1 ? "u8" : bytes == 2 ? "u16" : bytes == 4 ? "u32" : "u64";
}

// Wire type of a payload field: the element type for arrays, the underlying
// integer for enums (so `reason` reads as `u8`, not `GapReason`).
consteval std::string_view field_wire(sm::info type) {
    sm::info t = sm::is_array_type(type) ? sm::remove_extent(type) : type;
    if (sm::is_enum_type(t)) {
        return enum_wire(sm::size_of(t));
    }
    return scalar_wire(sm::display_string_of(t));
}

// Guard against "forgot the WIRE line": reflection still lists an un-annotated
// field, so it would slip into the schema with empty metadata. Every field must
// carry a wire annotation.
consteval bool all_fields_annotated(sm::info payload) {
    for (sm::info m : sm::nonstatic_data_members_of(payload, sm::access_context::current())) {
        if (!has_wire(m)) {
            return false;
        }
    }
    return true;
}

// Guard the parser's core assumption: __attribute__((packed)) means fields sit
// back-to-back in wire order with no padding. Verify offset == running sum and
// that the fields exactly fill sizeof (no trailing pad).
consteval bool layout_is_tight(sm::info payload) {
    std::size_t offset = 0;
    for (sm::info m : sm::nonstatic_data_members_of(payload, sm::access_context::current())) {
        if (sm::offset_of(m).bytes != offset) {
            return false;
        }
        offset += sm::size_of(sm::type_of(m));
    }
    return offset == sm::size_of(payload);
}

// JSON-escape is unnecessary here: units/descs are plain ASCII.
void emit_json() {
    // Wire types, from each payload struct's PACKET(...) annotation, so the Rust
    // PayloadType (enum + from_u8 + name + schema_name + source) is generated.
    std::printf("{\n  \"types\": [");
    bool first_type = true;
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view sname = sm::identifier_of(P); // "PayloadBtcEnv"
        constexpr std::string_view rname = sname.substr(7);      // drop "Payload" -> "BtcEnv"
        constexpr PacketProtocol::packet pk = packet_of(P);
        std::printf("%s\n    {\"name\": \"%.*s\", \"value\": %d, \"node\": \"%s\", \"rate_hz\": %d, "
                    "\"layout\": \"%.*s\", \"desc\": \"%s\"}",
                    first_type ? "" : ",", (int)rname.size(), rname.data(), static_cast<int>(pk.type), pk.node,
                    pk.rate_hz, (int)sname.size(), sname.data(), pk.desc);
        first_type = false;
    }
    std::printf("\n  ],\n  \"payloads\": [");

    bool first_payload = true;
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view pname = sm::identifier_of(P);
        constexpr std::size_t psize = sm::size_of(P);
        std::printf("%s\n    {\n      \"name\": \"%.*s\",\n      \"size\": %zu,\n      \"fields\": [",
                    first_payload ? "" : ",", (int)pname.size(), pname.data(), psize);
        first_payload = false;

        // define_static_array promotes the (allocating) members vector into
        // static storage so it is usable as a constant expression by template for.
        bool first_field = true;
        template for (constexpr sm::info m :
                      std::define_static_array(sm::nonstatic_data_members_of(P, sm::access_context::current()))) {
            constexpr std::string_view fname = sm::identifier_of(m);
            constexpr std::string_view ctype = sm::display_string_of(sm::type_of(m));
            constexpr std::size_t off = sm::offset_of(m).bytes;
            constexpr std::size_t count = sm::is_array_type(sm::type_of(m)) ? sm::extent(sm::type_of(m)) : 1;
            constexpr PacketProtocol::wire w = wire_of(m);
            // %.17g preserves the exact double so scales round-trip precisely.
            std::printf("%s\n        {\"name\":\"%.*s\",\"ctype\":\"%.*s\",\"byte_offset\":%zu,\"count\":%zu,"
                        "\"unit\":\"%s\",\"scale\":%.17g,\"offset\":%.17g,\"gate\":\"%s\",\"desc\":\"%s\"}",
                        first_field ? "" : ",", (int)fname.size(), fname.data(), (int)ctype.size(), ctype.data(), off,
                        count, w.unit, w.scale, w.offset, w.gate, w.desc);
            first_field = false;
        }
        std::printf("\n      ]\n    }");
    }
    std::printf("\n  ],\n  \"enums\": [");

    // Every documented enum with its values, so the ground codec generates the
    // name lookups (gap reason, node id, uplink command, ...) instead of mirroring
    // them by hand.
    bool first_enum = true;
    template for (constexpr sm::info E : std::define_static_array(documented_enums())) {
        constexpr std::string_view en = sm::identifier_of(E);
        std::printf("%s\n    {\"name\": \"%.*s\", \"underlying\": \"%s\", \"values\": [", first_enum ? "" : ",",
                    (int)en.size(), en.data(), enum_wire(sm::size_of(E)));
        first_enum = false;
        bool first_v = true;
        template for (constexpr sm::info c : std::define_static_array(sm::enumerators_of(E))) {
            constexpr std::string_view cn = sm::identifier_of(c);
            constexpr unsigned cv = static_cast<unsigned>(sm::extract<typename[:E:]>(c));
            constexpr PacketProtocol::wire w = wire_of(c);
            std::printf("%s{\"name\": \"%.*s\", \"value\": %u, \"desc\": \"%s\", \"label\": \"%s\", \"danger\": %s}",
                        first_v ? "" : ", ", (int)cn.size(), cn.data(), cv, w.desc, w.label,
                        w.danger ? "true" : "false");
            first_v = false;
        }
        std::printf("]}");
    }
    std::printf("\n  ]\n}\n");
}

// Human-readable interface control doc (same single source, so it never drifts).
void emit_icd(const char* generated_at) {
    constexpr int OVERHEAD = PacketProtocol::HEADER_SIZE + PacketProtocol::CRC_SIZE;

    std::printf("# ICD-007: Downlink packet payloads\n\n");
    std::printf("## Document Information\n\n");
    std::printf("| Field | Value |\n|-------|-------|\n");
    std::printf("| Document ID | ICD-007 |\n");
    std::printf("| Protocol version | %d |\n", (int)PacketProtocol::PROTOCOL_VERSION);
    std::printf("| Status | Generated |\n");
    std::printf("| Generated at | %s |\n", generated_at);
    std::printf("| Source | `bolt/wire/payloads.hpp` + `bolt/wire/types.hpp` |\n\n");
    std::printf("## Purpose\n\n");
    std::printf("Per-type binary payload definitions - the wire ABI between flight and ground. "
                "GENERATED by `tools/schemagen` from the `WIRE(...)` / `PACKET(...)` annotations via C++26 reflection; "
                "the header files are the single source of truth. Do not edit by hand.\n\n");
    std::printf("## Conventions\n\n");
    std::printf("- Multi-byte fields are little-endian. Payloads are `__attribute__((packed))` (no padding).\n");
    std::printf("- `gate` marks a field's validity source: `field:N` = valid only when bit N of `field` is set; "
                "a bare field name = valid only when that whole byte is non-zero; empty = always valid. Ground must "
                "not plot or act on a field whose gate is clear.\n");
    std::printf("- The `Type` column uses wire types (`u8`, `i16`, `u32`, `u16[18]` for arrays); enum fields show "
                "their underlying integer, with the value tables under [Enumerations](#enumerations).\n");
    std::printf("- `scale`/`offset`: engineering = raw * scale + offset. `unit` \"raw\" needs a sensor-specific "
                "on-ground calc (e.g. MS5611 uses D2 + PROM).\n");
    std::printf("- Header %d B + CRC16 %d B = %d B frame overhead per packet.\n", (int)PacketProtocol::HEADER_SIZE,
                (int)PacketProtocol::CRC_SIZE, OVERHEAD);

    // Frame format: endianness, the reflected header, the CRC. The offsets in the
    // payload tables below are meaningless without this context.
    std::printf("\n## Frame format\n\n");
    std::printf("```\n[ header %d B ][ payload 0..%d B ][ CRC16 %d B ]   %d B max, packed, no padding\n```\n\n",
                (int)PacketProtocol::HEADER_SIZE, (int)PacketProtocol::MAX_PAYLOAD, (int)PacketProtocol::CRC_SIZE,
                (int)PacketProtocol::MAX_PACKET_SIZE);
    std::printf("All multi-byte fields are little-endian. Sync bytes are `0x%02X 0x%02X`; protocol version `%d`.\n\n",
                (int)PacketProtocol::SYNC_0, (int)PacketProtocol::SYNC_1, (int)PacketProtocol::PROTOCOL_VERSION);
    std::printf("### Header (%d B)\n\n| Field | Type | Off | Size |\n|---|---|--:|--:|\n",
                (int)PacketProtocol::HEADER_SIZE);
    template for (constexpr sm::info m : std::define_static_array(
                      sm::nonstatic_data_members_of(^^PacketProtocol::PacketHeader, sm::access_context::current()))) {
        constexpr std::string_view fn = sm::identifier_of(m);
        constexpr sm::info ty = sm::type_of(m);
        constexpr std::string_view wt = field_wire(ty);
        constexpr std::size_t cnt = sm::is_array_type(ty) ? sm::extent(ty) : 1;
        std::printf("| %.*s | %.*s", (int)fn.size(), fn.data(), (int)wt.size(), wt.data());
        if (cnt > 1) {
            std::printf("[%zu]", cnt);
        }
        std::printf(" | %zu | %zu |\n", sm::offset_of(m).bytes, sm::size_of(ty));
    }
    std::printf("\n### CRC\n\nCRC-16/CCITT-FALSE (polynomial `0x1021`, init `0xFFFF`) over every byte after the two "
                "sync bytes - the header from `version` onward plus the payload - stored **big-endian** in the "
                "trailing %d B (the only big-endian field in the frame).\n",
                (int)PacketProtocol::CRC_SIZE);

    // Wire types. No bandwidth column: the rates are being reworked, a
    // computed budget here would only document a stale plan
    std::printf("\n## Wire types\n\n");
    std::printf("| Type | Byte | Node | Rate Hz | Payload B | Wire B | Description |\n");
    std::printf("|---|--:|---|--:|--:|--:|---|\n");
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view rname = std::string_view(sm::identifier_of(P)).substr(7);
        constexpr PacketProtocol::packet pk = packet_of(P);
        constexpr std::size_t psize = sm::size_of(P);
        constexpr std::size_t wire = psize + OVERHEAD;
        std::printf("| %.*s | 0x%02X | %s | %d | %zu | %zu | %s |\n", (int)rname.size(), rname.data(),
                    static_cast<int>(pk.type), pk.node, pk.rate_hz, psize, wire, pk.desc);
    }

    std::printf("\n## Payloads\n");
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view rname = std::string_view(sm::identifier_of(P)).substr(7);
        constexpr std::size_t psize = sm::size_of(P);
        constexpr PacketProtocol::packet pk = packet_of(P);
        std::printf("\n### %.*s - `0x%02X` (%zu B)\n\n", (int)rname.size(), rname.data(), static_cast<int>(pk.type),
                    psize);
        if (pk.rate_hz > 0) {
            std::printf("Emitter **%s**, %d Hz. %s.\n\n", pk.node, pk.rate_hz, pk.desc);
        } else {
            std::printf("Emitter **%s**, event-driven. %s.\n\n", pk.node, pk.desc);
        }
        std::printf("| Field | Type | Off | Size | Unit | Scale | Gate | Description |\n");
        std::printf("|---|---|--:|--:|---|--:|---|---|\n");
        template for (constexpr sm::info m :
                      std::define_static_array(sm::nonstatic_data_members_of(P, sm::access_context::current()))) {
            constexpr std::string_view fname = sm::identifier_of(m);
            constexpr sm::info ty = sm::type_of(m);
            constexpr std::string_view wt = field_wire(ty);
            constexpr std::size_t cnt = sm::is_array_type(ty) ? sm::extent(ty) : 1;
            constexpr std::size_t off = sm::offset_of(m).bytes;
            constexpr std::size_t sz = sm::size_of(ty);
            constexpr PacketProtocol::wire w = wire_of(m);
            std::printf("| %.*s | %.*s", (int)fname.size(), fname.data(), (int)wt.size(), wt.data());
            if (cnt > 1) {
                std::printf("[%zu]", cnt);
            }
            std::printf(" | %zu | %zu | %s | %g | %s | %s |\n", off, sz, w.unit, w.scale, w.gate, w.desc);
        }
    }

    // Every enum that appears as a wire field, so `reason: GapReason` etc. is
    // decodable at the ground without guessing the value meanings.
    std::printf("\n## Enumerations\n\n");
    std::printf("Downlink field enums plus the uplink command set (`CommandOpcode`/`CommandAckStatus`), so a value "
                "on the wire never needs a look in the source. \xE2\x9A\xA0\xEF\xB8\x8F marks a dangerous/irreversible "
                "command (the ground UI gates these behind an arm + confirm step).\n");
    template for (constexpr sm::info E : std::define_static_array(documented_enums())) {
        constexpr std::string_view enm = sm::identifier_of(E);
        std::printf("\n### %.*s (%s)\n\n| Value | Name | Description |\n|--:|---|---|\n", (int)enm.size(), enm.data(),
                    enum_wire(sm::size_of(E)));
        template for (constexpr sm::info c : std::define_static_array(sm::enumerators_of(E))) {
            constexpr std::string_view cn = sm::identifier_of(c);
            constexpr unsigned cv = static_cast<unsigned>(sm::extract<typename[:E:]>(c));
            constexpr PacketProtocol::wire w = wire_of(c); // same WIRE annotation, on the enumerator
            std::printf("| 0x%02X | %.*s%s | %s |\n", cv, (int)cn.size(), cn.data(),
                        w.danger ? " \xE2\x9A\xA0\xEF\xB8\x8F" : "", w.desc);
        }
    }
}

// LaTeX-escape for identifiers/units/descs going into the SED tables
std::string tex(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '_':
        case '&':
        case '%':
        case '#':
        case '$':
        case '{':
        case '}':
            out += '\\';
            out += c;
            break;
        case '<':
            out += "\\textless{}";
            break;
        case '>':
            out += "\\textgreater{}";
            break;
        case '~':
            out += "\\textasciitilde{}";
            break;
        case '^':
            out += "\\textasciicircum{}";
            break;
        case '\\':
            out += "\\textbackslash{}";
            break;
        default:
            out += c;
        }
    }
    return out;
}

// Same single source rendered as LaTeX for the SED appendix; \input it from the
// document. Needs float ([H]) and siunitx, both already used by the SED
void emit_tex(const char* generated_at) {
    constexpr int OVERHEAD = PacketProtocol::HEADER_SIZE + PacketProtocol::CRC_SIZE;

    std::printf("%% ICD-007 downlink payloads - GENERATED by tools/schemagen, do not edit\n");
    std::printf("%% Source: bolt/wire/payloads.hpp + bolt/wire/types.hpp; regenerate via run-schemagen.sh\n");
    std::printf("%% Generated at %s\n\n", generated_at);

    std::printf("The tables below mirror ICD-007: the per-type binary payload\n"
                "definitions of protocol version %d -- the wire ABI between flight\n"
                "software and ground tooling. They are generated (%s) from the\n"
                "\\texttt{WIRE(...)}/\\texttt{PACKET(...)} annotations in the flight\n"
                "headers via C++26 reflection, so this appendix cannot drift from\n"
                "the code.\n\n",
                (int)PacketProtocol::PROTOCOL_VERSION, generated_at);
    std::printf("\\begin{itemize}\n");
    std::printf("    \\item Multi-byte fields are little-endian; payloads are packed, no padding.\n");
    std::printf("    \\item \\emph{Gate} marks a field's validity source: \\texttt{field:N} = valid\n"
                "          only while bit N of \\texttt{field} is set; a bare field name = valid only\n"
                "          while that byte is non-zero; empty = always valid. Ground must not plot\n"
                "          or act on a field whose gate is clear.\n");
    std::printf("    \\item The \\emph{Type} column uses wire types (\\texttt{u8}, \\texttt{i16},\n"
                "          \\texttt{u16[18]} for arrays); enum fields show their underlying integer,\n"
                "          with the value tables under Enumerations.\n");
    std::printf("    \\item Engineering value = raw $\\cdot$ scale + offset. Unit \\texttt{raw} needs\n"
                "          a sensor-specific on-ground calculation (e.g.\\ MS5611 uses D2 + PROM).\n");
    std::printf("    \\item Header %d B + CRC16 %d B = %d B frame overhead per packet. A rate of 0\n"
                "          means event-driven.\n",
                (int)PacketProtocol::HEADER_SIZE, (int)PacketProtocol::CRC_SIZE, OVERHEAD);
    std::printf("\\end{itemize}\n");

    std::printf("\n\\subsubsection{Frame Format}\n\n");
    std::printf("\\begin{center}\n    \\texttt{[ header %d B ][ payload 0..%d B ][ CRC16 %d B ]}"
                "\\quad %d B max\n\\end{center}\n\n",
                (int)PacketProtocol::HEADER_SIZE, (int)PacketProtocol::MAX_PAYLOAD, (int)PacketProtocol::CRC_SIZE,
                (int)PacketProtocol::MAX_PACKET_SIZE);
    std::printf("Sync bytes are \\texttt{0x%02X 0x%02X}; protocol version \\texttt{%d}.\n\n",
                (int)PacketProtocol::SYNC_0, (int)PacketProtocol::SYNC_1, (int)PacketProtocol::PROTOCOL_VERSION);
    std::printf("\\begin{table}[H]\n    \\centering\\footnotesize\n    \\begin{tabular}{|l|l|r|r|}\n"
                "        \\hline\n        \\textbf{Field} & \\textbf{Type} & \\textbf{Off} & \\textbf{Size} \\\\\n"
                "        \\hline\n");
    template for (constexpr sm::info m : std::define_static_array(
                      sm::nonstatic_data_members_of(^^PacketProtocol::PacketHeader, sm::access_context::current()))) {
        constexpr std::string_view fn = sm::identifier_of(m);
        constexpr sm::info ty = sm::type_of(m);
        constexpr std::string_view wt = field_wire(ty);
        constexpr std::size_t cnt = sm::is_array_type(ty) ? sm::extent(ty) : 1;
        std::printf("        \\texttt{%s} & \\texttt{%.*s", tex(fn).c_str(), (int)wt.size(), wt.data());
        if (cnt > 1) {
            std::printf("[%zu]", cnt);
        }
        std::printf("} & %zu & %zu \\\\\n", sm::offset_of(m).bytes, sm::size_of(ty));
    }
    std::printf("        \\hline\n    \\end{tabular}\n"
                "    \\caption{Downlink packet header (%d B)}\n    \\label{tab:icd7-header}\n\\end{table}\n\n",
                (int)PacketProtocol::HEADER_SIZE);
    std::printf("CRC-16/CCITT-FALSE (polynomial \\texttt{0x1021}, init \\texttt{0xFFFF}) over every\n"
                "byte after the two sync bytes -- the header from \\texttt{version} onward plus the\n"
                "payload -- stored \\textbf{big-endian} in the trailing %d B (the only big-endian\n"
                "field in the frame).\n",
                (int)PacketProtocol::CRC_SIZE);

    // No bandwidth column here either: the rates are being reworked
    std::printf("\n\\subsubsection{Wire Types}\n\n");
    std::printf("\\begin{table}[H]\n    \\centering\\footnotesize\n"
                "    \\begin{tabular}{|l|r|l|r|r|r|p{4.6cm}|}\n        \\hline\n"
                "        \\textbf{Type} & \\textbf{Byte} & \\textbf{Node} & \\textbf{Rate Hz} & "
                "\\textbf{Payl.\\ B} & \\textbf{Wire B} & \\textbf{Description} \\\\\n        \\hline\n");
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view rname = std::string_view(sm::identifier_of(P)).substr(7);
        constexpr PacketProtocol::packet pk = packet_of(P);
        constexpr std::size_t psize = sm::size_of(P);
        std::printf("        \\texttt{%s} & \\texttt{0x%02X} & %s & %d & %zu & %zu & %s \\\\\n", tex(rname).c_str(),
                    static_cast<int>(pk.type), tex(pk.node).c_str(), pk.rate_hz, psize, psize + OVERHEAD,
                    tex(pk.desc).c_str());
    }
    std::printf("        \\hline\n    \\end{tabular}\n"
                "    \\caption{Downlink wire types (wire B = payload + %d B overhead)}\n"
                "    \\label{tab:icd7-wire-types}\n\\end{table}\n",
                OVERHEAD);

    std::printf("\n\\subsubsection{Payload Definitions}\n");
    template for (constexpr sm::info P : PAYLOADS) {
        constexpr std::string_view rname = std::string_view(sm::identifier_of(P)).substr(7);
        constexpr std::size_t psize = sm::size_of(P);
        constexpr PacketProtocol::packet pk = packet_of(P);
        std::printf("\n\\paragraph{%s -- \\texttt{0x%02X} (%zu B)}\n", tex(rname).c_str(), static_cast<int>(pk.type),
                    psize);
        if (pk.rate_hz > 0) {
            std::printf("Emitter \\textbf{%s}, \\SI{%d}{\\hertz} -- %s.\n", tex(pk.node).c_str(), pk.rate_hz,
                        tex(pk.desc).c_str());
        } else {
            std::printf("Emitter \\textbf{%s}, event-driven -- %s.\n", tex(pk.node).c_str(), tex(pk.desc).c_str());
        }
        std::printf("\\begin{center}\\footnotesize\n"
                    "    \\begin{tabular}{|l|l|r|r|l|r|l|p{4.2cm}|}\n        \\hline\n"
                    "        \\textbf{Field} & \\textbf{Type} & \\textbf{Off} & \\textbf{Size} & \\textbf{Unit} & "
                    "\\textbf{Scale} & \\textbf{Gate} & \\textbf{Description} \\\\\n        \\hline\n");
        template for (constexpr sm::info m :
                      std::define_static_array(sm::nonstatic_data_members_of(P, sm::access_context::current()))) {
            constexpr std::string_view fname = sm::identifier_of(m);
            constexpr sm::info ty = sm::type_of(m);
            constexpr std::string_view wt = field_wire(ty);
            constexpr std::size_t cnt = sm::is_array_type(ty) ? sm::extent(ty) : 1;
            constexpr PacketProtocol::wire w = wire_of(m);
            std::printf("        \\texttt{%s} & \\texttt{%.*s", tex(fname).c_str(), (int)wt.size(), wt.data());
            if (cnt > 1) {
                std::printf("[%zu]", cnt);
            }
            std::printf("} & %zu & %zu & %s & \\num{%g} & \\texttt{%s} & %s \\\\\n", sm::offset_of(m).bytes,
                        sm::size_of(ty), tex(w.unit).c_str(), w.scale, tex(w.gate).c_str(), tex(w.desc).c_str());
        }
        std::printf("        \\hline\n    \\end{tabular}\n\\end{center}\n");
    }

    std::printf("\n\\subsubsection{Enumerations}\n\n");
    std::printf("Downlink field enums plus the uplink command set\n"
                "(\\texttt{CommandOpcode}/\\texttt{CommandAckStatus}), so a value on the wire never\n"
                "needs a look in the source. \\textbf{(!)}\\ marks a dangerous/irreversible command;\n"
                "the ground UI gates these behind an arm + confirm step.\n");
    template for (constexpr sm::info E : std::define_static_array(documented_enums())) {
        constexpr std::string_view enm = sm::identifier_of(E);
        std::printf("\n\\paragraph{%s (\\texttt{%s})}\n", tex(enm).c_str(), enum_wire(sm::size_of(E)));
        std::printf("\\begin{center}\\footnotesize\n    \\begin{tabular}{|r|l|p{7.2cm}|}\n        \\hline\n"
                    "        \\textbf{Value} & \\textbf{Name} & \\textbf{Description} \\\\\n        \\hline\n");
        template for (constexpr sm::info c : std::define_static_array(sm::enumerators_of(E))) {
            constexpr std::string_view cn = sm::identifier_of(c);
            constexpr unsigned cv = static_cast<unsigned>(sm::extract<typename[:E:]>(c));
            constexpr PacketProtocol::wire w = wire_of(c);
            std::printf("        \\texttt{0x%02X} & \\texttt{%s}%s & %s \\\\\n", cv, tex(cn).c_str(),
                        w.danger ? "~\\textbf{(!)}" : "", tex(w.desc).c_str());
        }
        std::printf("        \\hline\n    \\end{tabular}\n\\end{center}\n");
    }
}

int main(int argc, char** argv) {
    // Compile-time contracts on the payloads (fail the build, not silently):
    //   - every field is annotated (no forgotten WIRE line)
    //   - packed layout is tight (no padding, fields in wire order)
    template for (constexpr sm::info P : PAYLOADS) {
        static_assert(all_fields_annotated(P), "a payload field is missing its WIRE(...) annotation");
        static_assert(layout_is_tight(P), "a payload has padding/gaps - fields must be contiguous in wire order");
        static_assert(has_packet(P), "a payload struct is missing its PACKET(...) annotation");
    }

    if (argc > 1 && std::string_view(argv[1]) == "icd") {
        emit_icd(argc > 2 ? argv[2] : "unknown");
    } else if (argc > 1 && std::string_view(argv[1]) == "tex") {
        emit_tex(argc > 2 ? argv[2] : "unknown");
    } else {
        emit_json();
    }
    return 0;
}
