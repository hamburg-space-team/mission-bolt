#pragma once

#include "packet_builder.hpp"

#include <cstdint>

class Rs422Downlink;
class Store;

/// Owns the "build a BTC-origin packet -> (maybe) send it -> always log it"
/// tail that BtcComputer repeats for every packet it emits
///
/// @ingroup apps
class TelemetryEmitter {
  public:
    TelemetryEmitter(PacketProtocol::PacketBuilder& pkt, uint8_t* tx_buf, Rs422Downlink& downlink,
                     Store& storage) noexcept;

    void emit_boot(PacketProtocol::BootReason reason, uint16_t reboot_count, PacketProtocol::NodeId node) noexcept;
    void emit_gap(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us,
                  const PacketProtocol::PayloadGapMarker& gap) noexcept;
    void emit_cmd_ack(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us, uint8_t opcode, uint8_t seq,
                      PacketProtocol::CommandAckStatus status) noexcept;
    void emit_fault(PacketProtocol::Tick tick, uint8_t fault_code, const Error& err,
                    PacketProtocol::NodeId node) noexcept;
    void emit_timing(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us, PacketProtocol::PayloadType type,
                     const Wcet::Timing& timings) noexcept;

    /// Generic path for the fixed-layout BTC payloads (env / status / imu).
    void emit(PacketProtocol::PayloadType type, PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us,
              const void* payload, uint8_t len) noexcept;

  private:
    //  on a successful build, send the framed packet
    void publish(const Result<uint8_t>& built) noexcept;

    // Disable the BTC downlink for testing
    static constexpr bool DOWNLINK_ENABLED = true;

    PacketProtocol::PacketBuilder& pkt; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    uint8_t* tx_buf;
    Rs422Downlink& downlink; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    Store& storage;          // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};
