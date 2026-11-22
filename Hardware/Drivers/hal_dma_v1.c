/**************************************************************************************************
 * @file    hal_dma_v1.c
 * @brief   Microprocessor DMA module, for the DMA with one register set per channel (L1, F1, L4)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten
 * @remarks Which peripheral request reaches which channel is fixed by the hardware on the L1:
 *          see the request mapping table of the reference manual.
 **************************************************************************************************/
#include "hal_dma.h"
#include "hal_private.h"

typedef struct
{
	HAL_DMA_CALLBACK_T m_pfCompleteCallback;
	HAL_DMA_CALLBACK_T m_pfHalfCallback;
	HAL_DMA_CALLBACK_T m_pfErrorCallback;
	void *m_pvContext;
	uint8_t m_ucConfigured;
}HAL_DMA_CHANNEL_STATE_T;

/* The flags of channel 1, and how far the flags of another channel are from them. */
#define HAL_DMA_ALL_FLAGS (DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1)
#define HAL_DMA_SHIFT(p_ucChannel) ((uint32_t) (p_ucChannel) * (DMA_IFCR_CGIF2_Pos - DMA_IFCR_CGIF1_Pos))

static HAL_DMA_CHANNEL_STATE_T g_astHAL_DMA_State[HAL_DMA_NO][HAL_DMA_CHANNEL_NO];

/**************************************************************************************************
 * @brief     Gets the registers of a channel
 * @param[in] p_ucModule - the DMA module, already checked
 * @param[in] p_ucChannel - the DMA channel, already checked
 * @return    The channel registers
 **************************************************************************************************/
static DMA_Channel_TypeDef *HAL_DMA_GetChannel(uint8_t p_ucModule, uint8_t p_ucChannel)
{
	return g_astHAL_DMA_Device[p_ucModule].m_apstChannel[p_ucChannel];
}

