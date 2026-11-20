/**************************************************************************************************
 * @file    hal_family.h
 * @brief   STM32L1 family: device header and what differs from the other families
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Private to the HAL sources (see hal_private.h).
 **************************************************************************************************/
#ifndef HAL_FAMILY_H_
#define HAL_FAMILY_H_

#include "stm32l1xx.h"

/* Limits of the clock tree, in Hz. Index: HAL_CLOCK_VOLTAGE__RANGE_1 to 3. */
#define HAL_FAMILY_HSI_FREQUENCY      16000000UL
#define HAL_FAMILY_SYSCLK_LIMITS      { 32000000UL, 16000000UL, 4200000UL }
#define HAL_FAMILY_PLL_VCO_LIMITS     { 96000000UL, 48000000UL, 24000000UL }
#define HAL_FAMILY_FLASH_0WS_LIMITS   { 16000000UL, 8000000UL, 2100000UL }
#define HAL_FAMILY_PLL_INPUT_MINIMUM  2000000UL
#define HAL_FAMILY_PLL_INPUT_MAXIMUM  24000000UL

/* EXTI registers: their names differ between families. */
#define HAL_FAMILY_EXTI_IMR  (EXTI->IMR)
#define HAL_FAMILY_EXTI_RTSR (EXTI->RTSR)
#define HAL_FAMILY_EXTI_FTSR (EXTI->FTSR)
#define HAL_FAMILY_EXTI_PR   (EXTI->PR)

/**************************************************************************************************
 * @brief     Routes a pin to its EXTI line
 * @param[in] p_ucPin - the pin, which is also the EXTI line (0 to 15)
 * @param[in] p_ucCode - the port code of this family (see g_aucHAL_GPIO_ExtiCode)
 * @remarks   Call with interrupts masked. The SYSCFG clock must be enabled.
 **************************************************************************************************/
static inline void HAL_FAMILY_SelectExti(uint8_t p_ucPin, uint8_t p_ucCode)
{
	uint32_t l_ulShift = 4UL * (p_ucPin & 0x03U);

	SYSCFG->EXTICR[p_ucPin >> 2] = (SYSCFG->EXTICR[p_ucPin >> 2] & ~(0x0FUL << l_ulShift)) | ((uint32_t) p_ucCode << l_ulShift);
}

/**************************************************************************************************
 * @brief   Enables the clock of the block that holds the EXTI routing
 **************************************************************************************************/
static inline void HAL_FAMILY_EnableExtiClock(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	(void) RCC->APB2ENR;
}

/**************************************************************************************************
 * @brief     Connects or disconnects the LSE as the clock of a timer
 * @param[in] p_pstTimer - the timer registers
 * @param[in] p_ucRoute - how the LSE reaches this timer (see HAL_TIMER_LseRoute)
 * @param[in] p_ucState - HAL_ENABLE to count LSE periods, HAL_DISABLE to count the bus clock
 * @remarks   TIM9 receives the LSE on its internal trigger 1 and counts it in external clock
 *            mode 1. TIM10 and TIM11 receive it on their external trigger input and count it in
 *            external clock mode 2.
 **************************************************************************************************/
static inline void HAL_FAMILY_SelectTimerLse(TIM_TypeDef *p_pstTimer, uint8_t p_ucRoute, uint8_t p_ucState)
{
	if (p_ucRoute == 1U)
	{
		if (p_ucState)
		{
			p_pstTimer->OR |= TIM9_OR_ITR1_RMP;
			p_pstTimer->SMCR = TIM_SMCR_TS_0 | TIM_SMCR_SMS;
		}
		else
		{
			p_pstTimer->SMCR = 0;
			p_pstTimer->OR &= ~TIM9_OR_ITR1_RMP;
		}
	}
	else if (p_ucRoute == 2U)
	{
		if (p_ucState)
		{
			p_pstTimer->OR |= TIM_OR_ETR_RMP;
			p_pstTimer->SMCR = TIM_SMCR_ECE;
		}
		else
		{
			p_pstTimer->SMCR = 0;
			p_pstTimer->OR &= ~TIM_OR_ETR_RMP;
		}
	}
	else
	{
		p_pstTimer->SMCR = 0;
	}
}

#endif /* HAL_FAMILY_H_ */
