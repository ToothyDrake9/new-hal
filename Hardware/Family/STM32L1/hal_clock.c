/**************************************************************************************************
 * @file    hal_clock.c
 * @brief   Microprocessor clock module for the STM32L1 family
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the core voltage range is selected and checked before the frequency rises,
 *          the configuration is validated against the limits of the range, every oscillator wait
 *          has a timeout and leaves the system running from the MSI on failure, the HSI is kept
 *          alive while a module (the ADC) needs it
 **************************************************************************************************/
#include "hal_clock.h"
#include "hal_private.h"

/* The frequency used between two configurations: valid in every voltage range, without wait state. */
#define HAL_CLOCK_TRANSIT_RANGE HAL_CLOCK_MSI__2097_KHZ

/* The slowest core clock that still gives a usable millisecond time base. */
#define HAL_CLOCK_MINIMUM 32000UL

static const uint32_t g_aulHAL_CLOCK_SysclkLimit[] = HAL_FAMILY_SYSCLK_LIMITS;
static const uint32_t g_aulHAL_CLOCK_VcoLimit[] = HAL_FAMILY_PLL_VCO_LIMITS;
static const uint32_t g_aulHAL_CLOCK_NoWaitLimit[] = HAL_FAMILY_FLASH_0WS_LIMITS;
static const uint8_t g_aucHAL_CLOCK_Multiplier[] = { 3, 4, 6, 8, 12, 16, 24, 32, 48 };
static const uint8_t g_aucHAL_CLOCK_AhbShift[] = { 0, 1, 2, 3, 4, 6, 7, 8, 9 };

/* Register values of the settings, in the order of the HAL enumerations. Used in both directions:
 * to program a setting and to find out which setting is programmed. */
static const uint32_t g_aulHAL_CLOCK_VoltageCode[] = { PWR_CR_VOS_0, PWR_CR_VOS_1, PWR_CR_VOS };
static const uint32_t g_aulHAL_CLOCK_MsiCode[] = { RCC_ICSCR_MSIRANGE_0, RCC_ICSCR_MSIRANGE_1, RCC_ICSCR_MSIRANGE_2, RCC_ICSCR_MSIRANGE_3, RCC_ICSCR_MSIRANGE_4, RCC_ICSCR_MSIRANGE_5, RCC_ICSCR_MSIRANGE_6 };
static const uint32_t g_aulHAL_CLOCK_MultiplierCode[] = { RCC_CFGR_PLLMUL3, RCC_CFGR_PLLMUL4, RCC_CFGR_PLLMUL6, RCC_CFGR_PLLMUL8, RCC_CFGR_PLLMUL12, RCC_CFGR_PLLMUL16, RCC_CFGR_PLLMUL24, RCC_CFGR_PLLMUL32, RCC_CFGR_PLLMUL48 };
static const uint32_t g_aulHAL_CLOCK_DividerCode[] = { RCC_CFGR_PLLDIV2, RCC_CFGR_PLLDIV3, RCC_CFGR_PLLDIV4 };
static const uint32_t g_aulHAL_CLOCK_AhbCode[] = { RCC_CFGR_HPRE_DIV1, RCC_CFGR_HPRE_DIV2, RCC_CFGR_HPRE_DIV4, RCC_CFGR_HPRE_DIV8, RCC_CFGR_HPRE_DIV16, RCC_CFGR_HPRE_DIV64, RCC_CFGR_HPRE_DIV128, RCC_CFGR_HPRE_DIV256, RCC_CFGR_HPRE_DIV512 };
static const uint32_t g_aulHAL_CLOCK_Apb1Code[] = { RCC_CFGR_PPRE1_DIV1, RCC_CFGR_PPRE1_DIV2, RCC_CFGR_PPRE1_DIV4, RCC_CFGR_PPRE1_DIV8, RCC_CFGR_PPRE1_DIV16 };
static const uint32_t g_aulHAL_CLOCK_Apb2Code[] = { RCC_CFGR_PPRE2_DIV1, RCC_CFGR_PPRE2_DIV2, RCC_CFGR_PPRE2_DIV4, RCC_CFGR_PPRE2_DIV8, RCC_CFGR_PPRE2_DIV16 };

#define HAL_CLOCK_COUNT(p_aTable) ((uint8_t) (sizeof(p_aTable) / sizeof((p_aTable)[0])))

/* CMSIS variable: the core clock (HCLK) in Hz. */
uint32_t SystemCoreClock = 2097152UL;

static HAL_CLOCK_FLAGS_T g_stHAL_CLOCK_Flags;
static uint32_t g_ulHAL_CLOCK_HseFrequency;
static uint32_t g_ulHAL_CLOCK_TicksPerMs = 1;
static volatile uint32_t g_ulHAL_CLOCK_Tick;
static volatile uint8_t g_ucHAL_CLOCK_HsiRequests;
static uint8_t g_ucHAL_CLOCK_TickState;

