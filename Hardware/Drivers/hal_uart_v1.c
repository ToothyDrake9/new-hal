/**************************************************************************************************
 * @file    hal_uart_v1.c
 * @brief   Microprocessor UART module, for the USART with SR/DR registers (L1, F1, F2, F4)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the baud rate uses the bus clock with rounding, the control registers are
 *          written as a whole, the interrupt handler reads the data register itself (which is
 *          what clears the flags on this peripheral), disabling one interrupt source no longer
 *          disables the NVIC line
 **************************************************************************************************/
#include "hal_uart.h"
#include "hal_clock.h"
#include "hal_calc.h"
#include "hal_private.h"

typedef struct
{
	HAL_UART_RX_CALLBACK_T m_pfRxCallback;
	HAL_UART_TX_CALLBACK_T m_pfTxCallback;
	HAL_UART_EVENT_CALLBACK_T m_pfTxCompleteCallback;
	HAL_UART_ERROR_CALLBACK_T m_pfErrorCallback;
	void *m_pvContext;
	uint8_t m_ucConfigured;
	uint8_t m_ucDataMask; // Hides the parity bit of a 7 bit frame
}HAL_UART_MODULE_STATE_T;

static HAL_UART_MODULE_STATE_T g_astHAL_UART_State[HAL_UART_NO];

