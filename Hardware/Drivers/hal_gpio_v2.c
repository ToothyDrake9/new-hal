/**************************************************************************************************
 * @file    hal_gpio_v2.c
 * @brief   Microprocessor GPIO module, for ports with MODER/AFR registers (L1, L4, F4, F7, ...)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: every field is cleared before it is set, the NVIC is only touched for pins
 *          that are interrupt sources, pending flags are cleared one at a time, callbacks are
 *          registered per EXTI line
 **************************************************************************************************/
#include "hal_gpio.h"
#include "hal_private.h"

_Static_assert(offsetof(HAL_GPIO_REGISTERS_T, IDR) == offsetof(GPIO_TypeDef, IDR), "GPIO layout");
_Static_assert(offsetof(HAL_GPIO_REGISTERS_T, ODR) == offsetof(GPIO_TypeDef, ODR), "GPIO layout");
_Static_assert(offsetof(HAL_GPIO_REGISTERS_T, BSRR) == offsetof(GPIO_TypeDef, BSRR), "GPIO layout");
_Static_assert(offsetof(HAL_GPIO_REGISTERS_T, AFR) == offsetof(GPIO_TypeDef, AFR), "GPIO layout");

typedef struct
{
	HAL_GPIO_CALLBACK_T m_pfCallback; // NULL - the line is free
	void *m_pvContext;
	uint8_t m_ucModule;
}HAL_GPIO_LINE_T;

/* One entry per EXTI line. A line serves one pin number, on one port at a time. */
static HAL_GPIO_LINE_T g_astHAL_GPIO_Line[HAL_GPIO_PIN_NO];

