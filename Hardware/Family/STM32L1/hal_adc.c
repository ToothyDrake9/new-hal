/**************************************************************************************************
 * @file    hal_adc.c
 * @brief   Microprocessor ADC module for the STM32L1 family
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the HSI oscillator that clocks this converter is requested from the clock
 *          module while the converter is enabled, so a clock source change or a low-power cycle
 *          no longer stops it; every wait has a timeout; delays use the time base and not a
 *          shared SysTick flag
 **************************************************************************************************/
#include "hal_adc.h"
#include "hal_clock.h"
#include "hal_private.h"

typedef struct
{
	HAL_ADC_CALLBACK m_pfConversionCallback;
	void *m_pvContext;
	uint8_t m_ucConfigured;
	uint8_t m_ucEnabled;
}HAL_ADC_Module_State;

static HAL_ADC_Module_State g_astHAL_ADC_State[HAL_ADC_NO];

/**************************************************************************************************
 * @brief     Waits until the masked bits of the status register have the expected value
 * @param[in] p_ulMask - the bits to look at
 * @param[in] p_ulValue - the expected value of these bits
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT
 **************************************************************************************************/
static HAL_Return_Code HAL_ADC_Wait(uint32_t p_ulMask, uint32_t p_ulValue)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_Timeout l_stTimeout;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_ADC_TIMEOUT_MS);

	while ((ADC1->SR & p_ulMask) != p_ulValue)
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;

			break;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Checks the arguments of a conversion
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in] p_ucChannel - the ADC channel
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED (not configured, or
 *            not enabled)
 **************************************************************************************************/
static HAL_Return_Code HAL_ADC_Check(uint8_t p_ucModule, uint8_t p_ucChannel)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;

	if ((p_ucModule >= HAL_ADC_NO) || (p_ucChannel >= HAL_ADC_CHANNEL_NO))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if ((!g_astHAL_ADC_State[p_ucModule].m_ucConfigured) || (!g_astHAL_ADC_State[p_ucModule].m_ucEnabled))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		/* The module can convert. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Starts the conversion of one channel
 * @param[in] p_ucChannel - the ADC channel, already checked
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT
 **************************************************************************************************/
static HAL_Return_Code HAL_ADC_Start(uint8_t p_ucChannel)
{
	/* The sequence may only be written when the regular channel is ready. */
	HAL_Return_Code l_eReturnCode = HAL_ADC_Wait(ADC_SR_RCNR, 0);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* A sequence of one conversion. */
		ADC1->SQR1 = 0;
		ADC1->SQR5 = p_ucChannel;

		ADC1->CR2 |= ADC_CR2_SWSTART;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     ADC configuration
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in] p_pstFlags - the ADC flags and callback
 * @return    HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks   The module is left disabled: call HAL_ADC_ConfigChannel for each channel, then
 *            HAL_ADC_SetState. The ADC clock is the HSI, divided so that it stays within the
 *            limit of the core voltage range that is selected now.
 **************************************************************************************************/
