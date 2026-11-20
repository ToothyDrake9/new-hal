/**************************************************************************************************
 * @file    hal_lpm.c
 * @brief   Microprocessor Low Power Mode module for the STM32L1 family
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the wake-up flag is cleared by writing 1 to its clear bit, the low-power
 *          run bit is only set under the conditions the reference manual gives for it (slow MSI
 *          clock, voltage range 2), the voltage range is written as a whole field by the clock
 *          module, the clock tree that was running is put back after wake-up
 **************************************************************************************************/
#include "hal_lpm.h"
#include "hal_clock.h"
#include "hal_private.h"

static const HAL_LPM_Flags g_stHAL_LPM_Default;

static HAL_CLOCK_Flags g_stHAL_LPM_SavedClock;
static HAL_LPM_Flags g_stHAL_LPM_RunFlags;
static uint8_t g_ucHAL_LPM_LowPowerRun;

/**************************************************************************************************
 * @brief   Enters the low-power run conditions
 * @return  HAL_SUCCESS, or the error of the clock change
 * @remarks The system clock becomes the MSI at 131 kHz in voltage range 2, then the regulator
 *          goes to low power. Peripheral dividers computed for the previous clock are wrong
 *          until the mode is left.
 **************************************************************************************************/
static HAL_Return_Code HAL_LPM_EnterLowPowerRun(void)
{
	HAL_Return_Code l_eReturnCode;
	HAL_CLOCK_Flags l_stClock;

	HAL_CLOCK_GetFlags(&g_stHAL_LPM_SavedClock);

	l_stClock = g_stHAL_LPM_SavedClock;
	l_stClock.m_ucSource = HAL_CLOCK_SOURCE__MSI;
	l_stClock.m_ucMsiRange = HAL_CLOCK_MSI__131_KHZ;
	l_stClock.m_ucVoltage = HAL_CLOCK_VOLTAGE__RANGE_2;
	l_stClock.m_ucAhbPrescaler = HAL_CLOCK_AHB__1;
	l_stClock.m_ucApb1Prescaler = HAL_CLOCK_APB__1;
	l_stClock.m_ucApb2Prescaler = HAL_CLOCK_APB__1;

	l_eReturnCode = HAL_CLOCK_Update(&l_stClock);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* The low-power deep sleep bit must be set before the low-power run bit. */
		HAL_PRIVATE_Modify(&PWR->CR, 0, PWR_CR_LPSDSR);
		HAL_PRIVATE_Modify(&PWR->CR, 0, PWR_CR_LPRUN);

		g_ucHAL_LPM_LowPowerRun = HAL_ENABLE;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Leaves the low-power run conditions and restores the clock that ran before
 * @return  HAL_SUCCESS, HAL_ERROR__TIMEOUT, or the error of the clock change
 **************************************************************************************************/
static HAL_Return_Code HAL_LPM_ExitLowPowerRun(void)
{
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_Timeout l_stTimeout;

	/* The low-power run bit must be cleared before the low-power deep sleep bit. */
	HAL_PRIVATE_Modify(&PWR->CR, PWR_CR_LPRUN, 0);
	HAL_PRIVATE_Modify(&PWR->CR, PWR_CR_LPSDSR, 0);

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_CLOCK_TIMEOUT_MS);

	/* Wait until the regulator is in main mode. */
	while (PWR->CSR & PWR_CSR_REGLPF)
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;

			break;
		}
	}

	g_ucHAL_LPM_LowPowerRun = HAL_DISABLE;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_CLOCK_Update(&g_stHAL_LPM_SavedClock);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Stops the core until an interrupt arrives
 * @param[in] p_pstFlags - the low power flags
 **************************************************************************************************/
static void HAL_LPM_Sleep(const HAL_LPM_Flags *p_pstFlags)
{
	if (!p_pstFlags->m_ucKeepTick)
	{
		HAL_CLOCK_SetTickState(HAL_DISABLE);
	}

	/* Clear the SLEEPDEEP bit of the Cortex System Control Register. */
	SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

	/* Request Wait For Interrupt. */
	__WFI();

	if (!p_pstFlags->m_ucKeepTick)
	{
		HAL_CLOCK_SetTickState(HAL_ENABLE);
	}
}

