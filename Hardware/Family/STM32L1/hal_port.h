/**************************************************************************************************
 * @file    hal_port.h
 * @brief   STM32L1 family: module instances and capabilities visible to the application
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks This is the only family specific header the public HAL headers include. It brings in
 *          the CMSIS device header: registers, addresses and bit names are defined there and
 *          nowhere else. The HAL does not restate them.
 **************************************************************************************************/
#ifndef HAL_PORT_H_
#define HAL_PORT_H_

#include "typedefs.h"
#include "stm32l1xx.h"

/* Capabilities of this family. Modules that are missing return HAL_ERROR__NOT_SUPPORTED. */
#define HAL_HAS_RNG   0
#define HAL_HAS_AES   0   /* STM32L162 only */
#define HAL_HAS_LPTIM 0

enum HAL_GPIO_Module
{
	HAL_GPIO_0 = ((uint8_t) 0x00), // GPIOA
	HAL_GPIO_1,                    // GPIOB
	HAL_GPIO_2,                    // GPIOC
	HAL_GPIO_3,                    // GPIOD
	HAL_GPIO_4,                    // GPIOE
	HAL_GPIO_5,                    // GPIOF
	HAL_GPIO_6,                    // GPIOG
	HAL_GPIO_7,                    // GPIOH

	HAL_GPIO_NO
};

enum HAL_UART_Module
{
	HAL_UART_0 = ((uint8_t) 0x00), // USART1
	HAL_UART_1,                    // USART2
	HAL_UART_2,                    // USART3
	HAL_UART_3,                    // UART4
	HAL_UART_4,                    // UART5

	HAL_UART_NO
};

enum HAL_SPI_Module
{
	HAL_SPI_0 = ((uint8_t) 0x00),  // SPI1
	HAL_SPI_1,                     // SPI2
	HAL_SPI_2,                     // SPI3

	HAL_SPI_NO
};

enum HAL_I2C_Module
{
	HAL_I2C_0 = ((uint8_t) 0x00),  // I2C1
	HAL_I2C_1,                     // I2C2

	HAL_I2C_NO
};

enum HAL_TIMER_Module
{
	HAL_TIMER_0 = ((uint8_t) 0x00), // TIM2  (16 bit, 4 compare channels)
	HAL_TIMER_1,                    // TIM3  (16 bit, 4 compare channels)
	HAL_TIMER_2,                    // TIM4  (16 bit, 4 compare channels)
	HAL_TIMER_3,                    // TIM5  (32 bit, 4 compare channels)
	HAL_TIMER_4,                    // TIM6  (16 bit, no compare channel)
	HAL_TIMER_5,                    // TIM7  (16 bit, no compare channel)
	HAL_TIMER_6,                    // TIM9  (16 bit, 2 compare channels, LSE clock option)
	HAL_TIMER_7,                    // TIM10 (16 bit, 1 compare channel, LSE clock option)
	HAL_TIMER_8,                    // TIM11 (16 bit, 1 compare channel, LSE clock option)

	HAL_TIMER_NO
};

enum HAL_ADC_Module
{
	HAL_ADC_0 = ((uint8_t) 0x00),  // ADC1

	HAL_ADC_NO
};

enum HAL_DAC_Module
{
	HAL_DAC_0 = ((uint8_t) 0x00),  // DAC channel 1 (PA4)
	HAL_DAC_1,                     // DAC channel 2 (PA5)

	HAL_DAC_NO
};

enum HAL_DMA_Module
{
	HAL_DMA_0 = ((uint8_t) 0x00),  // DMA1 (channels 0 to 6)
	HAL_DMA_1,                     // DMA2 (channels 0 to 4)

	HAL_DMA_NO
};

#define HAL_DMA_CHANNEL_NO 7

/* ADC channels that are not connected to a pin. */
#define HAL_ADC_CHANNEL__TEMPERATURE 16
#define HAL_ADC_CHANNEL__VREFINT     17
#define HAL_ADC_CHANNEL_NO           27

/* Flash geometry. */
#define HAL_FLASH_PAGE_SIZE      256U
#define HAL_FLASH_HALF_PAGE_SIZE 128U

/* The GPIO ports, in the order of the HAL_GPIO_Module values. */
#define HAL_GPIO_PORT_TABLE { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH }

#endif /* HAL_PORT_H_ */