/**************************************************************************************************
 * @brief     Waits until the masked bits of a register have the expected value
 * @param[in] p_pulRegister - the register
 * @param[in] p_ulMask - the bits to look at
 * @param[in] p_ulValue - the expected value of these bits
 * @param[in] p_ulMilliseconds - the longest wait
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_Wait(const volatile uint32_t *p_pulRegister, uint32_t p_ulMask, uint32_t p_ulValue, uint32_t p_ulMilliseconds)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, p_ulMilliseconds);

	while ((*p_pulRegister & p_ulMask) != p_ulValue)
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			/* Look one last time: the condition may have become true while the timeout ran out. */
			if ((*p_pulRegister & p_ulMask) != p_ulValue)
			{
				l_eReturnCode = HAL_ERROR__TIMEOUT;
			}

			break;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Finds which setting a register field holds
 * @param[in] p_pulTable - the register values of the settings
 * @param[in] p_ucSize - the number of settings
 * @param[in] p_ulCode - the value read from the field
 * @return    The index of the setting; 0 when the value is not in the table (the prescaler fields
 *            have several values for "not divided")
 **************************************************************************************************/
static uint8_t HAL_CLOCK_Decode(const uint32_t *p_pulTable, uint8_t p_ucSize, uint32_t p_ulCode)
{
	uint8_t l_ucIndex = 0;
	uint8_t l_ucSearch;

	for (l_ucSearch = 0; l_ucSearch < p_ucSize; l_ucSearch++)
	{
		if (p_pulTable[l_ucSearch] == p_ulCode)
		{
			l_ucIndex = l_ucSearch;

			break;
		}
	}

	return l_ucIndex;
}

/**************************************************************************************************
 * @brief     Finds which prescaler setting an APB bus has
 * @param[in] p_ucBus - HAL_CLOCK_BUS__APB1 or HAL_CLOCK_BUS__APB2
 * @return    The index of the setting: the bus clock is the core clock shifted right by it
 **************************************************************************************************/
static uint8_t HAL_CLOCK_GetApbPrescaler(uint8_t p_ucBus)
{
	uint8_t l_ucIndex;

	if (p_ucBus == HAL_CLOCK_BUS__APB1)
	{
		l_ucIndex = HAL_CLOCK_Decode(g_aulHAL_CLOCK_Apb1Code, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_Apb1Code), RCC->CFGR & RCC_CFGR_PPRE1);
	}
	else
	{
		l_ucIndex = HAL_CLOCK_Decode(g_aulHAL_CLOCK_Apb2Code, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_Apb2Code), RCC->CFGR & RCC_CFGR_PPRE2);
	}

	return l_ucIndex;
}

/**************************************************************************************************
 * @brief   Recomputes the core clock from the registers and reprograms the time base
 **************************************************************************************************/
static void HAL_CLOCK_Refresh(void)
{
	uint32_t l_ulClock = HAL_CLOCK_GetBus(HAL_CLOCK_BUS__AHB);

	SystemCoreClock = l_ulClock;

	g_ulHAL_CLOCK_TicksPerMs = l_ulClock / 1000U;

	if (g_ulHAL_CLOCK_TicksPerMs == 0U)
	{
		g_ulHAL_CLOCK_TicksPerMs = 1;
	}

	SysTick->LOAD = g_ulHAL_CLOCK_TicksPerMs - 1U;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk | (g_ucHAL_CLOCK_TickState ? SysTick_CTRL_TICKINT_Msk : 0U);
}

/**************************************************************************************************
 * @brief   Tells if the HSI oscillator feeds the system clock
 * @return  true when the HSI is the system clock or the PLL input
 **************************************************************************************************/
static bool HAL_CLOCK_IsHsiUsed(void)
{
	uint32_t l_ulSource = RCC->CFGR & RCC_CFGR_SWS;

	return (l_ulSource == RCC_CFGR_SWS_HSI) || ((l_ulSource == RCC_CFGR_SWS_PLL) && ((RCC->CFGR & RCC_CFGR_PLLSRC) == 0U));
}