/**************************************************************************************************
 * @brief     Checks the module and channel arguments
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @param[in] p_bConfigured - true when the channel must have been configured
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_DMA_Check(uint8_t p_ucModule, uint8_t p_ucChannel, bool p_bConfigured)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if ((p_ucModule >= HAL_DMA_NO) || (p_ucChannel >= g_astHAL_DMA_Device[p_ucModule].m_ucChannels))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (p_bConfigured && (!g_astHAL_DMA_State[p_ucModule][p_ucChannel].m_ucConfigured))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		/* The channel can be used. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DMA configuration
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @param[in] p_pstFlags - the DMA flags and callbacks
 * @return    HAL_SUCCESS or HAL_ERROR__PARAMETER
 * @remarks   The channel is left stopped. The peripheral that makes the requests must be told to
 *            use DMA with its own registers.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DMA_Config(uint8_t p_ucModule, uint8_t p_ucChannel, const HAL_DMA_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_DMA_Check(p_ucModule, p_ucChannel, false);
	HAL_DMA_CHANNEL_STATE_T *l_pstState;
	DMA_Channel_TypeDef *l_pstChannel;
	const HAL_DMA_DEVICE_T *l_pstDevice = NULL;
	uint32_t l_ulControl = 0;

	if ((l_eReturnCode == HAL_SUCCESS) && ((p_pstFlags == NULL) || (p_pstFlags->m_ucPeripheralSize > HAL_DMA_SIZE__32) || (p_pstFlags->m_ucMemorySize > HAL_DMA_SIZE__32)))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstDevice = &g_astHAL_DMA_Device[p_ucModule];
		l_pstState = &g_astHAL_DMA_State[p_ucModule][p_ucChannel];
		l_pstChannel = HAL_DMA_GetChannel(p_ucModule, p_ucChannel);

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&l_pstDevice->m_stClock);

		/* Stop the channel and clear its flags (they are cleared by writing 1). */
		l_pstChannel->CCR = 0;
		l_pstDevice->m_pstRegisters->IFCR = HAL_DMA_ALL_FLAGS << HAL_DMA_SHIFT(p_ucChannel);

		l_pstState->m_pfCompleteCallback = p_pstFlags->m_pfCompleteCallback;
		l_pstState->m_pfHalfCallback = p_pstFlags->m_pfHalfCallback;
		l_pstState->m_pfErrorCallback = p_pstFlags->m_pfErrorCallback;
		l_pstState->m_pvContext = p_pstFlags->m_pvContext;

		if (p_pstFlags->m_ucDirection == HAL_DMA_DIRECTION__MEMORY_TO_PERIPHERAL)
		{
			l_ulControl |= DMA_CCR_DIR;
		}

		if (p_pstFlags->m_ucCircular)
		{
			l_ulControl |= DMA_CCR_CIRC;
		}

		if (p_pstFlags->m_ucPeripheralIncrement)
		{
			l_ulControl |= DMA_CCR_PINC;
		}

		if (p_pstFlags->m_ucMemoryIncrement)
		{
			l_ulControl |= DMA_CCR_MINC;
		}

		if (p_pstFlags->m_pfErrorCallback != NULL)
		{
			l_ulControl |= DMA_CCR_TEIE;
		}

		l_ulControl |= ((uint32_t) p_pstFlags->m_ucPeripheralSize << DMA_CCR_PSIZE_Pos) | ((uint32_t) p_pstFlags->m_ucMemorySize << DMA_CCR_MSIZE_Pos) | ((uint32_t) p_pstFlags->m_ucPriority << DMA_CCR_PL_Pos);

		l_pstChannel->CCR = l_ulControl;

		l_pstState->m_ucConfigured = HAL_ENABLE;

		/* Configure the NVIC interrupt. */
		if ((p_pstFlags->m_pfCompleteCallback != NULL) || (p_pstFlags->m_pfHalfCallback != NULL) || (p_pstFlags->m_pfErrorCallback != NULL))
		{
			HAL_PRIVATE_EnableIRQ(l_pstDevice->m_aeIRQn[p_ucChannel], &p_pstFlags->m_stInterrupt);
		}
		else
		{
			NVIC_DisableIRQ(l_pstDevice->m_aeIRQn[p_ucChannel]);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DMA start
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @param[in] p_ulPeripheralAddress - the address of the peripheral data register
 * @param[in] p_ulMemoryAddress - the address of the buffer
 * @param[in] p_unCount - the number of items (not bytes) to transfer, at least 1
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 * @remarks   A transfer that is running is stopped first.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DMA_Start(uint8_t p_ucModule, uint8_t p_ucChannel, uint32_t p_ulPeripheralAddress, uint32_t p_ulMemoryAddress, uint16_t p_unCount)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_DMA_Check(p_ucModule, p_ucChannel, true);
	DMA_Channel_TypeDef *l_pstChannel;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_unCount == 0U))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstChannel = HAL_DMA_GetChannel(p_ucModule, p_ucChannel);

		/* The addresses and the count can only be written while the channel is disabled. */
		HAL_PRIVATE_Modify(&l_pstChannel->CCR, DMA_CCR_EN, 0);

		g_astHAL_DMA_Device[p_ucModule].m_pstRegisters->IFCR = HAL_DMA_ALL_FLAGS << HAL_DMA_SHIFT(p_ucChannel);

		l_pstChannel->CPAR = p_ulPeripheralAddress;
		l_pstChannel->CMAR = p_ulMemoryAddress;
		l_pstChannel->CNDTR = p_unCount;

		HAL_PRIVATE_Modify(&l_pstChannel->CCR, 0, DMA_CCR_EN);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DMA stop
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DMA_Stop(uint8_t p_ucModule, uint8_t p_ucChannel)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_DMA_Check(p_ucModule, p_ucChannel, true);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		HAL_PRIVATE_Modify(&HAL_DMA_GetChannel(p_ucModule, p_ucChannel)->CCR, DMA_CCR_EN, 0);

		g_astHAL_DMA_Device[p_ucModule].m_pstRegisters->IFCR = HAL_DMA_ALL_FLAGS << HAL_DMA_SHIFT(p_ucChannel);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DMA set interrupt
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @param[in] p_ucInterrupt - the DMA interrupt (e.g. HAL_DMA_COMPLETE)
 * @param[in] p_ucState - the DMA interrupt state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED when the channel was
 *            not configured or no callback was given for this interrupt
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DMA_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucChannel, uint8_t p_ucInterrupt, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_DMA_Check(p_ucModule, p_ucChannel, true);
	uint32_t l_ulMask = 0;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucInterrupt == HAL_DMA_COMPLETE)
		{
			l_ulMask = DMA_CCR_TCIE;

			if (g_astHAL_DMA_State[p_ucModule][p_ucChannel].m_pfCompleteCallback == NULL)
			{
				l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
			}
		}
		else if (p_ucInterrupt == HAL_DMA_HALF)
		{
			l_ulMask = DMA_CCR_HTIE;

			if (g_astHAL_DMA_State[p_ucModule][p_ucChannel].m_pfHalfCallback == NULL)
			{
				l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
			}
		}
		else
		{
			l_eReturnCode = HAL_ERROR__PARAMETER;
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucState)
		{
			HAL_PRIVATE_Modify(&HAL_DMA_GetChannel(p_ucModule, p_ucChannel)->CCR, 0, l_ulMask);
		}
		else
		{
			HAL_PRIVATE_Modify(&HAL_DMA_GetChannel(p_ucModule, p_ucChannel)->CCR, l_ulMask, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      DMA get remaining
 * @param[in]  p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in]  p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 * @param[out] p_punCount - the number of items still to transfer
 * @return     HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_DMA_GetRemaining(uint8_t p_ucModule, uint8_t p_ucChannel, uint16_t *p_punCount)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_DMA_Check(p_ucModule, p_ucChannel, true);

	if ((l_eReturnCode == HAL_SUCCESS) && (p_punCount == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		*p_punCount = (uint16_t) HAL_DMA_GetChannel(p_ucModule, p_ucChannel)->CNDTR;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     DMA interrupt handling
 * @param[in] p_ucModule - the DMA module (e.g. HAL_DMA_0)
 * @param[in] p_ucChannel - the DMA channel (e.g. HAL_DMA_CHANNEL_0)
 **************************************************************************************************/
static void HAL_DMA_Interrupt(uint8_t p_ucModule, uint8_t p_ucChannel)
{
	const HAL_DMA_CHANNEL_STATE_T *l_pstState = &g_astHAL_DMA_State[p_ucModule][p_ucChannel];
	DMA_TypeDef *l_pstDma = g_astHAL_DMA_Device[p_ucModule].m_pstRegisters;
	uint32_t l_ulControl = HAL_DMA_GetChannel(p_ucModule, p_ucChannel)->CCR;
	uint32_t l_ulShift = HAL_DMA_SHIFT(p_ucChannel);
	uint32_t l_ulStatus = l_pstDma->ISR >> l_ulShift;

	/* The flags are cleared by writing 1 to their clear bits. */
	if ((l_ulStatus & DMA_ISR_TEIF1) && (l_ulControl & DMA_CCR_TEIE))
	{
		l_pstDma->IFCR = DMA_IFCR_CTEIF1 << l_ulShift;

		if (l_pstState->m_pfErrorCallback != NULL)
		{
			l_pstState->m_pfErrorCallback(l_pstState->m_pvContext);
		}
	}

	if ((l_ulStatus & DMA_ISR_HTIF1) && (l_ulControl & DMA_CCR_HTIE))
	{
		l_pstDma->IFCR = DMA_IFCR_CHTIF1 << l_ulShift;

		if (l_pstState->m_pfHalfCallback != NULL)
		{
			l_pstState->m_pfHalfCallback(l_pstState->m_pvContext);
		}
	}

	if ((l_ulStatus & DMA_ISR_TCIF1) && (l_ulControl & DMA_CCR_TCIE))
	{
		l_pstDma->IFCR = DMA_IFCR_CTCIF1 << l_ulShift;

		if (l_pstState->m_pfCompleteCallback != NULL)
		{
			l_pstState->m_pfCompleteCallback(l_pstState->m_pvContext);
		}
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_DMA
#include "hal_family_irq.h"
