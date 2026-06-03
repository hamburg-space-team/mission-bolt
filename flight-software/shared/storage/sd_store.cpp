#include "sd_store.hpp"
#include "stm32l4xx_hal.h"

namespace {
    constexpr uint32_t SD_WRITE_TIMEOUT_MS = 500U;

    // Wait for SD card to return to TRANSFER state after a write.
    // HAL_SD_WriteBlocks() returns before the card finishes internally.
    bool wait_card_ready(SD_HandleTypeDef* hsd) {
        const uint32_t deadline = HAL_GetTick() + SD_WRITE_TIMEOUT_MS;
        while (HAL_SD_GetCardState(hsd) != HAL_SD_CARD_TRANSFER) {
            if (HAL_GetTick() > deadline) {
                return false;
            }
        }
        return true;
    }

} // namespace

SdStore::SdStore(void* hsd) noexcept : hsd(hsd) {
}

Result<void> SdStore::init() {
    auto* hsd_typed = static_cast<SD_HandleTypeDef*>(this->hsd);

    HAL_SD_CardInfoTypeDef info{};
    if (HAL_SD_GetCardInfo(hsd_typed, &info) != HAL_OK) {
        return std::unexpected(Error::IO_ERROR);
    }

    cfg.context = hsd_typed;
    cfg.read = bd_read;
    cfg.prog = bd_prog;
    cfg.erase = bd_erase;
    cfg.sync = bd_sync;
    cfg.read_size = BLOCK_SIZE;
    cfg.prog_size = BLOCK_SIZE;
    cfg.block_size = BLOCK_SIZE;
    cfg.block_count = info.BlockNbr;
    cfg.block_cycles = BLOCK_CYCLES;
    cfg.cache_size = CACHE_SIZE;
    cfg.lookahead_size = LOOKAHEAD_SIZE;
    cfg.read_buffer = read_buf.data();
    cfg.prog_buffer = prog_buf.data();
    cfg.lookahead_buffer = lookahead_buf.data();

    if (auto r = mount_or_format(); !r) {
        return r;
    }
    if (auto r = open_log(); !r) {
        lfs_unmount(&lfs);
        return r;
    }

    mounted = true;
    return {};
}

Result<void> SdStore::mount_or_format() {
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        // First boot or corrupted filesystem
        err = lfs_format(&lfs, &cfg);
        if (err < 0) {
            return std::unexpected(Error::IO_ERROR);
        }
        err = lfs_mount(&lfs, &cfg);
        if (err < 0) {
            return std::unexpected(Error::IO_ERROR);
        }
    }
    return {};
}

Result<void> SdStore::open_log() {
    file_cfg.buffer = file_buf.data();
    file_cfg.attrs = nullptr;
    file_cfg.attr_count = 0U;

    const int err =
        lfs_file_opencfg(&lfs, &file, LOG_FILENAME.data(), LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND, &file_cfg);
    if (err < 0) {
        return std::unexpected(Error::IO_ERROR);
    }
    file_open = true;
    return {};
}

Result<void> SdStore::write(const uint8_t* data, uint8_t len) {
    if (data == nullptr || len == 0U) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (!mounted || !file_open) {
        return std::unexpected(Error::DISABLED);
    }
    if (lfs_file_write(&lfs, &file, data, len) != static_cast<lfs_ssize_t>(len)) {
        return std::unexpected(Error::IO_ERROR);
    }
    return {};
}

Result<void> SdStore::flush() {
    if (!mounted || !file_open) {
        return std::unexpected(Error::DISABLED);
    }
    if (lfs_file_sync(&lfs, &file) != 0) {
        return std::unexpected(Error::IO_ERROR);
    }
    return {};
}

// --- Block device callbacks -------------------------------------------------
int SdStore::bd_read(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buf, lfs_size_t size) {
    auto* hsd = static_cast<SD_HandleTypeDef*>(c->context);
    const uint32_t sector = static_cast<uint32_t>(block) + static_cast<uint32_t>(off) / BLOCK_SIZE;
    const uint32_t count = size / BLOCK_SIZE;
    if (HAL_SD_ReadBlocks(hsd, static_cast<uint8_t*>(buf), sector, count, HAL_MAX_DELAY) != HAL_OK) {
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

int SdStore::bd_prog(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buf, lfs_size_t size) {
    auto* hsd = static_cast<SD_HandleTypeDef*>(c->context);
    const uint32_t sector = static_cast<uint32_t>(block) + static_cast<uint32_t>(off) / BLOCK_SIZE;
    const uint32_t count = size / BLOCK_SIZE;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto* data = const_cast<uint8_t*>(static_cast<const uint8_t*>(buf));

    if (HAL_SD_WriteBlocks(hsd, data, sector, count, HAL_MAX_DELAY) != HAL_OK) {
        return LFS_ERR_IO;
    }
    if (!wait_card_ready(hsd)) {
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

int SdStore::bd_erase(const lfs_config* c, lfs_block_t block) {
    // SD cards manage erase internally
    (void)c;
    (void)block;
    return LFS_ERR_OK;
}

int SdStore::bd_sync(const lfs_config* c) {
    (void)c;
    return LFS_ERR_OK;
}