/**************************************************************************************************
 * @brief      Checks a clock configuration against the limits of the microcontroller
 * @param[in]  p_pstFlags - the clock flags
 * @param[out] p_pulCoreClock - the core clock (HCLK) that the configuration produces, in Hz
 * @return     HAL_SUCCESS or HAL_ERROR__PARAMETER
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_Validate(const HAL_CLOCK_FLAGS_T *p_pstFlags, uint32_t *p_pulCoreClock)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulSystemClock = 0;
	uint32_t l_ulInput = 0;
	bool l_bHseUsed = false;

	if ((p_pstFlags->m_ucVoltage > HAL_CLOCK_VOLTAGE__RANGE_3) || (p_pstFlags->m_ucAhbPrescaler > HAL_CLOCK_AHB__512)
		|| (p_pstFlags->m_ucApb1Prescaler > HAL_CLOCK_APB__16) || (p_pstFlags->m_ucApb2Prescaler > HAL_CLOCK_APB__16)
		|| (p_pstFlags->m_ucHse > HAL_CLOCK_OSCILLATOR__BYPASS) || (p_pstFlags->m_ucLse > HAL_CLOCK_OSCILLATOR__BYPASS))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else
	{
		switch (p_pstFlags->m_ucSource)
		{
			case HAL_CLOCK_SOURCE__MSI:
			{
				if (p_pstFlags->m_ucMsiRange > HAL_CLOCK_MSI__4194_KHZ)
				{
					l_eReturnCode = HAL_ERROR__PARAMETER;
				}
				else
				{
					l_ulSystemClock = 65536UL << p_pstFlags->m_ucMsiRange;
				}

				break;
			}
			case HAL_CLOCK_SOURCE__HSI:
			{
				l_ulSystemClock = HAL_FAMILY_HSI_FREQUENCY;

				break;
			}
			case HAL_CLOCK_SOURCE__HSE:
			{
				l_ulSystemClock = p_pstFlags->m_ulHseFrequency;
				l_bHseUsed = true;

				break;
			}
			case HAL_CLOCK_SOURCE__PLL:
			{
				if ((p_pstFlags->m_ucPllMultiplier > HAL_CLOCK_PLL_MULTIPLIER__48) || (p_pstFlags->m_ucPllDivider > HAL_CLOCK_PLL_DIVIDER__4))
				{
					l_eReturnCode = HAL_ERROR__PARAMETER;
				}
				else if (p_pstFlags->m_ucPllSource == HAL_CLOCK_SOURCE__HSI)
				{
					l_ulInput = HAL_FAMILY_HSI_FREQUENCY;
				}
				else if (p_pstFlags->m_ucPllSource == HAL_CLOCK_SOURCE__HSE)
				{
					l_ulInput = p_pstFlags->m_ulHseFrequency;
					l_bHseUsed = true;
				}
				else
				{
					l_eReturnCode = HAL_ERROR__PARAMETER;
				}

				if (l_eReturnCode == HAL_SUCCESS)
				{
					if ((l_ulInput < HAL_FAMILY_PLL_INPUT_MINIMUM) || (l_ulInput > HAL_FAMILY_PLL_INPUT_MAXIMUM))
					{
						l_eReturnCode = HAL_ERROR__PARAMETER;
					}
					else
					{
						/* At most 24 MHz x 48, which fits in 32 bits. */
						l_ulSystemClock = l_ulInput * g_aucHAL_CLOCK_Multiplier[p_pstFlags->m_ucPllMultiplier];

						if (l_ulSystemClock > g_aulHAL_CLOCK_VcoLimit[p_pstFlags->m_ucVoltage])
						{
							l_eReturnCode = HAL_ERROR__PARAMETER;
						}

						l_ulSystemClock /= (uint32_t) p_pstFlags->m_ucPllDivider + 2U;
					}
				}

				break;
			}
			default:
			{
				l_eReturnCode = HAL_ERROR__PARAMETER;

				break;
			}
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (l_bHseUsed && ((p_pstFlags->m_ucHse == HAL_CLOCK_OSCILLATOR__OFF) || (p_pstFlags->m_ulHseFrequency < 1000000UL) || (p_pstFlags->m_ulHseFrequency > 32000000UL)))
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if (l_ulSystemClock > g_aulHAL_CLOCK_SysclkLimit[p_pstFlags->m_ucVoltage])
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else if ((l_ulSystemClock >> g_aucHAL_CLOCK_AhbShift[p_pstFlags->m_ucAhbPrescaler]) < HAL_CLOCK_MINIMUM)
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
		else
		{
			*p_pulCoreClock = l_ulSystemClock >> g_aucHAL_CLOCK_AhbShift[p_pstFlags->m_ucAhbPrescaler];
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Switches the system to the MSI at 2.097 MHz with all prescalers at 1
 * @return  HAL_SUCCESS or HAL_ERROR__TIMEOUT
 * @remarks From this state the voltage range, the flash latency and the other oscillators can be
 *          changed in any order.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_EnterTransit(void)
{
	HAL_RETURN_CODE_T l_eReturnCode;

	/* Enable the MSI. Its range may only be changed while it is off or ready. */
	RCC->CR |= RCC_CR_MSION;
	l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_MSIRDY, RCC_CR_MSIRDY, HAL_CLOCK_TIMEOUT_MS);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		RCC->ICSCR = (RCC->ICSCR & ~RCC_ICSCR_MSIRANGE) | g_aulHAL_CLOCK_MsiCode[HAL_CLOCK_TRANSIT_RANGE];
		l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_MSIRDY, RCC_CR_MSIRDY, HAL_CLOCK_TIMEOUT_MS);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Set the MSI as the clock source. */
		RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_MSI;
		l_eReturnCode = HAL_CLOCK_Wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_MSI, HAL_CLOCK_TIMEOUT_MS);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

		g_stHAL_CLOCK_Flags.m_ucSource = HAL_CLOCK_SOURCE__MSI;
		g_stHAL_CLOCK_Flags.m_ucMsiRange = HAL_CLOCK_TRANSIT_RANGE;
		g_stHAL_CLOCK_Flags.m_ucAhbPrescaler = HAL_CLOCK_AHB__1;
		g_stHAL_CLOCK_Flags.m_ucApb1Prescaler = HAL_CLOCK_APB__1;
		g_stHAL_CLOCK_Flags.m_ucApb2Prescaler = HAL_CLOCK_APB__1;
	}

	HAL_CLOCK_Refresh();

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Selects the core voltage range
 * @param[in] p_ucVoltage - the range (e.g. HAL_CLOCK_VOLTAGE__RANGE_1)
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT
 * @remarks   The field is written as a whole (01 range 1, 10 range 2, 11 range 3) and the regulator
 *            is given the time to settle, before and after.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_SetVoltage(uint8_t p_ucVoltage)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_CLOCK_Wait(&PWR->CSR, PWR_CSR_VOSF, 0, HAL_CLOCK_TIMEOUT_MS);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		HAL_PRIVATE_Modify(&PWR->CR, PWR_CR_VOS, g_aulHAL_CLOCK_VoltageCode[p_ucVoltage]);

		l_eReturnCode = HAL_CLOCK_Wait(&PWR->CSR, PWR_CSR_VOSF, 0, HAL_CLOCK_TIMEOUT_MS);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Sets the flash access time for a core clock
 * @param[in] p_ulCoreClock - the core clock in Hz
 * @param[in] p_ucVoltage - the core voltage range
 * @return    HAL_SUCCESS, or HAL_ERROR when the flash interface refuses the setting
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_SetLatency(uint32_t p_ulCoreClock, uint8_t p_ucVoltage)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ulCoreClock > g_aulHAL_CLOCK_NoWaitLimit[p_ucVoltage])
	{
		/* One wait state. The 64 bit access must be enabled first. */
		FLASH->ACR |= FLASH_ACR_ACC64;

		if (!(FLASH->ACR & FLASH_ACR_ACC64))
		{
			l_eReturnCode = HAL_ERROR;
		}
		else
		{
			FLASH->ACR |= FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN;

			if (!(FLASH->ACR & FLASH_ACR_LATENCY))
			{
				l_eReturnCode = HAL_ERROR;
			}
		}
	}
	else
	{
		/* No wait state. The 64 bit access must be disabled last. */
		FLASH->ACR &= ~(FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN);
		FLASH->ACR &= ~FLASH_ACR_ACC64;

		if (FLASH->ACR & FLASH_ACR_LATENCY)
		{
			l_eReturnCode = HAL_ERROR;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Starts the HSE oscillator
 * @param[in] p_ucMode - HAL_CLOCK_OSCILLATOR__CRYSTAL or HAL_CLOCK_OSCILLATOR__BYPASS
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT (no crystal, no external clock)
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_StartHse(uint8_t p_ucMode)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulBypass = (p_ucMode == HAL_CLOCK_OSCILLATOR__BYPASS) ? RCC_CR_HSEBYP : 0U;

	if (((RCC->CR & (RCC_CR_HSEON | RCC_CR_HSERDY)) != (RCC_CR_HSEON | RCC_CR_HSERDY)) || ((RCC->CR & RCC_CR_HSEBYP) != l_ulBypass))
	{
		/* The bypass bit may only be written while the oscillator is off. */
		RCC->CR &= ~RCC_CR_HSEON;
		l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_HSERDY, 0, HAL_CLOCK_TIMEOUT_MS);

		if (l_eReturnCode == HAL_SUCCESS)
		{
			RCC->CR = (RCC->CR & ~RCC_CR_HSEBYP) | l_ulBypass;
			RCC->CR |= RCC_CR_HSEON;

			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY, HAL_CLOCK_HSE_TIMEOUT_MS);

			if (l_eReturnCode != HAL_SUCCESS)
			{
				RCC->CR &= ~RCC_CR_HSEON;
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Configures the LSE oscillator
 * @param[in] p_ucMode - HAL_CLOCK_OSCILLATOR__OFF, __CRYSTAL or __BYPASS
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT (no 32.768 kHz crystal)
 * @remarks   An oscillator that already runs in the requested mode is left alone, so the RTC is
 *            not disturbed.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_ConfigLse(uint8_t p_ucMode)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulBypass = (p_ucMode == HAL_CLOCK_OSCILLATOR__BYPASS) ? RCC_CSR_LSEBYP : 0U;
	bool l_bRunning = (RCC->CSR & (RCC_CSR_LSEON | RCC_CSR_LSERDY)) == (RCC_CSR_LSEON | RCC_CSR_LSERDY);

	if (p_ucMode == HAL_CLOCK_OSCILLATOR__OFF)
	{
		if (RCC->CSR & RCC_CSR_LSEON)
		{
			PWR->CR |= PWR_CR_DBP;
			l_eReturnCode = HAL_CLOCK_Wait(&PWR->CR, PWR_CR_DBP, PWR_CR_DBP, HAL_CLOCK_TIMEOUT_MS);

			RCC->CSR &= ~RCC_CSR_LSEON;
		}
	}
	else if ((!l_bRunning) || ((RCC->CSR & RCC_CSR_LSEBYP) != l_ulBypass))
	{
		/* Disable backup domain write protection. */
		PWR->CR |= PWR_CR_DBP;
		l_eReturnCode = HAL_CLOCK_Wait(&PWR->CR, PWR_CR_DBP, PWR_CR_DBP, HAL_CLOCK_TIMEOUT_MS);

		if (l_eReturnCode == HAL_SUCCESS)
		{
			RCC->CSR &= ~RCC_CSR_LSEON;
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CSR, RCC_CSR_LSERDY, 0, HAL_CLOCK_TIMEOUT_MS);
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			RCC->CSR = (RCC->CSR & ~RCC_CSR_LSEBYP) | l_ulBypass;
			RCC->CSR |= RCC_CSR_LSEON;

			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CSR, RCC_CSR_LSERDY, RCC_CSR_LSERDY, HAL_CLOCK_LSE_TIMEOUT_MS);

			if (l_eReturnCode != HAL_SUCCESS)
			{
				RCC->CSR &= ~RCC_CSR_LSEON;
			}
		}
	}
	else
	{
		/* The oscillator already runs as requested. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Applies a clock configuration
 * @param[in] p_pstFlags - the clock flags
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__TIMEOUT or HAL_ERROR
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_CLOCK_Apply(const HAL_CLOCK_FLAGS_T *p_pstFlags)
{
	static const HAL_CLOCK_GATE_T l_stPower = { &RCC->APB1ENR, RCC_APB1ENR_PWREN };
	HAL_RETURN_CODE_T l_eReturnCode;
	HAL_RETURN_CODE_T l_eLseReturnCode;
	uint32_t l_ulCoreClock = 0;
	uint32_t l_ulSwitch = RCC_CFGR_SW_MSI;
	uint32_t l_ulStatus = RCC_CFGR_SWS_MSI;
	bool l_bHseUsed;
	bool l_bHsiUsed;

	l_eReturnCode = HAL_CLOCK_Validate(p_pstFlags, &l_ulCoreClock);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_bHseUsed = (p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__HSE) || ((p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__PLL) && (p_pstFlags->m_ucPllSource == HAL_CLOCK_SOURCE__HSE));
		l_bHsiUsed = (p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__HSI) || ((p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__PLL) && (p_pstFlags->m_ucPllSource == HAL_CLOCK_SOURCE__HSI));

		/* Enable the power interface clock. */
		HAL_PRIVATE_ClockEnable(&l_stPower);

		/* Run from the MSI while everything else changes. */
		l_eReturnCode = HAL_CLOCK_EnterTransit();

		/* Disable the PLL. */
		if (l_eReturnCode == HAL_SUCCESS)
		{
			RCC->CR &= ~RCC_CR_PLLON;
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_PLLRDY, 0, HAL_CLOCK_TIMEOUT_MS);
		}

		/* Set the voltage scaling range, then the flash latency of the final frequency. */
		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_eReturnCode = HAL_CLOCK_SetVoltage(p_pstFlags->m_ucVoltage);
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			g_stHAL_CLOCK_Flags.m_ucVoltage = p_pstFlags->m_ucVoltage;

			l_eReturnCode = HAL_CLOCK_SetLatency(l_ulCoreClock, p_pstFlags->m_ucVoltage);
		}

		/* Start the oscillators. */
		if ((l_eReturnCode == HAL_SUCCESS) && l_bHseUsed)
		{
			l_eReturnCode = HAL_CLOCK_StartHse(p_pstFlags->m_ucHse);

			if (l_eReturnCode == HAL_SUCCESS)
			{
				g_ulHAL_CLOCK_HseFrequency = p_pstFlags->m_ulHseFrequency;
			}
		}

		if ((l_eReturnCode == HAL_SUCCESS) && l_bHsiUsed)
		{
			RCC->CR |= RCC_CR_HSION;
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, HAL_CLOCK_TIMEOUT_MS);
		}

		/* Configure and enable the PLL. */
		if ((l_eReturnCode == HAL_SUCCESS) && (p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__PLL))
		{
			RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL | RCC_CFGR_PLLDIV))
				| ((p_pstFlags->m_ucPllSource == HAL_CLOCK_SOURCE__HSE) ? RCC_CFGR_PLLSRC : 0U)
				| g_aulHAL_CLOCK_MultiplierCode[p_pstFlags->m_ucPllMultiplier]
				| g_aulHAL_CLOCK_DividerCode[p_pstFlags->m_ucPllDivider];

			RCC->CR |= RCC_CR_PLLON;
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, HAL_CLOCK_TIMEOUT_MS);
		}

		/* Set the final MSI range. */
		if ((l_eReturnCode == HAL_SUCCESS) && (p_pstFlags->m_ucSource == HAL_CLOCK_SOURCE__MSI))
		{
			RCC->ICSCR = (RCC->ICSCR & ~RCC_ICSCR_MSIRANGE) | g_aulHAL_CLOCK_MsiCode[p_pstFlags->m_ucMsiRange];
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CR, RCC_CR_MSIRDY, RCC_CR_MSIRDY, HAL_CLOCK_TIMEOUT_MS);
		}

		/* Set the prescalers, then the clock source. */
		if (l_eReturnCode == HAL_SUCCESS)
		{
			switch (p_pstFlags->m_ucSource)
			{
				case HAL_CLOCK_SOURCE__HSI:
				{
					l_ulSwitch = RCC_CFGR_SW_HSI;
					l_ulStatus = RCC_CFGR_SWS_HSI;

					break;
				}
				case HAL_CLOCK_SOURCE__HSE:
				{
					l_ulSwitch = RCC_CFGR_SW_HSE;
					l_ulStatus = RCC_CFGR_SWS_HSE;

					break;
				}
				case HAL_CLOCK_SOURCE__PLL:
				{
					l_ulSwitch = RCC_CFGR_SW_PLL;
					l_ulStatus = RCC_CFGR_SWS_PLL;

					break;
				}
				default: break;
			}

			RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
				| g_aulHAL_CLOCK_AhbCode[p_pstFlags->m_ucAhbPrescaler]
				| g_aulHAL_CLOCK_Apb1Code[p_pstFlags->m_ucApb1Prescaler]
				| g_aulHAL_CLOCK_Apb2Code[p_pstFlags->m_ucApb2Prescaler];

			RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | l_ulSwitch;
			l_eReturnCode = HAL_CLOCK_Wait(&RCC->CFGR, RCC_CFGR_SWS, l_ulStatus, HAL_CLOCK_TIMEOUT_MS);

			if (l_eReturnCode != HAL_SUCCESS)
			{
				/* Stay on the MSI, with the prescalers of the transit state. */
				RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
			}
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			/* Disable the oscillators that are not used anymore. */
			if (p_pstFlags->m_ucSource != HAL_CLOCK_SOURCE__MSI)
			{
				RCC->CR &= ~RCC_CR_MSION;
			}

			if (!l_bHseUsed)
			{
				RCC->CR &= ~RCC_CR_HSEON;
			}

			if ((!l_bHsiUsed) && (g_ucHAL_CLOCK_HsiRequests == 0U))
			{
				RCC->CR &= ~RCC_CR_HSION;
			}

			/* Remember what runs now. The tick settings belong to HAL_CLOCK_Init. */
			g_stHAL_CLOCK_Flags.m_ucSource = p_pstFlags->m_ucSource;
			g_stHAL_CLOCK_Flags.m_ucMsiRange = p_pstFlags->m_ucMsiRange;
			g_stHAL_CLOCK_Flags.m_ucHse = p_pstFlags->m_ucHse;
			g_stHAL_CLOCK_Flags.m_ulHseFrequency = p_pstFlags->m_ulHseFrequency;
			g_stHAL_CLOCK_Flags.m_ucPllSource = p_pstFlags->m_ucPllSource;
			g_stHAL_CLOCK_Flags.m_ucPllMultiplier = p_pstFlags->m_ucPllMultiplier;
			g_stHAL_CLOCK_Flags.m_ucPllDivider = p_pstFlags->m_ucPllDivider;
			g_stHAL_CLOCK_Flags.m_ucAhbPrescaler = p_pstFlags->m_ucAhbPrescaler;
			g_stHAL_CLOCK_Flags.m_ucApb1Prescaler = p_pstFlags->m_ucApb1Prescaler;
			g_stHAL_CLOCK_Flags.m_ucApb2Prescaler = p_pstFlags->m_ucApb2Prescaler;
		}

		HAL_CLOCK_Refresh();

		/* A module still needs the HSI (the ADC): start it again if a stop mode switched it off. */
		if ((g_ucHAL_CLOCK_HsiRequests != 0U) && (!(RCC->CR & RCC_CR_HSIRDY)))
		{
			RCC->CR |= RCC_CR_HSION;
			(void) HAL_CLOCK_Wait(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, HAL_CLOCK_TIMEOUT_MS);
		}

		/* The LSE does not feed the system clock: its failure is reported, the rest stays applied. */
		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_eLseReturnCode = HAL_CLOCK_ConfigLse(p_pstFlags->m_ucLse);

			if (l_eLseReturnCode == HAL_SUCCESS)
			{
				g_stHAL_CLOCK_Flags.m_ucLse = p_pstFlags->m_ucLse;
			}
			else
			{
				g_stHAL_CLOCK_Flags.m_ucLse = HAL_CLOCK_OSCILLATOR__OFF;
				l_eReturnCode = l_eLseReturnCode;
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Clock initialization: starts the time base and applies the clock configuration
 * @param[in] p_pstFlags - the clock flags
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER (configuration outside the limits of the selected
 *            voltage range), HAL_ERROR__TIMEOUT (an oscillator did not start) or HAL_ERROR
 * @remarks   On failure the system runs from the MSI at 2.097 MHz and the time base is valid, so
 *            the application can report the problem or try another configuration. When only the
 *            LSE fails, the system clock is as requested and HAL_ERROR__TIMEOUT is returned.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_CLOCK_Init(const HAL_CLOCK_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;

	if (p_pstFlags != NULL)
	{
		/* Start the time base on the clock that runs now: the waits below depend on it. */
		g_ucHAL_CLOCK_TickState = HAL_DISABLE;
		HAL_CLOCK_Refresh();

		l_eReturnCode = HAL_CLOCK_Apply(p_pstFlags);

		/* Configure the 1 ms tick. */
		g_stHAL_CLOCK_Flags.m_stInterrupt = p_pstFlags->m_stInterrupt;
		g_stHAL_CLOCK_Flags.m_pvContext = p_pstFlags->m_pvContext;
		g_stHAL_CLOCK_Flags.m_pfTickCallback = p_pstFlags->m_pfTickCallback;

		NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), p_pstFlags->m_stInterrupt.m_ucPrePriority, p_pstFlags->m_stInterrupt.m_ucSubPriority));

		HAL_CLOCK_SetTickState(HAL_ENABLE);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Clock update: changes the clock configuration while the system runs
 * @param[in] p_pstFlags - the clock flags; the tick fields are ignored
 * @return    Same as HAL_CLOCK_Init
 * @remarks   Modules that derive a divider from a bus clock (UART baud rate, SPI speed, I2C
 *            timing) must be configured again afterwards.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_CLOCK_Update(const HAL_CLOCK_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;

	if (p_pstFlags != NULL)
	{
		l_eReturnCode = HAL_CLOCK_Apply(p_pstFlags);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      Gets the configuration that is applied now
 * @param[out] p_pstFlags - the clock flags
 **************************************************************************************************/
void HAL_CLOCK_GetFlags(HAL_CLOCK_FLAGS_T *p_pstFlags)
{
	if (p_pstFlags != NULL)
	{
		*p_pstFlags = g_stHAL_CLOCK_Flags;
	}
}

/**************************************************************************************************
 * @brief   Gets the system clock (SYSCLK)
 * @return  The system clock in Hz
 * @remarks Computed from the registers. Peripherals are clocked by their bus: use
 *          HAL_CLOCK_GetBus for a baud rate or a timer period.
 **************************************************************************************************/
uint32_t HAL_CLOCK_Get(void)
{
	uint32_t l_ulClock = 0;
	uint32_t l_ulDivider;

	switch (RCC->CFGR & RCC_CFGR_SWS)
	{
		case RCC_CFGR_SWS_MSI:
		{
			/* Each MSI range doubles the previous one, starting from 65.536 kHz. */
			l_ulClock = 65536UL << HAL_CLOCK_Decode(g_aulHAL_CLOCK_MsiCode, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_MsiCode), RCC->ICSCR & RCC_ICSCR_MSIRANGE);

			break;
		}
		case RCC_CFGR_SWS_HSI:
		{
			l_ulClock = HAL_FAMILY_HSI_FREQUENCY;

			break;
		}
		case RCC_CFGR_SWS_HSE:
		{
			l_ulClock = g_ulHAL_CLOCK_HseFrequency;

			break;
		}
		case RCC_CFGR_SWS_PLL:
		{
			l_ulClock = (RCC->CFGR & RCC_CFGR_PLLSRC) ? g_ulHAL_CLOCK_HseFrequency : HAL_FAMILY_HSI_FREQUENCY;
			l_ulClock *= g_aucHAL_CLOCK_Multiplier[HAL_CLOCK_Decode(g_aulHAL_CLOCK_MultiplierCode, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_MultiplierCode), RCC->CFGR & RCC_CFGR_PLLMUL)];

			/* The settings are divide by 2, 3 and 4. */
			l_ulDivider = 2UL + HAL_CLOCK_Decode(g_aulHAL_CLOCK_DividerCode, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_DividerCode), RCC->CFGR & RCC_CFGR_PLLDIV);
			l_ulClock /= l_ulDivider;

			break;
		}
		default: break;
	}

	return l_ulClock;
}

