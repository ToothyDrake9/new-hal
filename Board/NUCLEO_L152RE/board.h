/**************************************************************************************************
 * @file    board.h
 * @brief   NUCLEO-L152RE: what is connected to which pin, and how the board is clocked
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Everything that depends on the board is here and nowhere in the HAL. Another board is
 *          another copy of this file.
 **************************************************************************************************/
#ifndef BOARD_H_
#define BOARD_H_

#include "hal.h"

/* Green LED LD2. It shares PA5 with the SPI1 clock of the Arduino connector. */
#define BOARD_LED_MODULE     HAL_GPIO_0
#define BOARD_LED_PIN        HAL_GPIO_PIN_5

/* Blue button B1: low when pressed, pulled up on the board. */
#define BOARD_BUTTON_MODULE  HAL_GPIO_2
#define BOARD_BUTTON_PIN     HAL_GPIO_PIN_13

/* Virtual COM port of the ST-LINK: USART2 on PA2 (TX) and PA3 (RX). */
#define BOARD_VCP_UART       HAL_UART_1
#define BOARD_VCP_TX_PIN     { HAL_GPIO_0, HAL_GPIO_PIN_2, HAL_GPIO_FUNCTION__7 }
#define BOARD_VCP_RX_PIN     { HAL_GPIO_0, HAL_GPIO_PIN_3, HAL_GPIO_FUNCTION__7 }

/* SPI1, with the clock on PB3 so that the LED on PA5 stays free. */
#define BOARD_SPI            HAL_SPI_0
#define BOARD_SPI_CLK_PIN    { HAL_GPIO_1, HAL_GPIO_PIN_3, HAL_GPIO_FUNCTION__5 }
#define BOARD_SPI_MISO_PIN   { HAL_GPIO_0, HAL_GPIO_PIN_6, HAL_GPIO_FUNCTION__5 }
#define BOARD_SPI_MOSI_PIN   { HAL_GPIO_0, HAL_GPIO_PIN_7, HAL_GPIO_FUNCTION__5 }

/* I2C1 on the Arduino connector: PB8 (SCL, D15) and PB9 (SDA, D14). */
#define BOARD_I2C            HAL_I2C_0
#define BOARD_I2C_SCL_PIN    { HAL_GPIO_1, HAL_GPIO_PIN_8, HAL_GPIO_FUNCTION__4 }
#define BOARD_I2C_SDA_PIN    { HAL_GPIO_1, HAL_GPIO_PIN_9, HAL_GPIO_FUNCTION__4 }

/* A pin that is not connected, for the optional pins of a module. */
#define BOARD_NO_PIN         { HAL_GPIO_NONE, 0, 0 }

/*
 * 32 MHz from the 8 MHz clock that the ST-LINK provides on the HSE input (bypass):
 * 8 MHz x 8 = 64 MHz in the PLL, divided by 2. Needs voltage range 1.
 * The 32.768 kHz crystal X2 is started for the timers and the RTC.
 */
#define BOARD_CLOCK_FLAGS \
{ \
	.m_ucSource = HAL_CLOCK_SOURCE__PLL, \
	.m_ucVoltage = HAL_CLOCK_VOLTAGE__RANGE_1, \
	.m_ucHse = HAL_CLOCK_OSCILLATOR__BYPASS, \
	.m_ulHseFrequency = 8000000UL, \
	.m_ucPllSource = HAL_CLOCK_SOURCE__HSE, \
	.m_ucPllMultiplier = HAL_CLOCK_PLL_MULTIPLIER__8, \
	.m_ucPllDivider = HAL_CLOCK_PLL_DIVIDER__2, \
	.m_ucAhbPrescaler = HAL_CLOCK_AHB__1, \
	.m_ucApb1Prescaler = HAL_CLOCK_APB__1, \
	.m_ucApb2Prescaler = HAL_CLOCK_APB__1, \
	.m_ucLse = HAL_CLOCK_OSCILLATOR__CRYSTAL \
}

/*
 * 32 MHz without any external clock: 16 MHz HSI x 6 = 96 MHz in the PLL, divided by 3.
 * To fall back on when the board is used without its ST-LINK half.
 */
#define BOARD_CLOCK_FLAGS__INTERNAL \
{ \
	.m_ucSource = HAL_CLOCK_SOURCE__PLL, \
	.m_ucVoltage = HAL_CLOCK_VOLTAGE__RANGE_1, \
	.m_ucPllSource = HAL_CLOCK_SOURCE__HSI, \
	.m_ucPllMultiplier = HAL_CLOCK_PLL_MULTIPLIER__6, \
	.m_ucPllDivider = HAL_CLOCK_PLL_DIVIDER__3, \
	.m_ucAhbPrescaler = HAL_CLOCK_AHB__1, \
	.m_ucApb1Prescaler = HAL_CLOCK_APB__1, \
	.m_ucApb2Prescaler = HAL_CLOCK_APB__1, \
	.m_ucLse = HAL_CLOCK_OSCILLATOR__OFF \
}

#endif /* BOARD_H_ */
