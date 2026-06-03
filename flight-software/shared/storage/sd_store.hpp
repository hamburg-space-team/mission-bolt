#pragma once

#include "errors.hpp"
#include "lfs.h"
#include "packet_header.hpp"
#include "store.hpp"

#include <array>
#include <cstdint>
#include <string_view>

/// LittleFS-backed append-only SD log. Sits behind a producer/consumer
/// ring buffer so on_tick() never touches SDMMC directly (ADR-004):
///
///   on_tick() -> write() -> ring buffer in RAM   (microseconds)
///                    |
///                    v
///   framework idle phase -> drain_one() -> lfs_file_write()  (may stall)
///                                                  |
///                                                  v
///   1 Hz cadence + BOOT -> flush() -> lfs_file_sync()  (durability)
///
/// The ring buffer holds RING_CAPACITY slots, each big enough for the
/// full MAX_PACKET_SIZE downlink frame. SD cards stall unpredictably
/// for 50-300 ms during internal flash management; the ring buffer
/// absorbs the produced data during the stall and the drain catches
/// up once the card recovers.
///
/// One SdStore per node (D-120/D-140). BTC additionally mirrors the
/// EXP downlink to its own log (D-130). See ADR-003, ADR-004.
///
/// @ingroup storage
class SdStore final : public Store {
  public:
    /// hsd: pointer to SD_HandleTypeDef. Stored for the lifetime of the
    /// SdStore; the HAL handle must outlive this object (it lives in
    /// CubeMX's main.c as a global).
    explicit SdStore(void* hsd) noexcept;

    /// Mount the filesystem (format on first boot or after corruption),
    /// then open the log file for appending.
    [[nodiscard]] Result<void> init() override;

    /// Producer: enqueue raw bytes for later durable write. Copies the
    /// payload into a ring-buffer slot in microseconds. No SDMMC
    /// traffic. Returns Error::IO_ERROR (and increments dropped_count)
    /// if the ring buffer is full - data is dropped, the tick loop
    /// continues.
    [[nodiscard]] Result<void> write(const uint8_t* data, uint8_t len) override;

    /// Consumer: pop one slot and call lfs_file_write(). May stall up
    /// to the SD-card worst case. Caller (framework idle phase) is
    /// responsible for budget-checking before invoking.
    [[nodiscard]] bool drain_one() override;

    /// Commit cache to SD via lfs_file_sync(). Does NOT drain the ring
    /// buffer first; the on_drain loop is expected to have caught up.
    [[nodiscard]] Result<void> flush() override;

    [[nodiscard]] bool is_mounted() const override {
        return mounted;
    }

    [[nodiscard]] uint16_t dropped_count() const override {
        return drops;
    }

  private:
    // LittleFS / SD geometry
    static constexpr lfs_size_t BLOCK_SIZE = 512U;
    static constexpr lfs_size_t CACHE_SIZE = 512U;
    static constexpr lfs_size_t LOOKAHEAD_SIZE = 16U; // tracks 128 blocks per pass
    static constexpr int32_t BLOCK_CYCLES = 500;      // wear-levelling hint
    static constexpr std::string_view LOG_FILENAME = "log.bin";

    // Producer/consumer ring buffer geometry (ADR-004).
    // 64 slots * 64 B payload + 1 B length each = ~4.2 KB per controller.
    // Sized to absorb a 300 ms SD stall at the EXP3 worst-case production
    // rate (~125 packets/s).
    static constexpr uint8_t RING_CAPACITY = 64U;
    static constexpr uint8_t SLOT_BYTES = static_cast<uint8_t>(PacketProtocol::MAX_PACKET_SIZE);

    struct Slot {
        std::array<uint8_t, SLOT_BYTES> data{};
        uint8_t len = 0U;
    };

    lfs_t lfs{};
    lfs_file_t file{};
    lfs_config cfg{};
    lfs_file_config file_cfg{};

    void* hsd = nullptr; // SD_HandleTypeDef*, cast in sd_store.cpp
    bool mounted = false;
    bool file_open = false;

    // Persistent buffers required by LittleFS (must outlive every lfs call).
    alignas(4) std::array<uint8_t, CACHE_SIZE> read_buf{};
    alignas(4) std::array<uint8_t, CACHE_SIZE> prog_buf{};
    alignas(4) std::array<uint8_t, LOOKAHEAD_SIZE> lookahead_buf{};
    alignas(4) std::array<uint8_t, CACHE_SIZE> file_buf{};

    // Ring-buffer state. head advanced by write() (producer), tail by
    // drain_one() (consumer). Today both run in main-thread context so
    // plain volatile + std::atomic_signal_fence is enough; if we ever
    // migrate to FreeRTOS the fences become real DMB barriers without a
    // code change at the call sites.
    alignas(4) std::array<Slot, RING_CAPACITY> ring{};
    volatile uint8_t ring_head = 0U; // next free slot
    volatile uint8_t ring_tail = 0U; // next slot to drain
    uint16_t drops = 0U;

    [[nodiscard]] Result<void> mount_or_format();
    [[nodiscard]] Result<void> open_log();

    // LittleFS block-device callbacks - SD_HandleTypeDef* recovered via cfg.context.
    static int bd_read(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buf, lfs_size_t size);
    static int bd_prog(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buf, lfs_size_t size);
    static int bd_erase(const lfs_config* c, lfs_block_t block);
    static int bd_sync(const lfs_config* c);
};