/**************************************************************************************************
 * @brief     Gets the clock of a bus
 * @param[in] p_ucBus - the bus (e.g. HAL_CLOCK_BUS__APB1)
 * @return    The bus clock in Hz (HCLK, PCLK1 or PCLK2)
 **************************************************************************************************/
uint32_t HAL_CLOCK_GetBus(uint8_t p_ucBus)
{
	uint32_t l_ulClock = HAL_CLOCK_Get();

	l_ulClock >>= g_aucHAL_CLOCK_AhbShift[HAL_CLOCK_Decode(g_aulHAL_CLOCK_AhbCode, HAL_CLOCK_COUNT(g_aulHAL_CLOCK_AhbCode), RCC->CFGR & RCC_CFGR_HPRE)];

	if (p_ucBus != HAL_CLOCK_BUS__AHB)
	{
		/* The APB settings are divide by 1, 2, 4, 8 and 16. */
		l_ulClock >>= HAL_CLOCK_GetApbPrescaler(p_ucBus);
	}

	return l_ulClock;
}

/**************************************************************************************************
 * @brief     Gets the clock of the timers of a bus
 * @param[in] p_ucBus - HAL_CLOCK_BUS__APB1 or HAL_CLOCK_BUS__APB2
 * @return    The timer clock in Hz: the bus clock, doubled when the bus prescaler is not 1
 **************************************************************************************************/
