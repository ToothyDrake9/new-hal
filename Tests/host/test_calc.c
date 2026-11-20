/**************************************************************************************************
 * @file    test_calc.c
 * @brief   PC test of the register value calculations of hal_calc.h
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Build and run on the development machine: see Tests/host/CMakeLists.txt.
 **************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "hal_calc.h"

static int g_nFailures;

#define TEST_CHECK(p_condition) \
	do \
	{ \
		if (!(p_condition)) \
		{ \
			printf("FAILED line %d: %s\n", __LINE__, #p_condition); \
			g_nFailures++; \
		} \
	} while (0)

static void TEST_Uart(void)
{
	uint32_t l_ulDivider = 0;

	/* Rounded to nearest: 32 MHz / 115200 is 277.8, 32 MHz / 921600 is 34.7. */
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 115200UL, &l_ulDivider) == HAL_SUCCESS);
	TEST_CHECK(l_ulDivider == 278UL);
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 921600UL, &l_ulDivider) == HAL_SUCCESS);
	TEST_CHECK(l_ulDivider == 35UL);
	TEST_CHECK(HAL_CALC_UartDivider(16000000UL, 9600UL, &l_ulDivider) == HAL_SUCCESS);
	TEST_CHECK(l_ulDivider == 1667UL);

	/* The fastest and the slowest baud rate of a 32 MHz clock. */
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 2000000UL, &l_ulDivider) == HAL_SUCCESS);
	TEST_CHECK(l_ulDivider == 16UL);
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 489UL, &l_ulDivider) == HAL_SUCCESS);
	TEST_CHECK(l_ulDivider == 65440UL);

	/* Out of reach: too fast, too slow, zero. */
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 2000001UL, &l_ulDivider) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 300UL, &l_ulDivider) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_UartDivider(32000000UL, 0UL, &l_ulDivider) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_UartDivider(131072UL, 115200UL, &l_ulDivider) == HAL_ERROR__PARAMETER);
}

static void TEST_Spi(void)
{
	uint32_t l_ulSpeed = 0;
	uint8_t l_ucCode = 0xFF;

	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 16000000UL, &l_ucCode, &l_ulSpeed) == HAL_SUCCESS);
	TEST_CHECK((l_ucCode == 0U) && (l_ulSpeed == 16000000UL));

	/* Never faster than asked: 10 MHz gives 8 MHz, not 16 MHz. */
	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 10000000UL, &l_ucCode, &l_ulSpeed) == HAL_SUCCESS);
	TEST_CHECK((l_ucCode == 1U) && (l_ulSpeed == 8000000UL));

	/* Asking for more than the bus can give returns the fastest setting. */
	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 50000000UL, &l_ucCode, &l_ulSpeed) == HAL_SUCCESS);
	TEST_CHECK((l_ucCode == 0U) && (l_ulSpeed == 16000000UL));

	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 125000UL, &l_ucCode, &l_ulSpeed) == HAL_SUCCESS);
	TEST_CHECK((l_ucCode == 7U) && (l_ulSpeed == 125000UL));

	/* Slower than clock / 256, and zero. */
	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 124999UL, &l_ucCode, &l_ulSpeed) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_SpiPrescaler(32000000UL, 0UL, &l_ucCode, &l_ulSpeed) == HAL_ERROR__PARAMETER);
}

static void TEST_I2c(void)
{
	uint16_t l_unFrequency = 0;
	uint16_t l_unClockControl = 0;
	uint16_t l_unRiseTime = 0;

	/* 32 MHz, 100 kHz: the values the old HAL had hard coded. */
	TEST_CHECK(HAL_CALC_I2cTiming(32000000UL, 100000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_SUCCESS);
	TEST_CHECK((l_unFrequency == 32U) && (l_unClockControl == 160U) && (l_unRiseTime == 33U));

	/* 32 MHz, 400 kHz: 26.7 is rounded up to 27, with the fast mode bit. */
	TEST_CHECK(HAL_CALC_I2cTiming(32000000UL, 400000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_SUCCESS);
	TEST_CHECK((l_unFrequency == 32U) && (l_unClockControl == (0x8000U | 27U)) && (l_unRiseTime == 10U));

	/* Another bus clock gives other values. */
	TEST_CHECK(HAL_CALC_I2cTiming(16000000UL, 100000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_SUCCESS);
	TEST_CHECK((l_unFrequency == 16U) && (l_unClockControl == 80U) && (l_unRiseTime == 17U));

	/* The slowest standard mode divider is 4. */
	TEST_CHECK(HAL_CALC_I2cTiming(2000000UL, 100000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_SUCCESS);
	TEST_CHECK(l_unClockControl == 10U);

	/* Clock too slow, speed too high, speed zero. */
	TEST_CHECK(HAL_CALC_I2cTiming(1000000UL, 100000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_I2cTiming(2097152UL, 400000UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_I2cTiming(32000000UL, 400001UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CALC_I2cTiming(32000000UL, 0UL, &l_unFrequency, &l_unClockControl, &l_unRiseTime) == HAL_ERROR__PARAMETER);
}

static void TEST_Crc(void)
{
	static const uint8_t l_aucCheck[] = "123456789";
	uint32_t l_ulCrc;

	/* The published check value of CRC-32/MPEG-2. */
	TEST_CHECK(HAL_CALC_Crc32(0xFFFFFFFFUL, l_aucCheck, 9) == 0x0376E6E7UL);

	/* Splitting the data anywhere gives the same result: this is how the driver finishes the tail. */
	l_ulCrc = HAL_CALC_Crc32(0xFFFFFFFFUL, l_aucCheck, 8);
	TEST_CHECK(HAL_CALC_Crc32(l_ulCrc, &l_aucCheck[8], 1) == 0x0376E6E7UL);

	TEST_CHECK(HAL_CALC_Crc32(0xFFFFFFFFUL, l_aucCheck, 0) == 0xFFFFFFFFUL);
}

int main(void)
{
	TEST_Uart();
	TEST_Spi();
	TEST_I2c();
	TEST_Crc();

	printf("%s\n", (g_nFailures == 0) ? "All calculation tests passed" : "Calculation tests FAILED");

	return (g_nFailures == 0) ? 0 : 1;
}