HAL_Return_Code HAL_ADC_Config(uint8_t p_ucModule, const HAL_ADC_Flags *p_pstFlags)
{
	HAL_Return_Code l_eReturnCode = HAL_ERROR__PARAMETER;
	HAL_ADC_Module_State *l_pstState;
	uint32_t l_ulPrescaler = 0;

	if ((p_ucModule < HAL_ADC_NO) && (p_pstFlags != NULL))
	{
		l_pstState = &g_astHAL_ADC_State[p_ucModule];

		if (l_pstState->m_ucEnabled)
		{
			(void) HAL_ADC_SetState(p_ucModule, HAL_DISABLE);
		}

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&g_stHAL_ADC_Clock);

		l_pstState->m_pfConversionCallback = p_pstFlags->m_pfConversionCallback;
		l_pstState->m_pvContext = p_pstFlags->m_pvContext;

		/* ADC clock limit: 16 MHz in voltage range 1, 8 MHz in range 2, 4 MHz in range 3. */
		switch (PWR->CR & PWR_CR_VOS)
		{
			case PWR_CR_VOS_1: l_ulPrescaler = ADC_CCR_ADCPRE_0; break;
			case PWR_CR_VOS:   l_ulPrescaler = ADC_CCR_ADCPRE_1; break;
			default: break;
		}

		ADC1_COMMON->CCR = (ADC1_COMMON->CCR & ~ADC_CCR_ADCPRE) | l_ulPrescaler;

		/* Set the resolution and the alignment. Single conversion, software start. */
		ADC1->CR1 = (uint32_t) p_pstFlags->m_ucResolution << ADC_CR1_RES_Pos;
		ADC1->CR2 = p_pstFlags->m_ucAlign ? ADC_CR2_ALIGN : 0U;

		l_pstState->m_ucConfigured = HAL_ENABLE;

		/* Configure the NVIC interrupt. */
		if (p_pstFlags->m_pfConversionCallback != NULL)
		{
			HAL_PRIVATE_EnableIRQ(ADC1_IRQn, &p_pstFlags->m_stInterrupt);
		}
		else
		{
			NVIC_DisableIRQ(ADC1_IRQn);
		}

		l_eReturnCode = HAL_SUCCESS;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     ADC channel configuration
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in] p_ucChannel - the ADC channel (0 to 26, HAL_ADC_CHANNEL__TEMPERATURE, HAL_ADC_CHANNEL__VREFINT)
 * @param[in] p_pstFlags - the sampling time and the pin
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED or HAL_ERROR__TIMEOUT
 *            (the internal reference did not start)
 * @remarks   Configuring an internal channel switches the temperature sensor and the internal
 *            reference on; they stay on.
 **************************************************************************************************/
HAL_Return_Code HAL_ADC_ConfigChannel(uint8_t p_ucModule, uint8_t p_ucChannel, const HAL_ADC_ChannelFlags *p_pstFlags)
{
	static const HAL_GPIO_Flags l_stPinFlags = { HAL_GPIO_MODE__ANALOG, HAL_GPIO_TYPE__PUSH_PULL, HAL_GPIO_PULL__NO_PULL, HAL_GPIO_SPEED__LOW, 0 };
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	volatile uint32_t *l_pulRegister;
	HAL_CLOCK_Timeout l_stTimeout;
	uint32_t l_ulShift;

	if ((p_ucModule >= HAL_ADC_NO) || (p_ucChannel >= HAL_ADC_CHANNEL_NO) || (p_pstFlags == NULL) || (p_pstFlags->m_ucSampleTime > HAL_ADC_SAMPLE_TIME__384))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_ADC_State[p_ucModule].m_ucConfigured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Set the sampling time: 3 bits per channel, 10 channels per register. */
		if (p_ucChannel < 10U)
		{
			l_pulRegister = &ADC1->SMPR3;
		}
		else if (p_ucChannel < 20U)
		{
			l_pulRegister = &ADC1->SMPR2;
		}
		else
		{
			l_pulRegister = &ADC1->SMPR1;
		}

		l_ulShift = 3UL * (p_ucChannel % 10U);

		HAL_PRIVATE_Modify(l_pulRegister, 0x07UL << l_ulShift, (uint32_t) p_pstFlags->m_ucSampleTime << l_ulShift);

		if ((p_ucChannel == HAL_ADC_CHANNEL__TEMPERATURE) || (p_ucChannel == HAL_ADC_CHANNEL__VREFINT))
		{
			/* Enable the temperature sensor and the internal reference, and let them start. */
			ADC1_COMMON->CCR |= ADC_CCR_TSVREFE;

			HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_ADC_TIMEOUT_MS);

			while (!(PWR->CSR & PWR_CSR_VREFINTRDYF))
			{
				if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
				{
					l_eReturnCode = HAL_ERROR__TIMEOUT;

					break;
				}
			}

			HAL_CLOCK_DelayUs(10);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     ADC set state
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in] p_ucState - the ADC state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED or HAL_ERROR__TIMEOUT
 * @remarks   Enabling requests the HSI oscillator; disabling releases it.
 **************************************************************************************************/
