/**************************************************************************************************
 * @file    hal_crc.c
 * @brief   Microprocessor CRC module for the STM32L1 family
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten
 * @remarks The CRC unit of this family is fixed: CRC-32/MPEG-2 (polynomial 0x04C11DB7, initial
 *          value 0xFFFFFFFF, no reflection, no final XOR) and it only takes whole 32 bit words.
 *          Bytes are packed most significant first; the last 1 to 3 bytes are finished in
 *          software with the same algorithm.
 **************************************************************************************************/
#include "hal_crc.h"
#include "hal_calc.h"
#include "hal_private.h"

static uint8_t g_ucHAL_CRC_Configured;

/**************************************************************************************************
 * @brief     CRC configuration
 * @param[in] p_pstFlags - the CRC flags; NULL selects what the hardware offers
 * @return    HAL_SUCCESS, or HAL_ERROR__NOT_SUPPORTED when another polynomial, size or initial
 *            value is asked for
 **************************************************************************************************/
HAL_Return_Code HAL_CRC_Config(const HAL_CRC_Flags *p_pstFlags)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;

	if ((p_pstFlags != NULL) && ((p_pstFlags->m_ulPolynomial != HAL_CRC_POLYNOMIAL__CRC32) || (p_pstFlags->m_ulInitialValue != 0xFFFFFFFFUL) || (p_pstFlags->m_ucSize != 32U)))
	{
		l_eReturnCode = HAL_ERROR__NOT_SUPPORTED;
	}
	else
	{
		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&g_stHAL_CRC_Clock);

		g_ucHAL_CRC_Configured = HAL_ENABLE;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      CRC calculate
 * @param[in]  p_pucData - the data
 * @param[in]  p_unSize - the number of bytes
 * @param[out] p_pulCrc - the CRC-32/MPEG-2 of the data
 * @return     HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks    The unit keeps a running value: do not call from an interrupt handler while the
 *             main code may be inside this function.
 **************************************************************************************************/
HAL_Return_Code HAL_CRC_Calculate(const uint8_t *p_pucData, uint16_t p_unSize, uint32_t *p_pulCrc)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	uint16_t l_unIndex = 0;

	if ((p_pulCrc == NULL) || ((p_pucData == NULL) && (p_unSize != 0U)))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_ucHAL_CRC_Configured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		/* Load the initial value. */
		CRC->CR = CRC_CR_RESET;

		while ((uint16_t) (p_unSize - l_unIndex) >= 4U)
		{
			CRC->DR = ((uint32_t) p_pucData[l_unIndex] << 24) | ((uint32_t) p_pucData[l_unIndex + 1U] << 16) | ((uint32_t) p_pucData[l_unIndex + 2U] << 8) | p_pucData[l_unIndex + 3U];

			l_unIndex = (uint16_t) (l_unIndex + 4U);
		}

		*p_pulCrc = HAL_CALC_Crc32(CRC->DR, &p_pucData[l_unIndex], (uint32_t) p_unSize - l_unIndex);
	}

	return l_eReturnCode;
}
