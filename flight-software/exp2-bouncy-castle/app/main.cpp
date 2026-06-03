#include "main.h"
#include "bxcan_transport.hpp"
#include "cmsis_i2c_bus.hpp"
#include "exp2_computer.hpp"
#include "sd_store.hpp"
#include "timing.hpp"

// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_I2C Driver_I2C1;
extern IWDG_HandleTypeDef hiwdg;
extern SD_HandleTypeDef hsd1;

namespace {
    static void kick_wdg() {
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
    static const Platform platform{HAL_Delay, HAL_GetTick, kick_wdg, get_tick_us, led_can_write, led_err_write};
    static CmsisI2CBus i2c{&Driver_I2C1, HAL_GetTick};
    static SdStore storage{&hsd1};
    static BxcanTransport can_transport;
    static Exp2Computer computer{platform, i2c, storage, can_transport};
    computer.run();
}
