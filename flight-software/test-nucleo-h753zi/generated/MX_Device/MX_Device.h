/******************************************************************************
 * File Name   : MX_Device.h
 * Date        : 19/05/2026 17:23:29
 * Description : STM32Cube MX parameter definitions
 * Note        : This file is generated with a generator out of the
 *               STM32CubeMX project and its generated files (DO NOT EDIT!)
 ******************************************************************************/

#ifndef MX_DEVICE_H__
#define MX_DEVICE_H__

/* MX_Device.h version */
#define MX_DEVICE_VERSION                       0x01000000


/*------------------------------ I2C1           -----------------------------*/
#define MX_I2C1                                 1

/* Filter Settings */
#define MX_I2C1_ANF_ENABLE                      1
#define MX_I2C1_DNF                             0

/* Peripheral Clock Frequency */
#define MX_I2C1_PERIPH_CLOCK_FREQ               32000000

/* Pins */

/* I2C1_SCL */
#define MX_I2C1_SCL_Pin                         PB8
#define MX_I2C1_SCL_GPIO_Pin                    GPIO_PIN_8
#define MX_I2C1_SCL_GPIOx                       GPIOB
#define MX_I2C1_SCL_GPIO_Mode                   GPIO_MODE_AF_OD
#define MX_I2C1_SCL_GPIO_PuPd                   GPIO_NOPULL
#define MX_I2C1_SCL_GPIO_Speed                  GPIO_SPEED_FREQ_LOW
#define MX_I2C1_SCL_GPIO_AF                     GPIO_AF4_I2C1

/* I2C1_SDA */
#define MX_I2C1_SDA_Pin                         PB9
#define MX_I2C1_SDA_GPIO_Pin                    GPIO_PIN_9
#define MX_I2C1_SDA_GPIOx                       GPIOB
#define MX_I2C1_SDA_GPIO_Mode                   GPIO_MODE_AF_OD
#define MX_I2C1_SDA_GPIO_PuPd                   GPIO_NOPULL
#define MX_I2C1_SDA_GPIO_Speed                  GPIO_SPEED_FREQ_LOW
#define MX_I2C1_SDA_GPIO_AF                     GPIO_AF4_I2C1

/*------------------------------ USART3         -----------------------------*/
#define MX_USART3                               1

/* Virtual mode */
#define MX_USART3_VM                            VM_ASYNC
#define MX_USART3_VM_ASYNC                      1

/* Pins */

/* USART3_RX */
#define MX_USART3_RX_Pin                        PD9
#define MX_USART3_RX_GPIO_Pin                   GPIO_PIN_9
#define MX_USART3_RX_GPIOx                      GPIOD
#define MX_USART3_RX_GPIO_Mode                  GPIO_MODE_AF_PP
#define MX_USART3_RX_GPIO_PuPd                  GPIO_NOPULL
#define MX_USART3_RX_GPIO_Speed                 GPIO_SPEED_FREQ_LOW
#define MX_USART3_RX_GPIO_AF                    GPIO_AF7_USART3

/* USART3_TX */
#define MX_USART3_TX_Pin                        PD8
#define MX_USART3_TX_GPIO_Pin                   GPIO_PIN_8
#define MX_USART3_TX_GPIOx                      GPIOD
#define MX_USART3_TX_GPIO_Mode                  GPIO_MODE_AF_PP
#define MX_USART3_TX_GPIO_PuPd                  GPIO_NOPULL
#define MX_USART3_TX_GPIO_Speed                 GPIO_SPEED_FREQ_LOW
#define MX_USART3_TX_GPIO_AF                    GPIO_AF7_USART3

#endif  /* MX_DEVICE_H__ */
