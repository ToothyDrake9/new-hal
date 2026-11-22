/**************************************************************************************************
 * @file    hal_spi_v1.c
 * @brief   Microprocessor SPI module, for the SPI without FIFO (L1, F1, F2, F4)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the control register is written as a whole so a module can be configured
 *          again, the prescaler comes from the bus clock, no chip select is driven here, errors
 *          and timeouts are reported, the NVIC line is configured
 **************************************************************************************************/
#include "hal_spi.h"
#include "hal_clock.h"
#include "hal_calc.h"
#include "hal_private.h"

typedef struct
{
	HAL_SPI_RX_CALLBACK_T m_pfRxCallback;
	HAL_SPI_TX_CALLBACK_T m_pfTxCallback;
	HAL_SPI_ERROR_CALLBACK_T m_pfErrorCallback;
	void *m_pvContext;
	uint8_t m_ucConfigured;
}HAL_SPI_MODULE_STATE_T;

static HAL_SPI_MODULE_STATE_T g_astHAL_SPI_State[HAL_SPI_NO];

/**************************************************************************************************
 * @brief     Checks a module argument
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_SPI_Check(uint8_t p_ucModule)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ucModule >= HAL_SPI_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_SPI_State[p_ucModule].m_ucConfigured)
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
 * @brief     Clears the error flags of a module
 * @param[in] p_pstSpi - the SPI registers
 * @param[in] p_ulStatus - the status register value that showed the errors
 * @return    The errors, as HAL_SPI_ERROR__ bits
 **************************************************************************************************/
static uint8_t HAL_SPI_ClearErrors(SPI_TypeDef *p_pstSpi, uint32_t p_ulStatus)
{
	volatile uint32_t l_ulDummy;
	uint8_t l_ucErrors = 0;

	if (p_ulStatus & SPI_SR_OVR)
	{
		/* Cleared by reading the data register, then the status register. */
		l_ulDummy = p_pstSpi->DR;
		l_ulDummy = p_pstSpi->SR;

		l_ucErrors |= HAL_SPI_ERROR__OVERRUN;
	}

	if (p_ulStatus & SPI_SR_MODF)
	{
		/* Cleared by reading the status register, then writing the control register. */
		l_ulDummy = p_pstSpi->SR;
		p_pstSpi->CR1 = p_pstSpi->CR1;

		l_ucErrors |= HAL_SPI_ERROR__MODE_FAULT;
	}

	(void) l_ulDummy;

	return l_ucErrors;
}

