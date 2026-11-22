/**************************************************************************************************
 * @file    hal_timer_v1.c
 * @brief   Microprocessor TIMER module, for the general purpose and basic timers
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the direction is written to the direction bit (not to the enable bit),
 *          flags are cleared without read-modify-write, compare channels are checked against
 *          what each timer has, disabling one interrupt source leaves the others alone
 **************************************************************************************************/
#include "hal_timer.h"
#include "hal_clock.h"
#include "hal_private.h"

typedef struct
{
	HAL_TIMER_CALLBACK_T m_apfCallback[HAL_TIMER_INTERRUPT_NO]; // Index: HAL_TIMER_UPDATE, HAL_TIMER_COMPARE1, ...
	void *m_pvContext;
	uint8_t m_ucConfigured;
}HAL_TIMER_MODULE_STATE_T;

static HAL_TIMER_MODULE_STATE_T g_astHAL_TIMER_State[HAL_TIMER_NO];

/**************************************************************************************************
 * @brief     Checks a module argument
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_TIMER_Check(uint8_t p_ucModule)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ucModule >= HAL_TIMER_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_TIMER_State[p_ucModule].m_ucConfigured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		/* The module can be used. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Checks a compare channel argument
 * @param[in] p_ucModule - the TIMER module, already checked
 * @param[in] p_ucCompare - the compare channel (e.g. HAL_TIMER_COMPARE1)
 * @return    true when this timer has the channel
 **************************************************************************************************/
static bool HAL_TIMER_HasChannel(uint8_t p_ucModule, uint8_t p_ucCompare)
{
	return (p_ucCompare >= HAL_TIMER_COMPARE1) && (p_ucCompare <= g_astHAL_TIMER_Device[p_ucModule].m_ucChannels);
}

/**************************************************************************************************
 * @brief     Gets the register of a compare channel
 * @param[in] p_pstTimer - the timer registers
 * @param[in] p_ucCompare - the compare channel, already checked
 * @return    The compare register
 **************************************************************************************************/
static volatile uint32_t *HAL_TIMER_GetCompareRegister(TIM_TypeDef *p_pstTimer, uint8_t p_ucCompare)
{
	volatile uint32_t *l_pulRegister;

	switch (p_ucCompare)
	{
		case HAL_TIMER_COMPARE1: l_pulRegister = &p_pstTimer->CCR1; break;
		case HAL_TIMER_COMPARE2: l_pulRegister = &p_pstTimer->CCR2; break;
		case HAL_TIMER_COMPARE3: l_pulRegister = &p_pstTimer->CCR3; break;
		default:                 l_pulRegister = &p_pstTimer->CCR4; break;
	}

	return l_pulRegister;
}