HAL_Return_Code HAL_ADC_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	HAL_ADC_Module_State *l_pstState;

	if (p_ucModule >= HAL_ADC_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_ADC_State[p_ucModule].m_ucConfigured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		l_pstState = &g_astHAL_ADC_State[p_ucModule];

		if (p_ucState && (!l_pstState->m_ucEnabled))
		{
			l_eReturnCode = HAL_CLOCK_RequestHsi();

			if (l_eReturnCode == HAL_SUCCESS)
			{
				ADC1->CR2 |= ADC_CR2_ADON;

				/* Wait until the converter is ready. */
				l_eReturnCode = HAL_ADC_Wait(ADC_SR_ADONS, ADC_SR_ADONS);
			}

			if (l_eReturnCode == HAL_SUCCESS)
			{
				l_pstState->m_ucEnabled = HAL_ENABLE;
			}
			else
			{
				ADC1->CR2 &= ~ADC_CR2_ADON;
				HAL_CLOCK_ReleaseHsi();
			}
		}
		else if ((!p_ucState) && l_pstState->m_ucEnabled)
		{
			ADC1->CR1 &= ~ADC_CR1_EOCSIE;
			ADC1->CR2 &= ~ADC_CR2_ADON;

			l_eReturnCode = HAL_ADC_Wait(ADC_SR_ADONS, 0);

			l_pstState->m_ucEnabled = HAL_DISABLE;
			HAL_CLOCK_ReleaseHsi();
		}
		else
		{
			/* Already in the requested state. */
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      ADC read channel: converts one channel and waits for the result
 * @param[in]  p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in]  p_ucChannel - the ADC channel
 * @param[out] p_punValue - the conversion result
 * @return     HAL_SUCCESS, HAL_ERROR__TIMEOUT, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED
 *             when the module is not configured or not enabled
 **************************************************************************************************/
HAL_Return_Code HAL_ADC_ReadChannelRaw(uint8_t p_ucModule, uint8_t p_ucChannel, uint16_t *p_punValue)
{
	HAL_Return_Code l_eReturnCode = HAL_ADC_Check(p_ucModule, p_ucChannel);

	if ((l_eReturnCode == HAL_SUCCESS) && (p_punValue == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* This conversion is polled. */
		HAL_PRIVATE_Modify(&ADC1->CR1, ADC_CR1_EOCSIE, 0);

		/* Drop a result that nobody read. */
		(void) ADC1->DR;

		l_eReturnCode = HAL_ADC_Start(p_ucChannel);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Wait until the conversion is complete. Reading the data clears the flag. */
		l_eReturnCode = HAL_ADC_Wait(ADC_SR_EOCS, ADC_SR_EOCS);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		*p_punValue = (uint16_t) ADC1->DR;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     ADC start conversion: converts one channel and reports the result to the callback
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 * @param[in] p_ucChannel - the ADC channel
 * @return    HAL_SUCCESS, HAL_ERROR__TIMEOUT, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED
 *            when the module is not configured, not enabled, or has no conversion callback
 **************************************************************************************************/
HAL_Return_Code HAL_ADC_StartConversion(uint8_t p_ucModule, uint8_t p_ucChannel)
{
	HAL_Return_Code l_eReturnCode = HAL_ADC_Check(p_ucModule, p_ucChannel);

	if ((l_eReturnCode == HAL_SUCCESS) && (g_astHAL_ADC_State[p_ucModule].m_pfConversionCallback == NULL))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		(void) ADC1->DR;

		HAL_PRIVATE_Modify(&ADC1->CR1, 0, ADC_CR1_EOCSIE);

		l_eReturnCode = HAL_ADC_Start(p_ucChannel);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     ADC interrupt handling
 * @param[in] p_ucModule - the ADC module (e.g. HAL_ADC_0)
 **************************************************************************************************/
static void HAL_ADC_Interrupt(uint8_t p_ucModule)
{
	const HAL_ADC_Module_State *l_pstState = &g_astHAL_ADC_State[p_ucModule];
	uint16_t l_unValue;

	if ((ADC1->SR & ADC_SR_EOCS) && (ADC1->CR1 & ADC_CR1_EOCSIE))
	{
		/* Reading the data clears the flag. */
		l_unValue = (uint16_t) ADC1->DR;

		if (l_pstState->m_pfConversionCallback != NULL)
		{
			l_pstState->m_pfConversionCallback(l_pstState->m_pvContext, l_unValue);
		}
	}

	if (ADC1->SR & ADC_SR_OVR)
	{
		/* Clear the flag: write 0 to it and 1 to all the others. */
		ADC1->SR = (uint32_t) ~ADC_SR_OVR;
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_ADC
#include "hal_family_irq.h"
