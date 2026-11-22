/**************************************************************************************************
 * @file    hal_calc.h
 * @brief   Register value calculations that do not touch the hardware
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Kept apart from the drivers so that Tests/host can check them on a PC.
 **************************************************************************************************/
#ifndef HAL_CALC_H_
#define HAL_CALC_H_

#include "typedefs.h"

/**************************************************************************************************
 * @brief      Baud rate divider for 16 times oversampling, rounded to the nearest value
 * @param[in]  p_ulClock - the clock of the UART in Hz (the bus clock, not the system clock)
 * @param[in]  p_ulBaud - the requested baud rate
 * @param[out] p_pulDivider - the value for the baud rate register
 * @return     HAL_SUCCESS, or HAL_ERROR__PARAMETER when the baud rate cannot be produced
 **************************************************************************************************/
static inline HAL_RETURN_CODE_T HAL_CALC_UartDivider(uint32_t p_ulClock, uint32_t p_ulBaud, uint32_t *p_pulDivider)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	uint32_t l_ulDivider;

	if ((p_ulBaud != 0U) && (p_ulBaud <= (p_ulClock / 16U)))
	{
		l_ulDivider = (p_ulClock + (p_ulBaud / 2U)) / p_ulBaud;

		if ((l_ulDivider >= 16U) && (l_ulDivider <= 0xFFFFU))
		{
			*p_pulDivider = l_ulDivider;
			l_eReturnCode = HAL_SUCCESS;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      SPI prescaler: the fastest clock that does not exceed the requested speed
 * @param[in]  p_ulClock - the clock of the SPI in Hz
 * @param[in]  p_ulSpeed - the highest acceptable SPI clock in Hz
 * @param[out] p_pucCode - the baud rate field (0 is clock/2, 7 is clock/256)
 * @param[out] p_pulSpeed - the SPI clock that is obtained in Hz
 * @return     HAL_SUCCESS, or HAL_ERROR__PARAMETER when even clock/256 is too fast
 **************************************************************************************************/
static inline HAL_RETURN_CODE_T HAL_CALC_SpiPrescaler(uint32_t p_ulClock, uint32_t p_ulSpeed, uint8_t *p_pucCode, uint32_t *p_pulSpeed)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	uint8_t l_ucCode;

	for (l_ucCode = 0; l_ucCode < 8U; l_ucCode++)
	{
		if ((p_ulClock >> (l_ucCode + 1U)) <= p_ulSpeed)
		{
			if (p_ulSpeed != 0U)
			{
				*p_pucCode = l_ucCode;
				*p_pulSpeed = p_ulClock >> (l_ucCode + 1U);
				l_eReturnCode = HAL_SUCCESS;
			}

			break;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      I2C timing for the peripheral with CCR and TRISE registers
 * @param[in]  p_ulClock - the clock of the I2C in Hz
 * @param[in]  p_ulSpeed - the requested bus speed in Hz, up to 400000
 * @param[out] p_punFrequency - the FREQ field (clock in MHz)
 * @param[out] p_punClockControl - the CCR register, fast mode bit included
 * @param[out] p_punRiseTime - the TRISE register
 * @return     HAL_SUCCESS, or HAL_ERROR__PARAMETER when the clock is too slow for the speed
 * @remarks    The divider is rounded up, so the bus is never faster than requested.
 **************************************************************************************************/
static inline HAL_RETURN_CODE_T HAL_CALC_I2cTiming(uint32_t p_ulClock, uint32_t p_ulSpeed, uint16_t *p_punFrequency, uint16_t *p_punClockControl, uint16_t *p_punRiseTime)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	uint32_t l_ulFrequency = p_ulClock / 1000000U;
	uint32_t l_ulClockControl;

	if ((p_ulSpeed != 0U) && (p_ulSpeed <= 100000U) && (l_ulFrequency >= 2U) && (l_ulFrequency <= 63U))
	{
		/* Standard mode: the low and the high time are equal. */
		l_ulClockControl = (p_ulClock + (2U * p_ulSpeed) - 1U) / (2U * p_ulSpeed);

		if (l_ulClockControl < 4U)
		{
			l_ulClockControl = 4U;
		}

		if (l_ulClockControl <= 0x0FFFU)
		{
			*p_punFrequency = (uint16_t) l_ulFrequency;
			*p_punClockControl = (uint16_t) l_ulClockControl;
			*p_punRiseTime = (uint16_t) (l_ulFrequency + 1U);          /* 1000 ns */
			l_eReturnCode = HAL_SUCCESS;
		}
	}
	else if ((p_ulSpeed > 100000U) && (p_ulSpeed <= 400000U) && (l_ulFrequency >= 4U) && (l_ulFrequency <= 63U))
	{
		/* Fast mode: the low time is twice the high time. */
		l_ulClockControl = (p_ulClock + (3U * p_ulSpeed) - 1U) / (3U * p_ulSpeed);

		if (l_ulClockControl < 1U)
		{
			l_ulClockControl = 1U;
		}

		if (l_ulClockControl <= 0x0FFFU)
		{
			*p_punFrequency = (uint16_t) l_ulFrequency;
			*p_punClockControl = (uint16_t) (l_ulClockControl | 0x8000U);
			*p_punRiseTime = (uint16_t) (((l_ulFrequency * 300U) / 1000U) + 1U); /* 300 ns */
			l_eReturnCode = HAL_SUCCESS;
		}
	}
	else
	{
		/* Not a valid combination. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      CRC-32/MPEG-2 over bytes (polynomial 0x04C11DB7, no reflection, no final XOR)
 * @param[in]  p_ulCrc - the running value, 0xFFFFFFFF to start
 * @param[in]  p_pucData - the data
 * @param[in]  p_ulSize - the number of bytes
 * @return     The updated CRC
 * @remarks    Same result as the STM32 hardware unit fed with big endian words.
 **************************************************************************************************/
static inline uint32_t HAL_CALC_Crc32(uint32_t p_ulCrc, const uint8_t *p_pucData, uint32_t p_ulSize)
{
	uint32_t l_ulIndex;
	uint8_t l_ucBit;

	for (l_ulIndex = 0; l_ulIndex < p_ulSize; l_ulIndex++)
	{
		p_ulCrc ^= ((uint32_t) p_pucData[l_ulIndex]) << 24;

		for (l_ucBit = 0; l_ucBit < 8U; l_ucBit++)
		{
			p_ulCrc = (p_ulCrc & 0x80000000UL) ? ((p_ulCrc << 1) ^ 0x04C11DB7UL) : (p_ulCrc << 1);
		}
	}

	return p_ulCrc;
}

#endif /* HAL_CALC_H_ */
