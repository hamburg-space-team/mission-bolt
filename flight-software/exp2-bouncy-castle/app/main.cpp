#include "main.h"
#include "bxcan_transport.hpp"
#include "cmsis_i2c_bus.hpp"
#include "cmsis_uart_link.hpp"
#include "exp2_computer.hpp"
#include "lifi_protocol.hpp"
#include "sd_store.hpp"
#include "timing.hpp"

// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_I2C Driver_I2C1;
// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_USART Driver_USART4; // UART4, PA0 TX / PA1 RX
// NOLINTNEXTLINE(readability-identifier-naming)
extern ARM_DRIVER_USART Driver_USART2; // USART2, PA2 TX / PA3 RX
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;
extern SD_HandleTypeDef hsd1;

namespace {
    // Kick the IWDG every 3 ms (or more) to avoid a reset.
    static void kick_wdg() {
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

    // LiFi transceiver host UARTs (ICD-003). Which of the two connectors
    // carries the sending transceiver is a harness decision that no
    // document records yet: UART4 is taken as the sender. Swap the two
    // driver references below if the bench shows the roles reversed
    constexpr uint8_t LIFI_TX_SLOT = 0U;
    constexpr uint8_t LIFI_RX_SLOT = 1U;

    // The CubeMX project has no NVIC entry for UART4 / USART2 yet, so the
    // interrupt lines are enabled here. Once the .ioc enables them (and
    // stm32l4xx_it.c gains the handlers), this function and the two
    // handlers below must go; the linker reports the duplicate
    static void enable_lifi_uart_irqs() {
        HAL_NVIC_SetPriority(UART4_IRQn, 0U, 0U);
        HAL_NVIC_EnableIRQ(UART4_IRQn);
        HAL_NVIC_SetPriority(USART2_IRQn, 0U, 0U);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
} // namespace

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void UART4_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart4);
}
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart2);
}

extern "C" void app_main(void) {
    static const Platform platform{HAL_Delay, HAL_GetTick, kick_wdg, get_tick_us, led_can_write, led_err_write};
    static CmsisI2CBus i2c{&Driver_I2C1, HAL_GetTick};
    static SdStore storage{&hsd1};
    static BxcanTransport can_transport;
    static CmsisUartLink lifi_tx_link{Driver_USART4, LIFI_TX_SLOT, HAL_GetTick, get_tick_us};
    static CmsisUartLink lifi_rx_link{Driver_USART2, LIFI_RX_SLOT, HAL_GetTick, get_tick_us};

    enable_lifi_uart_irqs();

    // TX/RX pin swap for the straight-through LiFi cabling now lives in the
    // CubeMX config (UART4 + USART2 "Swap TX/RX pins"), so no override here.

    // a rejected bring-up leaves that link silent; the transceiver driver
    // then reports reply timeouts (FAULT LIFI_TX / LIFI_RX), so nothing is
    // lost by not checking here
    (void)lifi_tx_link.init(LifiProtocol::HOST_BAUD);
    (void)lifi_rx_link.init(LifiProtocol::HOST_BAUD);

    static Exp2Computer computer{platform, i2c, storage, can_transport, lifi_tx_link, lifi_rx_link};
    computer.run();
}