/**************************************************************************************************
 * @brief     Enters a low power mode
 * @param[in] p_ucMode - the mode (e.g. HAL_LPM_MODE__STOP)
 * @param[in] p_pstFlags - the low power flags and callbacks; NULL for the defaults
 * @return    HAL_SUCCESS after wake-up (sleep, low-power sleep, stop) or once the mode is active
 *            (low-power run); HAL_ERROR__PARAMETER; HAL_ERROR__BUSY when low-power run is already
 *            active; the error of the clock module when the clock could not be changed or
 *            restored; HAL_ERROR when standby was not entered
 * @remarks   Sleep modes: the 1 ms tick is paused unless m_ucKeepTick is set, so
 *            HAL_CLOCK_GetTick does not count the time slept.
 *            Stop: the clock tree of before is restored before this function returns; the tick
 *            does not count the time stopped. Pending EXTI flags of the wake-up source are the
 *            business of the GPIO module.
 *            Low-power run: returns at once; leave it with HAL_LPM_Exit.
 *            Standby: does not return. After the wake-up reset, HAL_LPM_WasInStandby tells it.
 **************************************************************************************************/
HAL_Return_Code HAL_LPM_Enter(uint8_t p_ucMode, const HAL_LPM_Flags *p_pstFlags)
{
	static const HAL_ClockGate l_stPower = { &RCC->APB1ENR, RCC_APB1ENR_PWREN };
	HAL_Return_Code l_eReturnCode = HAL_SUCCESS;
	HAL_Return_Code l_eExitReturnCode;
	uint32_t l_ulControl;

	if (p_pstFlags == NULL)
	{
		p_pstFlags = &g_stHAL_LPM_Default;
	}

	if (p_ucMode > HAL_LPM_MODE__STANDBY)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (g_ucHAL_LPM_LowPowerRun)
	{
		l_eReturnCode = HAL_ERROR__BUSY;
	}
	else
	{
		/* Enable the power interface clock. */
		HAL_PRIVATE_ClockEnable(&l_stPower);

		if (p_pstFlags->m_pfEnterCallback != NULL)
		{
			p_pstFlags->m_pfEnterCallback(p_pstFlags->m_pvContext);
		}

		switch (p_ucMode)
		{
			case HAL_LPM_MODE__SLEEP:
			{
				HAL_LPM_Sleep(p_pstFlags);

				break;
			}
			case HAL_LPM_MODE__LOW_POWER_RUN:
			{
				g_stHAL_LPM_RunFlags = *p_pstFlags;

				l_eReturnCode = HAL_LPM_EnterLowPowerRun();

				break;
			}
			case HAL_LPM_MODE__LOW_POWER_SLEEP:
			{
				l_eReturnCode = HAL_LPM_EnterLowPowerRun();

				if (l_eReturnCode == HAL_SUCCESS)
				{
					HAL_LPM_Sleep(p_pstFlags);
				}

				l_eExitReturnCode = HAL_LPM_ExitLowPowerRun();

				if (l_eReturnCode == HAL_SUCCESS)
				{
					l_eReturnCode = l_eExitReturnCode;
				}

				break;
			}
			case HAL_LPM_MODE__STOP:
			{
				HAL_CLOCK_GetFlags(&g_stHAL_LPM_SavedClock);

				/* The tick must not wake the core up at once. */
				HAL_CLOCK_SetTickState(HAL_DISABLE);
				SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

				/*
				 * Enter stop mode when the core enters deep sleep, with the regulator in low
				 * power. The wake-up flag is cleared by writing 1 to its clear bit.
				 */
				l_ulControl = PWR->CR & ~(PWR_CR_PDDS | PWR_CR_ULP | PWR_CR_FWU);
				l_ulControl |= PWR_CR_LPSDSR | PWR_CR_CWUF;

				if (p_pstFlags->m_ucUltraLowPower)
				{
					l_ulControl |= PWR_CR_ULP;

					if (p_pstFlags->m_ucFastWakeUp)
					{
						l_ulControl |= PWR_CR_FWU;
					}
				}

				PWR->CR = l_ulControl;

				/* Set the SLEEPDEEP bit of the Cortex System Control Register. */
				SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

				/* Request Wait For Interrupt. */
				__WFI();

				/* A later sleep must not become a stop. */
				SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
				HAL_PRIVATE_Modify(&PWR->CR, PWR_CR_LPSDSR, 0);

				/* The system woke up on the MSI: put the clock tree back. */
				l_eReturnCode = HAL_CLOCK_Update(&g_stHAL_LPM_SavedClock);

				HAL_CLOCK_SetTickState(HAL_ENABLE);

				break;
			}
			default:
			{
				/* Standby. Enable the wake-up pins. */
				l_ulControl = PWR->CSR & ~(PWR_CSR_EWUP1 | PWR_CSR_EWUP2 | PWR_CSR_EWUP3);

				if (p_pstFlags->m_ucWakeUpPins & HAL_LPM_WAKEUP_PIN__1)
				{
					l_ulControl |= PWR_CSR_EWUP1;
				}

				if (p_pstFlags->m_ucWakeUpPins & HAL_LPM_WAKEUP_PIN__2)
				{
					l_ulControl |= PWR_CSR_EWUP2;
				}

				if (p_pstFlags->m_ucWakeUpPins & HAL_LPM_WAKEUP_PIN__3)
				{
					l_ulControl |= PWR_CSR_EWUP3;
				}

				PWR->CSR = l_ulControl;

				HAL_CLOCK_SetTickState(HAL_DISABLE);
				SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

				/* Enter standby mode when the core enters deep sleep, with the wake-up flag cleared. */
				l_ulControl = PWR->CR & ~(PWR_CR_ULP | PWR_CR_FWU);
				l_ulControl |= PWR_CR_PDDS | PWR_CR_CWUF;

				if (p_pstFlags->m_ucUltraLowPower)
				{
					l_ulControl |= PWR_CR_ULP;
				}

				PWR->CR = l_ulControl;

				SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

				__WFI();

				/* Not reached unless an interrupt was pending and standby was not entered. */
				SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
				HAL_PRIVATE_Modify(&PWR->CR, PWR_CR_PDDS, 0);
				HAL_CLOCK_SetTickState(HAL_ENABLE);

				l_eReturnCode = HAL_ERROR;

				break;
			}
		}

		if ((p_ucMode != HAL_LPM_MODE__LOW_POWER_RUN) && (p_pstFlags->m_pfExitCallback != NULL))
		{
			p_pstFlags->m_pfExitCallback(p_pstFlags->m_pvContext);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Leaves a low power mode that does not end by itself
 * @param[in] p_ucMode - HAL_LPM_MODE__LOW_POWER_RUN
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER (another mode), HAL_ERROR__NOT_CONFIGURED (the
 *            mode is not active), HAL_ERROR__TIMEOUT, or the error of the clock module
 **************************************************************************************************/
HAL_Return_Code HAL_LPM_Exit(uint8_t p_ucMode)
{
	HAL_Return_Code l_eReturnCode;

	if (p_ucMode != HAL_LPM_MODE__LOW_POWER_RUN)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_ucHAL_LPM_LowPowerRun)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		l_eReturnCode = HAL_LPM_ExitLowPowerRun();

		if (g_stHAL_LPM_RunFlags.m_pfExitCallback != NULL)
		{
			g_stHAL_LPM_RunFlags.m_pfExitCallback(g_stHAL_LPM_RunFlags.m_pvContext);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Tells if the program restarted because of a wake-up from standby
 * @return  true once after a wake-up from standby; the flag is cleared by the call
 **************************************************************************************************/
bool HAL_LPM_WasInStandby(void)
{
	static const HAL_ClockGate l_stPower = { &RCC->APB1ENR, RCC_APB1ENR_PWREN };
	bool l_bStandby;

	HAL_PRIVATE_ClockEnable(&l_stPower);

	l_bStandby = (PWR->CSR & PWR_CSR_SBF) != 0U;

	if (l_bStandby)
	{
		/* The flags are cleared by writing 1 to their clear bits. */
		HAL_PRIVATE_Modify(&PWR->CR, 0, PWR_CR_CSBF | PWR_CR_CWUF);
	}

	return l_bStandby;
}