/**************************************************************************************************
 * @brief     Checks a module argument
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_UART_Check(uint8_t p_ucModule)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ucModule >= HAL_UART_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_UART_State[p_ucModule].m_ucConfigured)
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
 * @brief     UART configuration
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[in] p_pstFlags - the UART flags, pins and callbacks
 * @return    HAL_SUCCESS, or HAL_ERROR__PARAMETER (bad argument, frame format that the hardware
 *            cannot produce, baud rate out of reach of the bus clock)
 * @remarks   The module is left disabled: call HAL_UART_SetState, then HAL_UART_SetInterrupt for
 *            the interrupts that are wanted. The NVIC line is enabled here when at least one
 *            callback is given.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_Config(uint8_t p_ucModule, const HAL_UART_FLAGS_T *p_pstFlags)
{
	static const HAL_GPIO_FLAGS_T l_stPinFlags = { HAL_GPIO_MODE__FUNCTION, HAL_GPIO_TYPE__PUSH_PULL, HAL_GPIO_PULL__PULL_UP, HAL_GPIO_SPEED__HIGH, 0 };
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	const HAL_UART_DEVICE_T *l_pstDevice = NULL;
	HAL_UART_MODULE_STATE_T *l_pstState;
	USART_TypeDef *l_pstUart;
	uint32_t l_ulDivider = 0;
	uint32_t l_ulControl1 = 0;
	uint32_t l_ulControl2 = 0;
	uint32_t l_ulControl3 = 0;

	if ((p_ucModule >= HAL_UART_NO) || (p_pstFlags == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else
	{
		/*
		 * Frame length. The hardware counts the parity bit as a data bit: 8 data bits with
		 * parity and 9 data bits without parity both need the 9 bit word length.
		 */
		switch (p_pstFlags->m_ucDataBits)
		{
			case HAL_UART_DATABITS__7:
			{
				if (p_pstFlags->m_ucParity == HAL_UART_PARITY__NONE)
				{
					l_eReturnCode = HAL_ERROR__PARAMETER;
				}

				break;
			}
			case HAL_UART_DATABITS__8:
			{
				if (p_pstFlags->m_ucParity != HAL_UART_PARITY__NONE)
				{
					l_ulControl1 |= USART_CR1_M;
				}

				break;
			}
			case HAL_UART_DATABITS__9:
			{
				if (p_pstFlags->m_ucParity != HAL_UART_PARITY__NONE)
				{
					l_eReturnCode = HAL_ERROR__PARAMETER;
				}

				l_ulControl1 |= USART_CR1_M;

				break;
			}
			default:
			{
				l_eReturnCode = HAL_ERROR__PARAMETER;

				break;
			}
		}

		/* Set the parity. */
		switch (p_pstFlags->m_ucParity)
		{
			case HAL_UART_PARITY__NONE:
			{
				break;
			}
			case HAL_UART_PARITY__ODD:
			{
				l_ulControl1 |= USART_CR1_PCE | USART_CR1_PS;

				break;
			}
			case HAL_UART_PARITY__EVEN:
			{
				l_ulControl1 |= USART_CR1_PCE;

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
		l_pstDevice = &g_astHAL_UART_Device[p_ucModule];

		/* Set the baud rate, from the clock of the bus the module sits on. */
		l_eReturnCode = HAL_CALC_UartDivider(HAL_CLOCK_GetBus(l_pstDevice->m_ucBus), p_pstFlags->m_ulBaud, &l_ulDivider);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stTxPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stRxPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstUart = l_pstDevice->m_pstRegisters;
		l_pstState = &g_astHAL_UART_State[p_ucModule];

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&l_pstDevice->m_stClock);

		/* Disable the module and all its interrupt sources. */
		l_pstUart->CR1 = 0;

		/* The callbacks are stored before any interrupt can be enabled. */
		l_pstState->m_ucConfigured = HAL_DISABLE;
		l_pstState->m_pfRxCallback = p_pstFlags->m_pfRxCallback;
		l_pstState->m_pfTxCallback = p_pstFlags->m_pfTxCallback;
		l_pstState->m_pfTxCompleteCallback = p_pstFlags->m_pfTxCompleteCallback;
		l_pstState->m_pfErrorCallback = p_pstFlags->m_pfErrorCallback;
		l_pstState->m_pvContext = p_pstFlags->m_pvContext;
		l_pstState->m_ucDataMask = (p_pstFlags->m_ucDataBits == HAL_UART_DATABITS__7) ? 0x7FU : 0xFFU;

		/* Set the stop bits. */
		if (p_pstFlags->m_ucStopBits == HAL_UART_STOPBITS__2)
		{
			l_ulControl2 |= USART_CR2_STOP_1;
		}

		/* Set the IrDA capability. */
		if (p_pstFlags->m_ucIrDA)
		{
			l_ulControl3 |= USART_CR3_IREN;
		}

		/* Set the mode: only the directions that have a pin. Oversampling stays at 16. */
		if (p_pstFlags->m_stTxPin.m_ucModule != HAL_GPIO_NONE)
		{
			l_ulControl1 |= USART_CR1_TE;
		}

		if (p_pstFlags->m_stRxPin.m_ucModule != HAL_GPIO_NONE)
		{
			l_ulControl1 |= USART_CR1_RE;
		}

		l_pstUart->CR2 = l_ulControl2;
		l_pstUart->CR3 = l_ulControl3;
		l_pstUart->GTPR = p_pstFlags->m_ucIrDA ? 0x01U : 0x00U;
		l_pstUart->BRR = l_ulDivider;
		l_pstUart->CR1 = l_ulControl1;

		l_pstState->m_ucConfigured = HAL_ENABLE;

		/* Configure the NVIC interrupt. */
		if ((p_pstFlags->m_pfRxCallback != NULL) || (p_pstFlags->m_pfTxCallback != NULL) || (p_pstFlags->m_pfTxCompleteCallback != NULL))
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
 * @brief     UART set state
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[in] p_ucState - the UART state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucState)
		{
			HAL_PRIVATE_Modify(&g_astHAL_UART_Device[p_ucModule].m_pstRegisters->CR1, 0, USART_CR1_UE);
		}
		else
		{
			HAL_PRIVATE_Modify(&g_astHAL_UART_Device[p_ucModule].m_pstRegisters->CR1, USART_CR1_UE, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     UART set interrupt
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[in] p_ucInterrupt - the UART interrupt (e.g. HAL_UART_RX)
 * @param[in] p_ucState - the UART interrupt state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED when the module was
 *            not configured or no callback was given for this interrupt
 * @remarks   Only the enable bit of this source changes; the other sources of the module keep
 *            working. Enable HAL_UART_TX when there is data to send: the transmit callback is
 *            then called for each byte, and the module disables the source when the callback
 *            answers HAL_UART_TX__DONE.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	const HAL_UART_MODULE_STATE_T *l_pstState;
	uint32_t l_ulMask = 0;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstState = &g_astHAL_UART_State[p_ucModule];

		switch (p_ucInterrupt)
		{
			case HAL_UART_RX:
			{
				l_ulMask = USART_CR1_RXNEIE;

				if (l_pstState->m_pfRxCallback == NULL)
				{
					l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
				}

				break;
			}
			case HAL_UART_TX:
			{
				l_ulMask = USART_CR1_TXEIE;

				if (l_pstState->m_pfTxCallback == NULL)
				{
					l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
				}

				break;
			}
			case HAL_UART_TX_COMPLETE:
			{
				l_ulMask = USART_CR1_TCIE;

				if (l_pstState->m_pfTxCompleteCallback == NULL)
				{
					l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
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
		if (p_ucState)
		{
			HAL_PRIVATE_Modify(&g_astHAL_UART_Device[p_ucModule].m_pstRegisters->CR1, 0, l_ulMask);
		}
		else
		{
			HAL_PRIVATE_Modify(&g_astHAL_UART_Device[p_ucModule].m_pstRegisters->CR1, l_ulMask, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      UART get baud: the baud rate that the divider really produces
 * @param[in]  p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[out] p_pulBaud - the baud rate
 * @return     HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_GetBaud(uint8_t p_ucModule, uint32_t *p_pulBaud)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	uint32_t l_ulDivider;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pulBaud == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulDivider = g_astHAL_UART_Device[p_ucModule].m_pstRegisters->BRR;

		*p_pulBaud = (l_ulDivider != 0U) ? (HAL_CLOCK_GetBus(g_astHAL_UART_Device[p_ucModule].m_ucBus) / l_ulDivider) : 0U;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      UART read byte, by polling
 * @param[in]  p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[out] p_pucByte - the received byte
 * @param[in]  p_ulTimeoutMs - how long to wait for a byte; 0 returns at once
 * @return     HAL_SUCCESS, HAL_ERROR__TIMEOUT (nothing received), HAL_ERROR__BUS (framing, noise
 *             or parity error: the byte is dropped), HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED
 * @remarks    For use without the receive interrupt. An overrun is not an error of this byte:
 *             the byte is returned and the flag is cleared.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_ReadByte(uint8_t p_ucModule, uint8_t *p_pucByte, uint32_t p_ulTimeoutMs)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	USART_TypeDef *l_pstUart;
	uint32_t l_ulStatus;
	uint32_t l_ulData;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pucByte == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstUart = g_astHAL_UART_Device[p_ucModule].m_pstRegisters;

		HAL_CLOCK_TimeoutStart(&l_stTimeout, p_ulTimeoutMs);

		for (;;)
		{
			/* The status then the data register must be read, in this order, to clear the flags. */
			l_ulStatus = l_pstUart->SR;

			if (l_ulStatus & (USART_SR_RXNE | USART_SR_ORE))
			{
				l_ulData = l_pstUart->DR;

				if (l_ulStatus & (USART_SR_FE | USART_SR_NE | USART_SR_PE))
				{
					l_eReturnCode = HAL_ERROR__BUS;

					break;
				}

				if (l_ulStatus & USART_SR_RXNE)
				{
					*p_pucByte = (uint8_t) l_ulData & g_astHAL_UART_State[p_ucModule].m_ucDataMask;

					break;
				}
			}

			if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
			{
				l_eReturnCode = HAL_ERROR__TIMEOUT;

				break;
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     UART write byte
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[in] p_ucByte - the byte to be written
 * @return    HAL_SUCCESS, HAL_ERROR__TIMEOUT (module disabled or stuck), HAL_ERROR__PARAMETER or
 *            HAL_ERROR__NOT_CONFIGURED
 * @remarks   Waits until the module can take the byte, then returns: the byte is still being
 *            shifted out. Use HAL_UART_Flush to wait for the end of the transmission. Do not mix
 *            with interrupt driven transmission on the same module.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_WriteByte(uint8_t p_ucModule, uint8_t p_ucByte)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	USART_TypeDef *l_pstUart;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstUart = g_astHAL_UART_Device[p_ucModule].m_pstRegisters;

		HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_UART_TIMEOUT_MS);

		while (!(l_pstUart->SR & USART_SR_TXE))
		{
			if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
			{
				l_eReturnCode = HAL_ERROR__TIMEOUT;

				break;
			}
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_pstUart->DR = p_ucByte;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     UART write buffer
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @param[in] p_pucBuff - the buffer to be written
 * @param[in] p_unSize - the buffer size
 * @return    Same as HAL_UART_WriteByte; the transmission stops at the first error
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_WriteBuff(uint8_t p_ucModule, const uint8_t *p_pucBuff, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	uint16_t l_unIndex;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pucBuff == NULL) && (p_unSize != 0U))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	for (l_unIndex = 0; (l_unIndex < p_unSize) && (l_eReturnCode == HAL_SUCCESS); l_unIndex++)
	{
		l_eReturnCode = HAL_UART_WriteByte(p_ucModule, p_pucBuff[l_unIndex]);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     UART flush: waits until everything written has left the pin
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @return    HAL_SUCCESS, HAL_ERROR__TIMEOUT, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_UART_Flush(uint8_t p_ucModule)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Check(p_ucModule);
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	const USART_TypeDef *l_pstUart;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstUart = g_astHAL_UART_Device[p_ucModule].m_pstRegisters;

		HAL_CLOCK_TimeoutStart(&l_stTimeout, 2U * HAL_UART_TIMEOUT_MS);

		while ((l_pstUart->SR & (USART_SR_TXE | USART_SR_TC)) != (USART_SR_TXE | USART_SR_TC))
		{
			if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
			{
				l_eReturnCode = HAL_ERROR__TIMEOUT;

				break;
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     UART interrupt handling
 * @param[in] p_ucModule - the UART module (e.g. HAL_UART_0)
 * @remarks   No flag is cleared by writing the status register: on this peripheral a
 *            read-modify-write of it can erase an event that arrives in between.
 **************************************************************************************************/
static void HAL_UART_Interrupt(uint8_t p_ucModule)
{
	const HAL_UART_MODULE_STATE_T *l_pstState = &g_astHAL_UART_State[p_ucModule];
	USART_TypeDef *l_pstUart = g_astHAL_UART_Device[p_ucModule].m_pstRegisters;
	uint32_t l_ulStatus = l_pstUart->SR;
	uint32_t l_ulControl = l_pstUart->CR1;
	uint32_t l_ulData;
	uint8_t l_ucErrors = 0;
	uint8_t l_ucByte = 0;

	/* Reception. With the receive interrupt enabled an overrun alone also raises the interrupt. */
	if ((l_ulControl & USART_CR1_RXNEIE) && (l_ulStatus & (USART_SR_RXNE | USART_SR_ORE)))
	{
		/* Reading the data register after the status register clears all the receive flags. */
		l_ulData = l_pstUart->DR;

		if (l_ulStatus & USART_SR_ORE)
		{
			l_ucErrors |= HAL_UART_ERROR__OVERRUN;
		}

		if (l_ulStatus & USART_SR_FE)
		{
			l_ucErrors |= HAL_UART_ERROR__FRAMING;
		}

		if (l_ulStatus & USART_SR_NE)
		{
			l_ucErrors |= HAL_UART_ERROR__NOISE;
		}

		if (l_ulStatus & USART_SR_PE)
		{
			l_ucErrors |= HAL_UART_ERROR__PARITY;
		}

		if ((l_ucErrors != 0U) && (l_pstState->m_pfErrorCallback != NULL))
		{
			l_pstState->m_pfErrorCallback(l_pstState->m_pvContext, l_ucErrors);
		}

		/* An overrun loses the byte that came after this one: this one is still good. */
		if ((l_ulStatus & USART_SR_RXNE) && ((l_ucErrors & (uint8_t) ~HAL_UART_ERROR__OVERRUN) == 0U) && (l_pstState->m_pfRxCallback != NULL))
		{
			l_pstState->m_pfRxCallback(l_pstState->m_pvContext, (uint8_t) l_ulData & l_pstState->m_ucDataMask);
		}
	}

	/* Transmission: the callback gives the next byte, or the source is switched off. */
	if ((l_ulControl & USART_CR1_TXEIE) && (l_ulStatus & USART_SR_TXE))
	{
		if ((l_pstState->m_pfTxCallback != NULL) && (l_pstState->m_pfTxCallback(l_pstState->m_pvContext, &l_ucByte) == HAL_UART_TX__SEND))
		{
			l_pstUart->DR = l_ucByte;
		}
		else
		{
			l_pstUart->CR1 &= ~USART_CR1_TXEIE;
		}
	}

	/* Transmission complete. The flag is cleared by writing 0 to it and 1 to all the others. */
	if ((l_ulControl & USART_CR1_TCIE) && (l_ulStatus & USART_SR_TC))
	{
		l_pstUart->SR = (uint32_t) ~USART_SR_TC;

		if (l_pstState->m_pfTxCompleteCallback != NULL)
		{
			l_pstState->m_pfTxCompleteCallback(l_pstState->m_pvContext);
		}
		else
		{
			l_pstUart->CR1 &= ~USART_CR1_TCIE;
		}
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_UART
#include "hal_family_irq.h"