/**************************************************************************************************
 * @brief     GPIO configuration
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @param[in] p_pstFlags - the GPIO flags (e.g. HAL_GPIO_MODE__OUTPUT)
 * @return    HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks   The pin can be configured again at any time. Interrupts are configured separately
 *            with HAL_GPIO_ConfigInterrupt.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_GPIO_Config(uint8_t p_ucModule, uint8_t p_ucPin, const HAL_GPIO_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	GPIO_TypeDef *l_pstPort;
	uint32_t l_ulState;
	uint32_t l_ulShift;

	if ((p_ucModule < HAL_GPIO_NO) && (p_ucPin < HAL_GPIO_PIN_NO) && (p_pstFlags != NULL))
	{
		l_pstPort = (GPIO_TypeDef *) g_aulHAL_GPIO_Address[p_ucModule];

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&g_astHAL_GPIO_Clock[p_ucModule]);

		l_ulState = HAL_PRIVATE_EnterCritical();

		/* Set the alternate function before the mode, so the pin never drives a wrong function. */
		if (p_pstFlags->m_ucMode == HAL_GPIO_MODE__FUNCTION)
		{
			l_ulShift = 4UL * (p_ucPin & 0x07U);
			l_pstPort->AFR[p_ucPin >> 3] = (l_pstPort->AFR[p_ucPin >> 3] & ~(0x0FUL << l_ulShift)) | ((uint32_t) p_pstFlags->m_ucFunction << l_ulShift);
		}

		/* Set the pin type. */
		l_pstPort->OTYPER = (l_pstPort->OTYPER & ~(1UL << p_ucPin)) | ((uint32_t) p_pstFlags->m_ucType << p_ucPin);

		/* Set the pin output speed. */
		l_ulShift = 2UL * p_ucPin;
		l_pstPort->OSPEEDR = (l_pstPort->OSPEEDR & ~(0x03UL << l_ulShift)) | ((uint32_t) p_pstFlags->m_ucSpeed << l_ulShift);

		/* Set the pin resistor. */
		l_pstPort->PUPDR = (l_pstPort->PUPDR & ~(0x03UL << l_ulShift)) | ((uint32_t) p_pstFlags->m_ucPull << l_ulShift);

		/* Set the pin mode. */
		l_pstPort->MODER = (l_pstPort->MODER & ~(0x03UL << l_ulShift)) | ((uint32_t) p_pstFlags->m_ucMode << l_ulShift);

		HAL_PRIVATE_ExitCritical(l_ulState);

		l_eReturnCode = HAL_SUCCESS;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     GPIO configuration of a pin that belongs to another module
 * @param[in] p_pstPin - the pin and its alternate function
 * @param[in] p_pstFlags - the GPIO flags; the function field is taken from p_pstPin
 * @return    HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks   A pin whose module is HAL_GPIO_NONE is skipped and reported as a success.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_GPIO_ConfigPin(const HAL_GPIO_PIN_T *p_pstPin, const HAL_GPIO_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	HAL_GPIO_FLAGS_T l_stFlags;

	if ((p_pstPin != NULL) && (p_pstFlags != NULL))
	{
		if (p_pstPin->m_ucModule == HAL_GPIO_NONE)
		{
			l_eReturnCode = HAL_SUCCESS;
		}
		else
		{
			l_stFlags = *p_pstFlags;
			l_stFlags.m_ucFunction = p_pstPin->m_ucFunction & 0x0FU;

			l_eReturnCode = HAL_GPIO_Config(p_pstPin->m_ucModule, p_pstPin->m_ucPin, &l_stFlags);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     GPIO interrupt configuration
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @param[in] p_pstFlags - the edge, the priority and the callback
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__CONFLICT when the EXTI line is taken
 *            by the same pin number of another port, or when the pin shares its interrupt line
 *            with a pin that was given another priority
 * @remarks   The interrupt stays masked until HAL_GPIO_SetInterrupt enables it. The pin itself is
 *            configured with HAL_GPIO_Config.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_GPIO_ConfigInterrupt(uint8_t p_ucModule, uint8_t p_ucPin, const HAL_GPIO_INTERRUPT_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulPriority;
	uint32_t l_ulState;
	uint32_t l_ulMask;
	uint8_t l_ucIndex;
	IRQn_Type l_eIRQn = (IRQn_Type) 0;

	if ((p_ucModule >= HAL_GPIO_NO) || (p_ucPin >= HAL_GPIO_PIN_NO) || (p_pstFlags == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if ((p_pstFlags->m_pfCallback == NULL) || (p_pstFlags->m_ucEdge < HAL_GPIO_RISING) || (p_pstFlags->m_ucEdge > HAL_GPIO_BOTH))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if ((g_astHAL_GPIO_Line[p_ucPin].m_pfCallback != NULL) && (g_astHAL_GPIO_Line[p_ucPin].m_ucModule != p_ucModule))
	{
		l_eReturnCode = HAL_ERROR__CONFLICT;
	}
	else
	{
		l_eIRQn = g_aeHAL_GPIO_IRQn[p_ucPin];
		l_ulPriority = NVIC_EncodePriority(NVIC_GetPriorityGrouping(), p_pstFlags->m_stInterrupt.m_ucPrePriority, p_pstFlags->m_stInterrupt.m_ucSubPriority);

		/* Pins that share an interrupt line share its priority. */
		for (l_ucIndex = 0; l_ucIndex < HAL_GPIO_PIN_NO; l_ucIndex++)
		{
			if ((l_ucIndex != p_ucPin) && (g_aeHAL_GPIO_IRQn[l_ucIndex] == l_eIRQn) && (g_astHAL_GPIO_Line[l_ucIndex].m_pfCallback != NULL))
			{
				if (NVIC_GetPriority(l_eIRQn) != l_ulPriority)
				{
					l_eReturnCode = HAL_ERROR__CONFLICT;
				}
			}
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulMask = 1UL << p_ucPin;

		l_ulState = HAL_PRIVATE_EnterCritical();

		HAL_FAMILY_EnableExtiClock();

		/* Mask the line while it is changed. */
		HAL_FAMILY_EXTI_IMR &= ~l_ulMask;

		/* Configure the pin as interrupt source. */
		HAL_FAMILY_SelectExti(p_ucPin, g_aucHAL_GPIO_ExtiCode[p_ucModule]);

		/* Select the edges. */
		if (p_pstFlags->m_ucEdge & HAL_GPIO_RISING)
		{
			HAL_FAMILY_EXTI_RTSR |= l_ulMask;
		}
		else
		{
			HAL_FAMILY_EXTI_RTSR &= ~l_ulMask;
		}

		if (p_pstFlags->m_ucEdge & HAL_GPIO_FALLING)
		{
			HAL_FAMILY_EXTI_FTSR |= l_ulMask;
		}
		else
		{
			HAL_FAMILY_EXTI_FTSR &= ~l_ulMask;
		}

		/* Clear the interrupt flag of this line only: the register is cleared by writing 1. */
		HAL_FAMILY_EXTI_PR = l_ulMask;

		g_astHAL_GPIO_Line[p_ucPin].m_pvContext = p_pstFlags->m_pvContext;
		g_astHAL_GPIO_Line[p_ucPin].m_ucModule = p_ucModule;
		g_astHAL_GPIO_Line[p_ucPin].m_pfCallback = p_pstFlags->m_pfCallback;

		HAL_PRIVATE_ExitCritical(l_ulState);

		/* Configure the NVIC interrupt. */
		HAL_PRIVATE_EnableIRQ(l_eIRQn, &p_pstFlags->m_stInterrupt);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     GPIO set interrupt
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @param[in] p_ucState - the GPIO interrupt state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED when
 *            HAL_GPIO_ConfigInterrupt was not called for this pin
 * @remarks   Only the mask of this line changes. The NVIC line stays enabled, because other pins
 *            may share it. An edge that was seen while the line was masked is discarded.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_GPIO_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucPin, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulState;
	uint32_t l_ulMask;

	if ((p_ucModule >= HAL_GPIO_NO) || (p_ucPin >= HAL_GPIO_PIN_NO))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if ((g_astHAL_GPIO_Line[p_ucPin].m_pfCallback == NULL) || (g_astHAL_GPIO_Line[p_ucPin].m_ucModule != p_ucModule))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		l_ulMask = 1UL << p_ucPin;

		l_ulState = HAL_PRIVATE_EnterCritical();

		if (p_ucState)
		{
			HAL_FAMILY_EXTI_PR = l_ulMask;
			HAL_FAMILY_EXTI_IMR |= l_ulMask;
		}
		else
		{
			HAL_FAMILY_EXTI_IMR &= ~l_ulMask;
		}

		HAL_PRIVATE_ExitCritical(l_ulState);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     GPIO release interrupt: frees the EXTI line of a pin
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks   The NVIC line is disabled only when no other pin uses it.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_GPIO_ReleaseInterrupt(uint8_t p_ucModule, uint8_t p_ucPin)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_GPIO_SetInterrupt(p_ucModule, p_ucPin, HAL_DISABLE);
	uint32_t l_ulState;
	uint32_t l_ulMask;
	uint8_t l_ucIndex;
	bool l_bShared = false;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulMask = 1UL << p_ucPin;

		l_ulState = HAL_PRIVATE_EnterCritical();

		HAL_FAMILY_EXTI_RTSR &= ~l_ulMask;
		HAL_FAMILY_EXTI_FTSR &= ~l_ulMask;
		HAL_FAMILY_EXTI_PR = l_ulMask;

		g_astHAL_GPIO_Line[p_ucPin].m_pfCallback = NULL;

		for (l_ucIndex = 0; l_ucIndex < HAL_GPIO_PIN_NO; l_ucIndex++)
		{
			if ((g_aeHAL_GPIO_IRQn[l_ucIndex] == g_aeHAL_GPIO_IRQn[p_ucPin]) && (g_astHAL_GPIO_Line[l_ucIndex].m_pfCallback != NULL))
			{
				l_bShared = true;
			}
		}

		if (!l_bShared)
		{
			NVIC_DisableIRQ(g_aeHAL_GPIO_IRQn[p_ucPin]);
		}

		HAL_PRIVATE_ExitCritical(l_ulState);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     GPIO interrupt handling for a group of EXTI lines
 * @param[in] p_ulLines - the lines served by the interrupt vector (one bit per line)
 **************************************************************************************************/
static void HAL_GPIO_Interrupt(uint32_t p_ulLines)
{
	uint32_t l_ulPending = HAL_FAMILY_EXTI_PR & HAL_FAMILY_EXTI_IMR & p_ulLines;
	const HAL_GPIO_LINE_T *l_pstLine;
	uint32_t l_ulLine;

	while (l_ulPending != 0U)
	{
		l_ulLine = (uint32_t) __builtin_ctzl(l_ulPending);
		l_ulPending &= ~(1UL << l_ulLine);

		/* Clear the flag before the callback, so an edge that arrives during it is kept. */
		HAL_FAMILY_EXTI_PR = 1UL << l_ulLine;

		l_pstLine = &g_astHAL_GPIO_Line[l_ulLine];

		if (l_pstLine->m_pfCallback != NULL)
		{
			l_pstLine->m_pfCallback(l_pstLine->m_pvContext);
		}
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_GPIO
#include "hal_family_irq.h"