/**************************************************************************************************
 * @brief     TIMER configuration
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_pstFlags - the TIMER flags and callbacks
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER (bad argument, period too large for a 16 bit
 *            timer, callback for a channel the timer does not have) or HAL_ERROR__NOT_SUPPORTED
 *            (direction or clock that this timer does not offer)
 * @remarks   The timer is left stopped with all its interrupt sources disabled and all compare
 *            channels unconfigured. The NVIC line is enabled here when a callback is given.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_Config(uint8_t p_ucModule, const HAL_TIMER_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	const HAL_TIMER_DEVICE_T *l_pstDevice = NULL;
	HAL_TIMER_MODULE_STATE_T *l_pstState;
	TIM_TypeDef *l_pstTimer;
	uint32_t l_ulControl = 0;
	uint8_t l_ucIndex;
	bool l_bCallback = false;

	if ((p_ucModule >= HAL_TIMER_NO) || (p_pstFlags == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else
	{
		l_pstDevice = &g_astHAL_TIMER_Device[p_ucModule];
		l_bCallback = (p_pstFlags->m_pfUpdateCallback != NULL);

		if ((!l_pstDevice->m_ucWide) && (p_pstFlags->m_ulUpdate > 0xFFFFUL))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if ((p_pstFlags->m_ucDirection == HAL_TIMER_DIRECTION__DOWN) && l_pstDevice->m_ucUpOnly)
		{
			l_eReturnCode = HAL_ERROR__NOT_SUPPORTED;
		}
		else if ((p_pstFlags->m_ucClock == HAL_TIMER_CLOCK__ALTERNATE) && (l_pstDevice->m_ucLseRoute == HAL_TIMER_LSE__NONE))
		{
			l_eReturnCode = HAL_ERROR__NOT_SUPPORTED;
		}
		else
		{
			for (l_ucIndex = 0; l_ucIndex < 4U; l_ucIndex++)
			{
				if (p_pstFlags->m_apfCompareCallback[l_ucIndex] != NULL)
				{
					l_bCallback = true;

					if (l_ucIndex >= l_pstDevice->m_ucChannels)
					{
						l_eReturnCode = HAL_ERROR__PARAMETER;
					}
				}
			}
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstTimer = l_pstDevice->m_pstRegisters;
		l_pstState = &g_astHAL_TIMER_State[p_ucModule];

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&l_pstDevice->m_stClock);

		/* Stop the timer and disable all its interrupt sources. */
		l_pstTimer->CR1 = 0;
		l_pstTimer->DIER = 0;

		if (l_pstDevice->m_ucChannels != 0U)
		{
			l_pstTimer->CCER = 0;
			l_pstTimer->CCMR1 = 0;
		}

		if (l_pstDevice->m_ucChannels > 2U)
		{
			l_pstTimer->CCMR2 = 0;
		}

		/* Select the timer clock. */
		HAL_FAMILY_SelectTimerLse(l_pstTimer, l_pstDevice->m_ucLseRoute, (p_pstFlags->m_ucClock == HAL_TIMER_CLOCK__ALTERNATE) ? HAL_ENABLE : HAL_DISABLE);

		/* Set the timer direction and the one pulse mode. */
		if (p_pstFlags->m_ucDirection == HAL_TIMER_DIRECTION__DOWN)
		{
			l_ulControl |= TIM_CR1_DIR;
		}

		if (p_pstFlags->m_ucOnePulse)
		{
			l_ulControl |= TIM_CR1_OPM;
		}

		l_pstTimer->CR1 = l_ulControl;

		/* Set the prescaler register and the auto-reload register. */
		l_pstTimer->PSC = p_pstFlags->m_unPrescaler;
		l_pstTimer->ARR = p_pstFlags->m_ulUpdate;

		/* Load the prescaler now (it is otherwise taken at the next update), then clear the flags. */
		l_pstTimer->EGR = TIM_EGR_UG;
		l_pstTimer->SR = 0;

		l_pstState->m_apfCallback[HAL_TIMER_UPDATE] = p_pstFlags->m_pfUpdateCallback;

		for (l_ucIndex = 0; l_ucIndex < 4U; l_ucIndex++)
		{
			l_pstState->m_apfCallback[HAL_TIMER_COMPARE1 + l_ucIndex] = p_pstFlags->m_apfCompareCallback[l_ucIndex];
		}

		l_pstState->m_pvContext = p_pstFlags->m_pvContext;
		l_pstState->m_ucConfigured = HAL_ENABLE;

		/* Configure the NVIC interrupt. */
		if (l_bCallback)
		{
			HAL_PRIVATE_EnableIRQ(l_pstDevice->m_eIRQn, &p_pstFlags->m_stInterrupt);
		}
		else
		{
			NVIC_DisableIRQ(l_pstDevice->m_eIRQn);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER compare channel configuration
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_ucCompare - the compare channel (e.g. HAL_TIMER_COMPARE1)
 * @param[in] p_pstFlags - the compare value, the output mode and the output pin
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks   Call after HAL_TIMER_Config. The compare value takes effect at once.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_ConfigChannel(uint8_t p_ucModule, uint8_t p_ucCompare, const HAL_TIMER_CHANNEL_FLAGS_T *p_pstFlags)
{
	static const HAL_GPIO_FLAGS_T l_stPinFlags = { HAL_GPIO_MODE__FUNCTION, HAL_GPIO_TYPE__PUSH_PULL, HAL_GPIO_PULL__NO_PULL, HAL_GPIO_SPEED__HIGH, 0 };
	static const uint8_t l_aucMode[] = { 0x00, 0x03, 0x06, 0x07 }; // Frozen, toggle, PWM 1, PWM 2
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);
	const HAL_TIMER_DEVICE_T *l_pstDevice = NULL;
	volatile uint32_t *l_pulMode;
	TIM_TypeDef *l_pstTimer;
	uint32_t l_ulState;
	uint32_t l_ulShift;
	uint8_t l_ucChannel;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstDevice = &g_astHAL_TIMER_Device[p_ucModule];

		if ((p_pstFlags == NULL) || (!HAL_TIMER_HasChannel(p_ucModule, p_ucCompare)) || (p_pstFlags->m_ucOutput > HAL_TIMER_OUTPUT__PWM_INVERTED))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if ((!l_pstDevice->m_ucWide) && (p_pstFlags->m_ulCompare > 0xFFFFUL))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if (p_pstFlags->m_ucOutput != HAL_TIMER_OUTPUT__NONE)
		{
			l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stPin, &l_stPinFlags);
		}
		else
		{
			/* No pin to configure. */
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstTimer = l_pstDevice->m_pstRegisters;
		l_ucChannel = (uint8_t) (p_ucCompare - HAL_TIMER_COMPARE1);
		l_pulMode = (l_ucChannel < 2U) ? &l_pstTimer->CCMR1 : &l_pstTimer->CCMR2;
		l_ulShift = 8UL * (l_ucChannel & 1U);

		l_ulState = HAL_PRIVATE_EnterCritical();

		/* Disable the output while it changes. */
		l_pstTimer->CCER &= ~(0x0FUL << (4U * l_ucChannel));

		/* Set the compare register. */
		*HAL_TIMER_GetCompareRegister(l_pstTimer, p_ucCompare) = p_pstFlags->m_ulCompare;

		/* Set the output mode: the channel is an output, without preload. */
		*l_pulMode = (*l_pulMode & ~(0xFFUL << l_ulShift)) | (((uint32_t) l_aucMode[p_pstFlags->m_ucOutput] << 4) << l_ulShift);

		if (p_pstFlags->m_ucOutput != HAL_TIMER_OUTPUT__NONE)
		{
			l_pstTimer->CCER |= 1UL << (4U * l_ucChannel);
		}

		HAL_PRIVATE_ExitCritical(l_ulState);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER set state
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_ucState - the TIMER state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks   Pending events are kept: use HAL_TIMER_SetInterrupt to start from a clean flag.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucState)
		{
			HAL_PRIVATE_Modify(&g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters->CR1, 0, TIM_CR1_CEN);
		}
		else
		{
			HAL_PRIVATE_Modify(&g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters->CR1, TIM_CR1_CEN, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER set interrupt
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_ucInterrupt - the TIMER interrupt (e.g. HAL_TIMER_UPDATE)
 * @param[in] p_ucState - the TIMER interrupt state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED when the module was
 *            not configured or no callback was given for this interrupt
 * @remarks   Enabling a source first clears its stale flag. Only this source changes.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);
	TIM_TypeDef *l_pstTimer;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if ((p_ucInterrupt != HAL_TIMER_UPDATE) && (!HAL_TIMER_HasChannel(p_ucModule, p_ucInterrupt)))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if (g_astHAL_TIMER_State[p_ucModule].m_apfCallback[p_ucInterrupt] == NULL)
		{
			l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
		}
		else
		{
			l_pstTimer = g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters;

			if (p_ucState)
			{
				/* Clear the interrupt flag: write 0 to it and 1 to all the others. */
				l_pstTimer->SR = ~(1UL << p_ucInterrupt);

				HAL_PRIVATE_Modify(&l_pstTimer->DIER, 0, 1UL << p_ucInterrupt);
			}
			else
			{
				HAL_PRIVATE_Modify(&l_pstTimer->DIER, 1UL << p_ucInterrupt, 0);
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER set counter
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_ulCounter - the TIMER counter value
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_SetCounter(uint8_t p_ucModule, uint32_t p_ulCounter)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if ((!g_astHAL_TIMER_Device[p_ucModule].m_ucWide) && (p_ulCounter > 0xFFFFUL))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else
		{
			g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters->CNT = p_ulCounter;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER get counter
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @return    The TIMER counter value; 0 when the module does not exist
 * @remarks   Kept without status code because it is read in timing critical code.
 **************************************************************************************************/
uint32_t HAL_TIMER_GetCounter(uint8_t p_ucModule)
{
	return (p_ucModule < HAL_TIMER_NO) ? g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters->CNT : 0U;
}

/**************************************************************************************************
 * @brief     TIMER set compare
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in] p_ucCompare - HAL_TIMER_UPDATE for the auto-reload value, or the compare channel
 * @param[in] p_ulValue - the value
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks   The value takes effect at once (no preload).
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_SetCompare(uint8_t p_ucModule, uint8_t p_ucCompare, uint32_t p_ulValue)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);
	TIM_TypeDef *l_pstTimer;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstTimer = g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters;

		if ((!g_astHAL_TIMER_Device[p_ucModule].m_ucWide) && (p_ulValue > 0xFFFFUL))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if (p_ucCompare == HAL_TIMER_UPDATE)
		{
			l_pstTimer->ARR = p_ulValue;
		}
		else if (HAL_TIMER_HasChannel(p_ucModule, p_ucCompare))
		{
			*HAL_TIMER_GetCompareRegister(l_pstTimer, p_ucCompare) = p_ulValue;
		}
		else
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      TIMER get compare
 * @param[in]  p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[in]  p_ucCompare - HAL_TIMER_UPDATE for the auto-reload value, or the compare channel
 * @param[out] p_pulValue - the value
 * @return     HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_GetCompare(uint8_t p_ucModule, uint8_t p_ucCompare, uint32_t *p_pulValue)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_Check(p_ucModule);
	TIM_TypeDef *l_pstTimer;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstTimer = g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters;

		if (p_pulValue == NULL)
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if (p_ucCompare == HAL_TIMER_UPDATE)
		{
			*p_pulValue = l_pstTimer->ARR;
		}
		else if (HAL_TIMER_HasChannel(p_ucModule, p_ucCompare))
		{
			*p_pulValue = *HAL_TIMER_GetCompareRegister(l_pstTimer, p_ucCompare);
		}
		else
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      TIMER get clock: the frequency that enters the prescaler
 * @param[in]  p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 * @param[out] p_pulClock - the clock in Hz
 * @return     HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks    This is the bus timer clock. A timer configured with the alternate clock counts
 *             the LSE (32768 Hz) instead.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_TIMER_GetClock(uint8_t p_ucModule, uint32_t *p_pulClock)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;

	if ((p_ucModule < HAL_TIMER_NO) && (p_pulClock != NULL))
	{
		*p_pulClock = HAL_CLOCK_GetTimer(g_astHAL_TIMER_Device[p_ucModule].m_ucBus);
		l_eReturnCode = HAL_SUCCESS;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     TIMER interrupt handling
 * @param[in] p_ucModule - the TIMER module (e.g. HAL_TIMER_0)
 **************************************************************************************************/
static void HAL_TIMER_Interrupt(uint8_t p_ucModule)
{
	const HAL_TIMER_MODULE_STATE_T *l_pstState = &g_astHAL_TIMER_State[p_ucModule];
	TIM_TypeDef *l_pstTimer = g_astHAL_TIMER_Device[p_ucModule].m_pstRegisters;
	uint32_t l_ulPending = l_pstTimer->SR & l_pstTimer->DIER & 0x1FUL;
	uint8_t l_ucInterrupt;

	for (l_ucInterrupt = HAL_TIMER_UPDATE; l_ucInterrupt < HAL_TIMER_INTERRUPT_NO; l_ucInterrupt++)
	{
		if (l_ulPending & (1UL << l_ucInterrupt))
		{
			/* Clear this flag only: write 0 to it and 1 to all the others. */
			l_pstTimer->SR = ~(1UL << l_ucInterrupt);

			if (l_pstState->m_apfCallback[l_ucInterrupt] != NULL)
			{
				l_pstState->m_apfCallback[l_ucInterrupt](l_pstState->m_pvContext);
			}
		}
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_TIMER
#include "hal_family_irq.h"
