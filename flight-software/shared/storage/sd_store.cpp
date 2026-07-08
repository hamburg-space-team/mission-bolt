#include "sd_store.hpp"
#include "stm32l4xx_hal.h"

#include <atomic>
#include <cstring>

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

    // Full card bring-up lives HERE, not in CubeMX's main(): probing the
    // card must be free to fail - no card inserted is a reportable fault
    // (LED code SD, sd_status bit 0 = 0), never an Error_Handler() boot
    // abort. MX_SDMMC1_SD_Init() may be skipped or early-return; this
    // re-initializes the handle from scratch either way.
    if (hsd_typed->State != HAL_SD_STATE_RESET) {
        (void)HAL_SD_DeInit(hsd_typed);
    }
    hsd_typed->Instance = SDMMC1;
    hsd_typed->Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd_typed->Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
    hsd_typed->Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd_typed->Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd_typed->Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd_typed->Init.ClockDiv = 0U;
    if (HAL_SD_Init(hsd_typed) != HAL_OK) {
        return fail(ErrorCode::IO_ERROR, Step::SD_INIT, __LINE__);
    }

    HAL_SD_CardInfoTypeDef info{};
    if (HAL_SD_GetCardInfo(hsd_typed, &info) != HAL_OK) {
        return fail(ErrorCode::IO_ERROR, Step::SD_INIT, __LINE__);
    }

    this->cfg.context = hsd_typed;
    this->cfg.read = bd_read;
    this->cfg.prog = bd_prog;
    this->cfg.erase = bd_erase;
    this->cfg.sync = bd_sync;
    this->cfg.read_size = BLOCK_SIZE;
    this->cfg.prog_size = BLOCK_SIZE;
    this->cfg.block_size = BLOCK_SIZE;
    this->cfg.block_count = info.BlockNbr;
    this->cfg.block_cycles = BLOCK_CYCLES;
    this->cfg.cache_size = CACHE_SIZE;
    this->cfg.lookahead_size = LOOKAHEAD_SIZE;
    this->cfg.read_buffer = this->read_buf.data();
    this->cfg.prog_buffer = this->prog_buf.data();
    this->cfg.lookahead_buffer = this->lookahead_buf.data();

    // mount_or_format / open_log self-identify (SD_MOUNT, SD_OPEN).
    if (auto r = mount_or_format(); !r) {
        return r;
    }
    if (auto r = open_log(); !r) {
        lfs_unmount(&this->lfs);
        return r;
    }

    this->mounted = true;
    return {};
}

Result<void> SdStore::mount_or_format() {
    int err = lfs_mount(&this->lfs, &this->cfg);
    if (err < 0) {
        // First boot or corrupted filesystem
        err = lfs_format(&this->lfs, &this->cfg);
        if (err < 0) {
            return fail(ErrorCode::IO_ERROR, Step::SD_MOUNT, __LINE__);
        }
        err = lfs_mount(&this->lfs, &this->cfg);
        if (err < 0) {
            return fail(ErrorCode::IO_ERROR, Step::SD_MOUNT, __LINE__);
        }
    }
    return {};
}

Result<void> SdStore::open_log() {
    this->file_cfg.buffer = this->file_buf.data();
    this->file_cfg.attrs = nullptr;
    this->file_cfg.attr_count = 0U;

    const int err = lfs_file_opencfg(&this->lfs, &this->file, LOG_FILENAME.data(),
                                     LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND, &this->file_cfg);
    if (err < 0) {
        return fail(ErrorCode::IO_ERROR, Step::SD_OPEN, __LINE__);
    }
    this->file_open = true;
    return {};
}

Result<void> SdStore::write(const uint8_t* data, uint8_t len) {
    // PRODUCER side of the ADR-004 ring buffer. Runs in on_tick() and
    // must complete in microseconds: no SDMMC traffic happens here.
    if (data == nullptr || len == 0U || len > SLOT_BYTES) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::SD_WRITE, __LINE__);
    }
    if (!this->mounted || !this->file_open) {
        return fail(ErrorCode::DISABLED, Step::SD_WRITE, __LINE__);
    }

    const auto head = this->ring_head;
    const auto next = static_cast<uint8_t>((head + 1U) % RING_CAPACITY);
    if (next == this->ring_tail) {
        this->drops = static_cast<uint16_t>(this->drops + 1U);
        return fail(ErrorCode::IO_ERROR, Step::SD_WRITE, __LINE__);
    }

    auto& slot = this->ring[head];
    std::memcpy(slot.data.data(), data, len);
    slot.len = len;

    std::atomic_signal_fence(std::memory_order_release);
    this->ring_head = next;
    return {};
}

bool SdStore::drain_one() {
    if (!this->mounted || !this->file_open) {
        return false;
    }
    const auto tail = this->ring_tail;
    if (tail == this->ring_head) {
        return false; // ring empty
    }

    std::atomic_signal_fence(std::memory_order_acquire);
    auto& slot = this->ring[tail];
    (void)lfs_file_write(&this->lfs, &this->file, slot.data.data(), slot.len);

    this->ring_tail = static_cast<uint8_t>((tail + 1U) % RING_CAPACITY);
    return true;
}

Result<void> SdStore::flush() {
    if (!this->mounted || !this->file_open) {
        return fail(ErrorCode::DISABLED, Step::SD_FLUSH, __LINE__);
    }
    if (lfs_file_sync(&this->lfs, &this->file) != 0) {
        return fail(ErrorCode::IO_ERROR, Step::SD_FLUSH, __LINE__);
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
