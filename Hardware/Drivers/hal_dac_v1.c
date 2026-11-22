/**************************************************************************************************
 * @file    hal_dac_v1.c
 * @brief   Microprocessor DAC module, for the two channel 12 bit DAC
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten
 **************************************************************************************************/
#include "hal_dac.h"
#include "hal_private.h"

/* Bits of channel 1, and how far the bits of channel 2 are from them. */
#define HAL_DAC_CHANNEL_BITS  (DAC_CR_EN1 | DAC_CR_BOFF1 | DAC_CR_TEN1 | DAC_CR_TSEL1 | DAC_CR_WAVE1 | DAC_CR_MAMP1 | DAC_CR_DMAEN1 | DAC_CR_DMAUDRIE1)
#define HAL_DAC_CHANNEL_SHIFT (DAC_CR_EN2_Pos - DAC_CR_EN1_Pos)

static uint8_t g_aucHAL_DAC_Configured[HAL_DAC_NO];

/**************************************************************************************************
 * @brief     DAC configuration
 * @param[in] p_ucModule - the DAC module (e.g. HAL_DAC_0)
 * @param[in] p_pstFlags - the DAC flags and pin
 * @return    HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks   The channel is left disabled with an output value of 0.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DAC_Config(uint8_t p_ucModule, const HAL_DAC_FLAGS_T *p_pstFlags)
{
	static const HAL_GPIO_FLAGS_T l_stPinFlags = { HAL_GPIO_MODE__ANALOG, HAL_GPIO_TYPE__PUSH_PULL, HAL_GPIO_PULL__NO_PULL, HAL_GPIO_SPEED__LOW, 0 };
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	uint32_t l_ulShift;

	if ((p_ucModule < HAL_DAC_NO) && (p_pstFlags != NULL))
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulShift = HAL_DAC_CHANNEL_SHIFT * p_ucModule;

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&g_stHAL_DAC_Clock);

		/* All the settings of the channel are cleared; the buffer bit disables the buffer when set. */
		HAL_PRIVATE_Modify(&DAC->CR, (uint32_t) HAL_DAC_CHANNEL_BITS << l_ulShift, (p_pstFlags->m_ucBuffer ? 0U : DAC_CR_BOFF1) << l_ulShift);

		if (p_ucModule == HAL_DAC_0)
		{
			DAC->DHR12R1 = 0;
		}
		else
		{
			DAC->DHR12R2 = 0;
		}

		g_aucHAL_DAC_Configured[p_ucModule] = HAL_ENABLE;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DAC set state
 * @param[in] p_ucModule - the DAC module (e.g. HAL_DAC_0)
 * @param[in] p_ucState - the DAC state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DAC_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ucModule >= HAL_DAC_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_aucHAL_DAC_Configured[p_ucModule])
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else if (p_ucState)
	{
		HAL_PRIVATE_Modify(&DAC->CR, 0, DAC_CR_EN1 << (HAL_DAC_CHANNEL_SHIFT * p_ucModule));
	}
	else
	{
		HAL_PRIVATE_Modify(&DAC->CR, DAC_CR_EN1 << (HAL_DAC_CHANNEL_SHIFT * p_ucModule), 0);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DAC write
 * @param[in] p_ucModule - the DAC module (e.g. HAL_DAC_0)
 * @param[in] p_unValue - the output value, 0 to HAL_DAC_MAXIMUM
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DAC_Write(uint8_t p_ucModule, uint16_t p_unValue)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if ((p_ucModule >= HAL_DAC_NO) || (p_unValue > HAL_DAC_MAXIMUM))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_aucHAL_DAC_Configured[p_ucModule])
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else if (p_ucModule == HAL_DAC_0)
	{
		DAC->DHR12R1 = p_unValue;
	}
	else
	{
		DAC->DHR12R2 = p_unValue;
	}

	return l_eReturnCode;
}