uint32_t HAL_CLOCK_GetTimer(uint8_t p_ucBus)
{
	uint32_t l_ulClock = HAL_CLOCK_GetBus(p_ucBus);

	if ((p_ucBus != HAL_CLOCK_BUS__AHB) && (HAL_CLOCK_GetApbPrescaler(p_ucBus) != 0U))
	{
		l_ulClock *= 2U;
	}

	return l_ulClock;
}

/**************************************************************************************************
 * @brief   Asks for the HSI oscillator to run, whatever the system clock source is
 * @return  HAL_SUCCESS or HAL_ERROR__TIMEOUT
 * @remarks Used by modules clocked by the HSI (the ADC). Each request must be released once.
 *          HAL_CLOCK_Update keeps the HSI on while requests are open.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_CLOCK_RequestHsi(void)
{
	uint32_t l_ulState = HAL_PRIVATE_EnterCritical();

	if (g_ucHAL_CLOCK_HsiRequests < 0xFFU)
	{
		g_ucHAL_CLOCK_HsiRequests++;
	}

	RCC->CR |= RCC_CR_HSION;

	HAL_PRIVATE_ExitCritical(l_ulState);

	return HAL_CLOCK_Wait(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, HAL_CLOCK_TIMEOUT_MS);
}

/**************************************************************************************************
 * @brief   Releases a request made with HAL_CLOCK_RequestHsi
 * @remarks The oscillator stops when no request is left and the system clock does not use it.
 **************************************************************************************************/
