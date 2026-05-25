#pragma once

#include "lfs.h"

#include <array>
#include <cstdint>
#include <string_view>

// Writes are buffered in a 512-byte cache and flushed to the SD card on
// flush(). LittleFS commits metadata atomically, so a power failure at any
// point leaves the filesystem and all previously-flushed packets intact.
class SdStore {
  public:
    // Mount the filesystem (format on first boot or after corruption), then
    // open the log file for appending. Returns 0 on success, negative on error.
    // hsd: pointer to SD handle
    [[nodiscard]] int init(void* hsd);

    // Append raw bytes to the log file cache. Does not hit the SD card unless
    // the internal 512-byte cache overflows - always call flush() at tick end.
    // Returns true on success.
    bool write(const uint8_t* data, uint8_t len);

    // Commit buffered writes to SD (lfs_file_sync). Call once per tick.
    // Power-fail safe: all data written before flush() is guaranteed intact.
    bool flush();

    [[nodiscard]] bool is_mounted() const {
        return mounted;
    }

  private:
    // SD sectors are 512 bytes - read_size = prog_size = block_size = 512.
    static constexpr lfs_size_t BLOCK_SIZE = 512U;
    static constexpr lfs_size_t CACHE_SIZE = 512U;
    static constexpr lfs_size_t LOOKAHEAD_SIZE = 16U; // tracks 128 blocks per pass
    static constexpr int32_t BLOCK_CYCLES = 500;      // wear-levelling hint
    static constexpr std::string_view LOG_FILENAME = "log.bin";

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

    [[nodiscard]] int mount_or_format();
    [[nodiscard]] int open_log();

    // LittleFS block-device callbacks - SD_HandleTypeDef* is recovered via cfg.context.
    static int bd_read(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buf, lfs_size_t size);
    static int bd_prog(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buf, lfs_size_t size);
    static int bd_erase(const lfs_config* c, lfs_block_t block);
    static int bd_sync(const lfs_config* c);
};
