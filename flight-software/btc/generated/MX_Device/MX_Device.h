/******************************************************************************
 * File Name   : MX_Device.h
 * Date        : 26/10/2026 19:27:09
 * Description : STM32Cube MX parameter definitions
 * Note        : This file is generated with a generator out of the
 *               STM32CubeMX project and its generated files (DO NOT EDIT!)
 ******************************************************************************/

#ifndef MX_DEVICE_H__
#define MX_DEVICE_H__

/* MX_Device.h version */
#define MX_DEVICE_VERSION                       0x01000000


/*------------------------------ CAN1           -----------------------------*/
#define MX_CAN1                                 1

/* Pins */

/* CAN1_RX */
#define MX_CAN1_RX_Pin                          PB8
#define MX_CAN1_RX_GPIO_Pin                     GPIO_PIN_8
#define MX_CAN1_RX_GPIOx                        GPIOB
#define MX_CAN1_RX_GPIO_Mode                    GPIO_MODE_AF_PP
#define MX_CAN1_RX_GPIO_PuPd                    GPIO_NOPULL
#define MX_CAN1_RX_GPIO_Speed                   GPIO_SPEED_FREQ_VERY_HIGH
#define MX_CAN1_RX_GPIO_AF                      GPIO_AF9_CAN1

/* CAN1_TX */
#define MX_CAN1_TX_Pin                          PB9
#define MX_CAN1_TX_GPIO_Pin                     GPIO_PIN_9
#define MX_CAN1_TX_GPIOx                        GPIOB
#define MX_CAN1_TX_GPIO_Mode                    GPIO_MODE_AF_PP
#define MX_CAN1_TX_GPIO_PuPd                    GPIO_NOPULL
#define MX_CAN1_TX_GPIO_Speed                   GPIO_SPEED_FREQ_VERY_HIGH
#define MX_CAN1_TX_GPIO_AF                      GPIO_AF9_CAN1

/*------------------------------ I2C1           -----------------------------*/
#define MX_I2C1                                 1

/* Filter Settings */
#define MX_I2C1_ANF_ENABLE                      1
#define MX_I2C1_DNF                             0

/* Peripheral Clock Frequency */
#define MX_I2C1_PERIPH_CLOCK_FREQ               4000000

/* Pins */

/* I2C1_SCL */
#define MX_I2C1_SCL_Pin                         PB6
#define MX_I2C1_SCL_GPIO_Pin                    GPIO_PIN_6
#define MX_I2C1_SCL_GPIOx                       GPIOB
#define MX_I2C1_SCL_GPIO_Mode                   GPIO_MODE_AF_OD
#define MX_I2C1_SCL_GPIO_PuPd                   GPIO_NOPULL
#define MX_I2C1_SCL_GPIO_Speed                  GPIO_SPEED_FREQ_VERY_HIGH
#define MX_I2C1_SCL_GPIO_AF                     GPIO_AF4_I2C1

/* I2C1_SDA */
#define MX_I2C1_SDA_Pin                         PB7
#define MX_I2C1_SDA_GPIO_Pin                    GPIO_PIN_7
#define MX_I2C1_SDA_GPIOx                       GPIOB
#define MX_I2C1_SDA_GPIO_Mode                   GPIO_MODE_AF_OD
#define MX_I2C1_SDA_GPIO_PuPd                   GPIO_NOPULL
#define MX_I2C1_SDA_GPIO_Speed                  GPIO_SPEED_FREQ_VERY_HIGH
#define MX_I2C1_SDA_GPIO_AF                     GPIO_AF4_I2C1

/*------------------------------ LPUART1        -----------------------------*/
#define MX_LPUART1                              1

/* Pins */

/* LPUART1_RX */
#define MX_LPUART1_RX_Pin                       PC0
#define MX_LPUART1_RX_GPIO_Pin                  GPIO_PIN_0
#define MX_LPUART1_RX_GPIOx                     GPIOC
#define MX_LPUART1_RX_GPIO_Mode                 GPIO_MODE_AF_PP
#define MX_LPUART1_RX_GPIO_PuPd                 GPIO_NOPULL
#define MX_LPUART1_RX_GPIO_Speed                GPIO_SPEED_FREQ_VERY_HIGH
#define MX_LPUART1_RX_GPIO_AF                   GPIO_AF8_LPUART1

/* LPUART1_TX */
#define MX_LPUART1_TX_Pin                       PC1
#define MX_LPUART1_TX_GPIO_Pin                  GPIO_PIN_1
#define MX_LPUART1_TX_GPIOx                     GPIOC
#define MX_LPUART1_TX_GPIO_Mode                 GPIO_MODE_AF_PP
#define MX_LPUART1_TX_GPIO_PuPd                 GPIO_NOPULL
#define MX_LPUART1_TX_GPIO_Speed                GPIO_SPEED_FREQ_VERY_HIGH
#define MX_LPUART1_TX_GPIO_AF                   GPIO_AF8_LPUART1

#endif  /* MX_DEVICE_H__ */