void HAL_CLOCK_ReleaseHsi(void)
{
	uint32_t l_ulState = HAL_PRIVATE_EnterCritical();

	if (g_ucHAL_CLOCK_HsiRequests > 0U)
	{
		g_ucHAL_CLOCK_HsiRequests--;
	}

	if ((g_ucHAL_CLOCK_HsiRequests == 0U) && (!HAL_CLOCK_IsHsiUsed()))
	{
		RCC->CR &= ~RCC_CR_HSION;
	}

	HAL_PRIVATE_ExitCritical(l_ulState);
}

/**************************************************************************************************
 * @brief   Gets the time since HAL_CLOCK_Init
 * @return  The number of milliseconds; it wraps after 49 days
 * @remarks The tick does not advance while its interrupt is masked or disabled. Delays and
 *          timeouts do not depend on it.
 **************************************************************************************************/
uint32_t HAL_CLOCK_GetTick(void)
{
	return g_ulHAL_CLOCK_Tick;
}

/**************************************************************************************************
 * @brief     Enables or disables the 1 ms tick interrupt
 * @param[in] p_ucState - HAL_ENABLE or HAL_DISABLE
 * @remarks   The counter keeps running, so delays and timeouts still work when it is disabled.
 **************************************************************************************************/
void HAL_CLOCK_SetTickState(uint8_t p_ucState)
{
	uint32_t l_ulState = HAL_PRIVATE_EnterCritical();

	g_ucHAL_CLOCK_TickState = p_ucState ? HAL_ENABLE : HAL_DISABLE;

	if (p_ucState)
	{
		SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
	}
	else
	{
		SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
	}

	HAL_PRIVATE_ExitCritical(l_ulState);
}