/**************************************************************************************************
 * @brief     SPI configuration
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in] p_pstFlags - the SPI flags, pins and callbacks
 * @return    HAL_SUCCESS, or HAL_ERROR__PARAMETER (bad argument, or a master speed below what
 *            the bus clock divided by 256 gives)
 * @remarks   Frames are 8 bits. The module is left disabled: call HAL_SPI_SetState. In master
 *            mode the chip select of each device is a plain GPIO driven by the caller.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_Config(uint8_t p_ucModule, const HAL_SPI_FLAGS_T *p_pstFlags)
{
	static const HAL_GPIO_FLAGS_T l_stPinFlags = { HAL_GPIO_MODE__FUNCTION, HAL_GPIO_TYPE__PUSH_PULL, HAL_GPIO_PULL__NO_PULL, HAL_GPIO_SPEED__VERY_HIGH, 0 };
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	const HAL_SPI_DEVICE_T *l_pstDevice = NULL;
	HAL_SPI_MODULE_STATE_T *l_pstState;
	SPI_TypeDef *l_pstSpi;
	uint32_t l_ulControl = 0;
	uint32_t l_ulSpeed = 0;
	uint8_t l_ucCode = 0;

	if ((p_ucModule >= HAL_SPI_NO) || (p_pstFlags == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else
	{
		l_pstDevice = &g_astHAL_SPI_Device[p_ucModule];

		if (p_pstFlags->m_ucMode == HAL_SPI_MODE__MASTER)
		{
			/* Set the speed, from the clock of the bus the module sits on. */
			l_eReturnCode = HAL_CALC_SpiPrescaler(HAL_CLOCK_GetBus(l_pstDevice->m_ucBus), p_pstFlags->m_ulSpeed, &l_ucCode, &l_ulSpeed);

			/* Software slave management, with the internal select held high. */
			l_ulControl |= SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | ((uint32_t) l_ucCode << SPI_CR1_BR_Pos);
		}
		else if (p_pstFlags->m_stSsPin.m_ucModule == HAL_GPIO_NONE)
		{
			/* Slave that is always selected. */
			l_ulControl |= SPI_CR1_SSM;
		}
		else
		{
			/* Slave selected by its hardware input. */
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stClkPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stMisoPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stMosiPin, &l_stPinFlags);
	}

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pstFlags->m_ucMode == HAL_SPI_MODE__SLAVE))
	{
		l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stSsPin, &l_stPinFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstSpi = l_pstDevice->m_pstRegisters;
		l_pstState = &g_astHAL_SPI_State[p_ucModule];

		/* Enable the module peripheral clock. */
		HAL_PRIVATE_ClockEnable(&l_pstDevice->m_stClock);

		/* Disable the module and all its interrupt sources. */
		l_pstSpi->CR1 = 0;
		l_pstSpi->CR2 = 0;
		l_pstSpi->I2SCFGR &= ~SPI_I2SCFGR_I2SMOD;

		l_pstState->m_ucConfigured = HAL_DISABLE;
		l_pstState->m_pfRxCallback = p_pstFlags->m_pfRxCallback;
		l_pstState->m_pfTxCallback = p_pstFlags->m_pfTxCallback;
		l_pstState->m_pfErrorCallback = p_pstFlags->m_pfErrorCallback;
		l_pstState->m_pvContext = p_pstFlags->m_pvContext;

		/* Set the phase, the polarity and the bit order. */
		if (p_pstFlags->m_ucPhase == HAL_SPI_PHASE__SECOND_EDGE)
		{
			l_ulControl |= SPI_CR1_CPHA;
		}

		if (p_pstFlags->m_ucPolarity == HAL_SPI_POLARITY__HIGH)
		{
			l_ulControl |= SPI_CR1_CPOL;
		}

		if (p_pstFlags->m_ucBitOrder == HAL_SPI_BIT_ORDER__LSB_FIRST)
		{
			l_ulControl |= SPI_CR1_LSBFIRST;
		}

		l_pstSpi->CR1 = l_ulControl;

		l_pstState->m_ucConfigured = HAL_ENABLE;

		/* Configure the NVIC interrupt. */
		if ((p_pstFlags->m_pfRxCallback != NULL) || (p_pstFlags->m_pfTxCallback != NULL))
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
 * @brief     SPI set state
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in] p_ucState - the SPI state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SPI_Check(p_ucModule);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucState)
		{
			HAL_PRIVATE_Modify(&g_astHAL_SPI_Device[p_ucModule].m_pstRegisters->CR1, 0, SPI_CR1_SPE);
		}
		else
		{
			HAL_PRIVATE_Modify(&g_astHAL_SPI_Device[p_ucModule].m_pstRegisters->CR1, SPI_CR1_SPE, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     SPI set interrupt
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in] p_ucInterrupt - the SPI interrupt (e.g. HAL_SPI_RX)
 * @param[in] p_ucState - the SPI interrupt state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, or HAL_ERROR__NOT_CONFIGURED when the module was
 *            not configured or no callback was given for this interrupt
 * @remarks   The error interrupt follows HAL_SPI_RX. Do not use the blocking transfer functions
 *            while an interrupt source of the module is enabled.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SPI_Check(p_ucModule);
	uint32_t l_ulMask = 0;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucInterrupt == HAL_SPI_RX)
		{
			l_ulMask = SPI_CR2_RXNEIE | SPI_CR2_ERRIE;

			if (g_astHAL_SPI_State[p_ucModule].m_pfRxCallback == NULL)
			{
				l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
			}
		}
		else if (p_ucInterrupt == HAL_SPI_TX)
		{
			l_ulMask = SPI_CR2_TXEIE;

			if (g_astHAL_SPI_State[p_ucModule].m_pfTxCallback == NULL)
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
			HAL_PRIVATE_Modify(&g_astHAL_SPI_Device[p_ucModule].m_pstRegisters->CR2, 0, l_ulMask);
		}
		else
		{
			HAL_PRIVATE_Modify(&g_astHAL_SPI_Device[p_ucModule].m_pstRegisters->CR2, l_ulMask, 0);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      SPI get speed: the clock that the prescaler really produces
 * @param[in]  p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[out] p_pulSpeed - the clock in Hz
 * @return     HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_GetSpeed(uint8_t p_ucModule, uint32_t *p_pulSpeed)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SPI_Check(p_ucModule);
	uint32_t l_ulCode;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pulSpeed == NULL))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulCode = (g_astHAL_SPI_Device[p_ucModule].m_pstRegisters->CR1 & SPI_CR1_BR) >> SPI_CR1_BR_Pos;

		*p_pulSpeed = HAL_CLOCK_GetBus(g_astHAL_SPI_Device[p_ucModule].m_ucBus) >> (l_ulCode + 1U);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      Exchanges one byte, without checking the module argument
 * @param[in]  p_pstSpi - the SPI registers
 * @param[in]  p_ucByte - the byte to be written
 * @param[out] p_pucByte - the byte that was received
 * @return     HAL_SUCCESS, HAL_ERROR__TIMEOUT or HAL_ERROR__BUS
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_SPI_Exchange(SPI_TypeDef *p_pstSpi, uint8_t p_ucByte, uint8_t *p_pucByte)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	uint32_t l_ulStatus;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_SPI_TIMEOUT_MS);

	/* Wait until the last written byte has been transferred. */
	while (!(p_pstSpi->SR & SPI_SR_TXE))
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;

			break;
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		*(volatile uint8_t *) &p_pstSpi->DR = p_ucByte;

		/* Wait until a byte is received. */
		for (;;)
		{
			l_ulStatus = p_pstSpi->SR;

			if (l_ulStatus & (SPI_SR_OVR | SPI_SR_MODF))
			{
				(void) HAL_SPI_ClearErrors(p_pstSpi, l_ulStatus);
				l_eReturnCode = HAL_ERROR__BUS;

				break;
			}

			if (l_ulStatus & SPI_SR_RXNE)
			{
				*p_pucByte = *(volatile uint8_t *) &p_pstSpi->DR;

				break;
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
 * @brief      SPI transfer byte
 * @param[in]  p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in]  p_ucByte - the byte to be written
 * @param[out] p_pucByte - the byte that was received; NULL when it is not wanted
 * @return     HAL_SUCCESS, HAL_ERROR__TIMEOUT (module disabled, or a slave that got no clock),
 *             HAL_ERROR__BUS (overrun or mode fault), HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_TransferByte(uint8_t p_ucModule, uint8_t p_ucByte, uint8_t *p_pucByte)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SPI_Check(p_ucModule);
	uint8_t l_ucByte = 0;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_SPI_Exchange(g_astHAL_SPI_Device[p_ucModule].m_pstRegisters, p_ucByte, &l_ucByte);

		if ((l_eReturnCode == HAL_SUCCESS) && (p_pucByte != NULL))
		{
			*p_pucByte = l_ucByte;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      SPI transfer: writes and reads a buffer
 * @param[in]  p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in]  p_pucTxBuff - the bytes to be written; NULL sends HAL_SPI_FILL_BYTE
 * @param[out] p_pucRxBuff - the bytes that were received; NULL drops them. It may be the same
 *             buffer as p_pucTxBuff.
 * @param[in]  p_unSize - the number of bytes
 * @return     Same as HAL_SPI_TransferByte; the transfer stops at the first error
 * @remarks    The chip select is not touched: drive it around this call.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_Transfer(uint8_t p_ucModule, const uint8_t *p_pucTxBuff, uint8_t *p_pucRxBuff, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SPI_Check(p_ucModule);
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	SPI_TypeDef *l_pstSpi;
	uint16_t l_unIndex;
	uint8_t l_ucByte = 0;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstSpi = g_astHAL_SPI_Device[p_ucModule].m_pstRegisters;

		for (l_unIndex = 0; (l_unIndex < p_unSize) && (l_eReturnCode == HAL_SUCCESS); l_unIndex++)
		{
			l_eReturnCode = HAL_SPI_Exchange(l_pstSpi, (p_pucTxBuff != NULL) ? p_pucTxBuff[l_unIndex] : (uint8_t) HAL_SPI_FILL_BYTE, &l_ucByte);

			if ((l_eReturnCode == HAL_SUCCESS) && (p_pucRxBuff != NULL))
			{
				p_pucRxBuff[l_unIndex] = l_ucByte;
			}
		}

		/* Wait until the bus is idle, so the caller can release the chip select. */
		if (l_eReturnCode == HAL_SUCCESS)
		{
			HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_SPI_TIMEOUT_MS);

			while (l_pstSpi->SR & SPI_SR_BSY)
			{
				if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
				{
					l_eReturnCode = HAL_ERROR__TIMEOUT;

					break;
				}
			}
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     SPI write buffer
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[in] p_pucBuff - the buffer to be written
 * @param[in] p_unSize - the buffer size
 * @return    Same as HAL_SPI_Transfer
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_WriteBuff(uint8_t p_ucModule, const uint8_t *p_pucBuff, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;

	if ((p_pucBuff != NULL) || (p_unSize == 0U))
	{
		l_eReturnCode = HAL_SPI_Transfer(p_ucModule, p_pucBuff, NULL, p_unSize);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      SPI read buffer
 * @param[in]  p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @param[out] p_pucBuff - the buffer that receives the bytes
 * @param[in]  p_unSize - the buffer size
 * @return     Same as HAL_SPI_Transfer
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_SPI_ReadBuff(uint8_t p_ucModule, uint8_t *p_pucBuff, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;

	if ((p_pucBuff != NULL) || (p_unSize == 0U))
	{
		l_eReturnCode = HAL_SPI_Transfer(p_ucModule, NULL, p_pucBuff, p_unSize);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     SPI interrupt handling
 * @param[in] p_ucModule - the SPI module (e.g. HAL_SPI_0)
 * @remarks   The data register is read here, so an empty callback cannot leave the interrupt
 *            pending forever.
 **************************************************************************************************/
static void HAL_SPI_Interrupt(uint8_t p_ucModule)
{
	const HAL_SPI_MODULE_STATE_T *l_pstState = &g_astHAL_SPI_State[p_ucModule];
	SPI_TypeDef *l_pstSpi = g_astHAL_SPI_Device[p_ucModule].m_pstRegisters;
	uint32_t l_ulStatus = l_pstSpi->SR;
	uint32_t l_ulControl = l_pstSpi->CR2;
	uint8_t l_ucErrors;
	uint8_t l_ucByte = 0;

	/* Reception. */
	if ((l_ulControl & SPI_CR2_RXNEIE) && (l_ulStatus & SPI_SR_RXNE))
	{
		l_ucByte = *(volatile uint8_t *) &l_pstSpi->DR;

		if (l_pstState->m_pfRxCallback != NULL)
		{
			l_pstState->m_pfRxCallback(l_pstState->m_pvContext, l_ucByte);
		}
	}

	/* Errors. */
	if ((l_ulControl & SPI_CR2_ERRIE) && (l_ulStatus & (SPI_SR_OVR | SPI_SR_MODF)))
	{
		l_ucErrors = HAL_SPI_ClearErrors(l_pstSpi, l_ulStatus);

		if (l_pstState->m_pfErrorCallback != NULL)
		{
			l_pstState->m_pfErrorCallback(l_pstState->m_pvContext, l_ucErrors);
		}
	}

	/* Transmission: the callback gives the next byte, or the source is switched off. */
	if ((l_ulControl & SPI_CR2_TXEIE) && (l_ulStatus & SPI_SR_TXE))
	{
		if ((l_pstState->m_pfTxCallback != NULL) && (l_pstState->m_pfTxCallback(l_pstState->m_pvContext, &l_ucByte) == HAL_SPI_TX__SEND))
		{
			*(volatile uint8_t *) &l_pstSpi->DR = l_ucByte;
		}
		else
		{
			l_pstSpi->CR2 &= ~SPI_CR2_TXEIE;
		}
	}
}

/* Interrupt vectors of this family. */
#define HAL_FAMILY_IRQ_SPI
#include "hal_family_irq.h"
