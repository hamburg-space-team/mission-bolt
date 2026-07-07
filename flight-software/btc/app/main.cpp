#include "main.h"
#include "btc_computer.hpp"
#include "cmsis_i2c_bus.hpp"
#include "null_store.hpp"
#include "timing.hpp"

// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_I2C Driver_I2C1;
// RS-422 downlink runs on LPUART1 (transceiver wired to PC0/PC1, AF8).
// CMSIS-Driver_STM32 naming: instance 21 = LPUART1 (USART_STM32.c).
// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_USART Driver_USART21;
extern IWDG_HandleTypeDef hiwdg;
extern SD_HandleTypeDef hsd1;

namespace {
    static void kick_wdg() {
        // IWDG runs with the window option (Window == Reload == 300, prescaler
        // 64 -> one counter tick per ~2 ms of LSI): a refresh while the counter
        // still reads 300 -- i.e. within the same IWDG tick as the previous
        // refresh -- resets the MCU ("refreshed too early"). Rate-limit to one
        // reload per 3 ms (> one 2 ms tick even at the slow end of the LSI
        // tolerance, 29.5 kHz) so callers may kick freely.
        static uint32_t last_kick_ms;
        const uint32_t now = HAL_GetTick();
        if ((now - last_kick_ms) < 3U) {
            return;
        }
        last_kick_ms = now;
        HAL_IWDG_Refresh(&hiwdg);
    }
    static uint32_t get_tick_us() {
        return Timing::us_now();
    }

    static void led_can_write(bool on) {
        HAL_GPIO_WritePin(LED_CAN_GPIO_Port, LED_CAN_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    static void led_err_write(bool on) {
        HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
} // namespace

extern "C" void app_main(void) {
    static const Platform plat{HAL_Delay, HAL_GetTick, kick_wdg, get_tick_us, led_can_write, led_err_write};
    static CmsisI2CBus i2c{&Driver_I2C1, HAL_GetTick};
    static NullStore storage{};
    // static SdStore storage{&hsd1};
    static BtcComputer computer{plat, i2c, storage, Driver_USART21};
    computer.run();
}