/**************************************************************************************************
 * @brief      Starts a timeout
 * @param[out] p_pstTimeout - the timeout state
 * @param[in]  p_ulMilliseconds - the duration; it is limited to what 32 bits of clock cycles hold
 *             (134 seconds at 32 MHz)
 * @remarks    The time is measured on the SysTick counter itself, not on the tick interrupt: a
 *             timeout works with interrupts masked and inside any interrupt handler.
 **************************************************************************************************/
void HAL_CLOCK_TimeoutStart(HAL_CLOCK_TIMEOUT_T *p_pstTimeout, uint32_t p_ulMilliseconds)
{
	uint32_t l_ulLimit = 0xFFFFFFFFUL / g_ulHAL_CLOCK_TicksPerMs;

	if (p_ulMilliseconds > l_ulLimit)
	{
		p_ulMilliseconds = l_ulLimit;
	}

	p_pstTimeout->m_ulLast = SysTick->VAL;
	p_pstTimeout->m_ulRemaining = p_ulMilliseconds * g_ulHAL_CLOCK_TicksPerMs;
}

/**************************************************************************************************
 * @brief         Checks a timeout
 * @param[in,out] p_pstTimeout - the timeout state
 * @return        true when the time is over
 * @remarks       Must be called at least once per millisecond while waiting, which a polling loop
 *                does. When the time base is not running (HAL_CLOCK_Init was not called) the
 *                timeout is reported as expired at once: a wait then fails instead of hanging.
 **************************************************************************************************/
