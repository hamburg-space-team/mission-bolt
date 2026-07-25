#include "hal_btc_board.hpp"
#include "can_protocol.hpp"

#include <array>

HalBtcBoard::HalBtcBoard(CAN_HandleTypeDef& hcan_in, RTC_HandleTypeDef& hrtc_in) noexcept
    : hcan(hcan_in), hrtc(hrtc_in) {
}

void HalBtcBoard::send_sync(uint16_t tick, BootState::Mode mode, uint8_t test_target) noexcept {
    CAN_TxHeaderTypeDef hdr{};
    hdr.StdId = CanProtocol::SYNC_ID;
    hdr.DLC = CanProtocol::SYNC_DLC;
    hdr.TransmitGlobalTime = DISABLE;

    const std::array<uint8_t, CanProtocol::SYNC_DLC> data = {
        static_cast<uint8_t>(tick),
        static_cast<uint8_t>(tick >> 8U),
        static_cast<uint8_t>(mode),
        test_target,
    };

    uint32_t mailbox = 0U;
    HAL_CAN_AddTxMessage(&this->hcan, &hdr, data.data(), &mailbox);
}

void HalBtcBoard::broadcast_storage_start() noexcept {
    CAN_TxHeaderTypeDef hdr{};
    hdr.StdId = CanProtocol::STORAGE_START_ID;
    hdr.DLC = CanProtocol::STORAGE_START_DLC;
    hdr.TransmitGlobalTime = DISABLE;

    uint32_t mailbox = 0U;
    const uint8_t placeholder = 0U;
    HAL_CAN_AddTxMessage(&this->hcan, &hdr, &placeholder, &mailbox);
}

void HalBtcBoard::start_can(std::span<const uint32_t> rx_ids) noexcept {
    // One filter bank per RX id - exact ID match into FIFO1 (bxCAN has no
    // range filter). Banks are assigned in id order (bank 0, 1, 2, ...).
    uint8_t bank = 0U;
    for (const uint32_t stid : rx_ids) {
        CAN_FilterTypeDef f{};
        // In the 32-bit filter register STID[10:0] lives in bits [31:21];
        // FilterIdHigh = register[31:16], so STID maps to bits [15:5] there.
        f.FilterIdHigh = static_cast<uint16_t>((stid << 5U) & 0xFFFFU);
        f.FilterIdLow = 0U;
        f.FilterMaskIdHigh = 0xFFFFU; // all bits must match (IDE=0, RTR=0, EXID=0 included)
        f.FilterMaskIdLow = 0xFFFFU;
        f.FilterFIFOAssignment = CAN_RX_FIFO1;
        f.FilterBank = bank;
        f.FilterMode = CAN_FILTERMODE_IDMASK;
        f.FilterScale = CAN_FILTERSCALE_32BIT;
        f.FilterActivation = ENABLE;
        HAL_CAN_ConfigFilter(&this->hcan, &f);
        bank++;
    }

    HAL_CAN_Start(&this->hcan);
    HAL_CAN_ActivateNotification(&this->hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
}

bool HalBtcBoard::lo_asserted() const noexcept {
    return HAL_GPIO_ReadPin(LO_GPIO_Port, LO_Pin) == GPIO_PIN_RESET;
}

bool HalBtcBoard::soe_asserted() const noexcept {
    return HAL_GPIO_ReadPin(SOE_GPIO_Port, SOE_Pin) == GPIO_PIN_RESET;
}

bool HalBtcBoard::sods_asserted() const noexcept {
    return HAL_GPIO_ReadPin(SODS_GPIO_Port, SODS_Pin) == GPIO_PIN_RESET;
}

uint32_t HalBtcBoard::rtc_now_s() const noexcept {
    // GetTime locks the RTC shadow registers until GetDate releases them - the
    // two reads must stay paired.
    RTC_TimeTypeDef t{};
    RTC_DateTypeDef d{};
    HAL_RTC_GetTime(&this->hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&this->hrtc, &d, RTC_FORMAT_BIN);
    return static_cast<uint32_t>(t.Hours) * 3600U + static_cast<uint32_t>(t.Minutes) * 60U +
           static_cast<uint32_t>(t.Seconds);
}
