#include "telemetry_emitter.hpp"
#include "board/rs422_downlink.hpp"
#include "store.hpp"

TelemetryEmitter::TelemetryEmitter(PacketProtocol::PacketBuilder& pkt_in, uint8_t* tx_buf_in,
                                   Rs422Downlink& downlink_in, Store& storage_in) noexcept
    : pkt(pkt_in), tx_buf(tx_buf_in), downlink(downlink_in), storage(storage_in) {
}

void TelemetryEmitter::publish(const Result<uint8_t>& built) noexcept {
    if (!built) {
        return;
    }
    if constexpr (DOWNLINK_ENABLED) {
        downlink.send(this->tx_buf, *built);
    }
    (void)this->storage.write(this->tx_buf, *built);
}

void TelemetryEmitter::emit_boot(PacketProtocol::BootReason reason, uint16_t reboot_count,
                                 PacketProtocol::NodeId node) noexcept {
    publish(this->pkt.build_boot(this->tx_buf, reason, reboot_count, node));
}

void TelemetryEmitter::emit_gap(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us,
                                const PacketProtocol::PayloadGapMarker& gap) noexcept {
    publish(this->pkt.build_gap(this->tx_buf, tick, ts_us, gap));
}

void TelemetryEmitter::emit_cmd_ack(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us, uint8_t opcode,
                                    uint8_t seq, PacketProtocol::CommandAckStatus status) noexcept {
    publish(this->pkt.build_cmd_ack(this->tx_buf, tick, ts_us, opcode, seq, status));
}

void TelemetryEmitter::emit_fault(PacketProtocol::Tick tick, uint8_t fault_code, const Error& err,
                                  PacketProtocol::NodeId node) noexcept {
    publish(this->pkt.build_fault(this->tx_buf, tick, fault_code, err, node));
}

void TelemetryEmitter::emit_timing(PacketProtocol::Tick tick, PacketProtocol::TimestampUs ts_us,
                                   PacketProtocol::PayloadType type, const Wcet::Timing& timings) noexcept {
    publish(this->pkt.build_timing(this->tx_buf, type, tick, ts_us, timings));
}

void TelemetryEmitter::emit(PacketProtocol::PayloadType type, PacketProtocol::Tick tick,
                            PacketProtocol::TimestampUs ts_us, const void* payload, uint8_t len) noexcept {
    publish(this->pkt.build(this->tx_buf, type, tick, ts_us, payload, len));
}