bool HAL_CLOCK_TimeoutExpired(HAL_CLOCK_TIMEOUT_T *p_pstTimeout)
{
	bool l_bExpired = true;
	uint32_t l_ulNow;
	uint32_t l_ulElapsed;

	if (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk)
	{
		/* The counter counts down and reloads at zero. */
		l_ulNow = SysTick->VAL;
		l_ulElapsed = (p_pstTimeout->m_ulLast >= l_ulNow) ? (p_pstTimeout->m_ulLast - l_ulNow) : (p_pstTimeout->m_ulLast + (SysTick->LOAD + 1U) - l_ulNow);

		p_pstTimeout->m_ulLast = l_ulNow;

		if (l_ulElapsed >= p_pstTimeout->m_ulRemaining)
		{
			p_pstTimeout->m_ulRemaining = 0;
		}
		else
		{
			p_pstTimeout->m_ulRemaining -= l_ulElapsed;
			l_bExpired = false;
		}
	}

	return l_bExpired;
}

/**************************************************************************************************
 * @brief     Waits
 * @param[in] p_ulMilliseconds - the duration in milliseconds
 * @remarks   Busy wait. Works with interrupts masked.
 **************************************************************************************************/
void HAL_CLOCK_Delay(uint32_t p_ulMilliseconds)
{
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	uint32_t l_ulStep;

	while (p_ulMilliseconds != 0U)
	{
		l_ulStep = (p_ulMilliseconds > 1000U) ? 1000U : p_ulMilliseconds;
		p_ulMilliseconds -= l_ulStep;

		HAL_CLOCK_TimeoutStart(&l_stTimeout, l_ulStep);

		while (!HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			/* Wait. */
		}
	}
}

/**************************************************************************************************
 * @brief     Waits
 * @param[in] p_ulMicroseconds - the duration in microseconds
 * @remarks   Busy wait. The resolution is one core clock cycle plus the call overhead, which is
 *            a few microseconds at 32 MHz and much more on a slow clock.
 **************************************************************************************************/
void HAL_CLOCK_DelayUs(uint32_t p_ulMicroseconds)
{
	HAL_CLOCK_TIMEOUT_T l_stTimeout;

	HAL_CLOCK_Delay(p_ulMicroseconds / 1000U);

	l_stTimeout.m_ulLast = SysTick->VAL;
	l_stTimeout.m_ulRemaining = ((p_ulMicroseconds % 1000U) * g_ulHAL_CLOCK_TicksPerMs) / 1000U;

	while (!HAL_CLOCK_TimeoutExpired(&l_stTimeout))
	{
		/* Wait. */
	}
}

/**************************************************************************************************
 * @brief   SysTick interrupt handling: the 1 ms tick
 **************************************************************************************************/
static void HAL_CLOCK_SysTick_Interrupt(void)
{
	g_ulHAL_CLOCK_Tick++;

	if (g_stHAL_CLOCK_Flags.m_pfTickCallback != NULL)
	{
		g_stHAL_CLOCK_Flags.m_pfTickCallback(g_stHAL_CLOCK_Flags.m_pvContext);
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_CLOCK
#include "hal_family_irq.h"
