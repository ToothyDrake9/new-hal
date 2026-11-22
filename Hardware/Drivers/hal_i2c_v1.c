/**************************************************************************************************
 * @file    hal_i2c_v1.c
 * @brief   Microprocessor I2C module (master), for the I2C with CCR/TRISE registers (L1, F1, F2, F4)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the timing is computed from the bus clock, a transfer checks for a missing
 *          acknowledge, bus error and arbitration loss at every step and has a timeout at every
 *          step, receptions of 1 byte, 2 bytes and more follow the three sequences of the
 *          reference manual, a stuck bus is recovered
 **************************************************************************************************/
#include "hal_i2c.h"
#include "hal_clock.h"
#include "hal_calc.h"
#include "hal_private.h"

#define HAL_I2C_ERROR_FLAGS (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)

typedef struct
{
	HAL_GPIO_PIN_T m_stSclPin;
	HAL_GPIO_PIN_T m_stSdaPin;
	uint16_t m_unFrequency;
	uint16_t m_unClockControl;
	uint16_t m_unRiseTime;
	uint8_t m_ucConfigured;
	uint8_t m_ucEnabled;
	uint8_t m_ucHolding;   // A transfer ended without stop: the bus is ours and the clock is held low
}HAL_I2C_MODULE_STATE_T;

static HAL_I2C_MODULE_STATE_T g_astHAL_I2C_State[HAL_I2C_NO];

static const HAL_GPIO_FLAGS_T g_stHAL_I2C_PinFlags = { HAL_GPIO_MODE__FUNCTION, HAL_GPIO_TYPE__OPEN_DRAIN, HAL_GPIO_PULL__PULL_UP, HAL_GPIO_SPEED__MEDIUM, 0 };

/**************************************************************************************************
 * @brief     Waits for a flag of the status register 1, watching the error flags
 * @param[in] p_pstI2c - the I2C registers
 * @param[in] p_ulFlag - the flag to wait for
 * @return    HAL_SUCCESS, HAL_ERROR__NACK, HAL_ERROR__BUS or HAL_ERROR__TIMEOUT
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_WaitFlag(const I2C_TypeDef *p_pstI2c, uint32_t p_ulFlag)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	uint32_t l_ulStatus;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_I2C_TIMEOUT_MS);

	for (;;)
	{
		l_ulStatus = p_pstI2c->SR1;

		if (l_ulStatus & I2C_SR1_AF)
		{
			l_eReturnCode = HAL_ERROR__NACK;

			break;
		}

		if (l_ulStatus & (I2C_SR1_BERR | I2C_SR1_ARLO))
		{
			l_eReturnCode = HAL_ERROR__BUS;

			break;
		}

		if (l_ulStatus & p_ulFlag)
		{
			break;
		}

		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;

			break;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Waits until the hardware has sent the stop condition
 * @param[in] p_pstI2c - the I2C registers
 * @return    HAL_SUCCESS or HAL_ERROR__TIMEOUT
 * @remarks   The control register must not be written while the stop bit is set.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_WaitStop(const I2C_TypeDef *p_pstI2c)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_I2C_TIMEOUT_MS);

	while (p_pstI2c->CR1 & I2C_CR1_STOP)
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
 * @brief     Clears the address flag: status register 1 then status register 2 must be read
 * @param[in] p_pstI2c - the I2C registers
 **************************************************************************************************/
static void HAL_I2C_ClearAddress(const I2C_TypeDef *p_pstI2c)
{
	volatile uint32_t l_ulDummy;

	l_ulDummy = p_pstI2c->SR1;
	l_ulDummy = p_pstI2c->SR2;

	(void) l_ulDummy;
}

/**************************************************************************************************
 * @brief     Generates a start (or repeated start) condition and sends the address byte
 * @param[in] p_pstI2c - the I2C registers
 * @param[in] p_ucAddressByte - the 7 bit address shifted left, with the read bit
 * @return    HAL_SUCCESS, HAL_ERROR__NACK (no device answers), HAL_ERROR__BUS or HAL_ERROR__TIMEOUT
 * @remarks   On success the address flag is still set: the caller clears it, at the point that
 *            the sequence it runs requires.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_Start(I2C_TypeDef *p_pstI2c, uint8_t p_ucAddressByte)
{
	HAL_RETURN_CODE_T l_eReturnCode;

	/* Generate a start condition. */
	p_pstI2c->CR1 |= I2C_CR1_START;

	l_eReturnCode = HAL_I2C_WaitFlag(p_pstI2c, I2C_SR1_SB);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Write the slave address. */
		p_pstI2c->DR = p_ucAddressByte;

		l_eReturnCode = HAL_I2C_WaitFlag(p_pstI2c, I2C_SR1_ADDR);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Writes the timing registers and the enable bit from the stored configuration
 * @param[in] p_ucModule - the I2C module, already checked
 **************************************************************************************************/
static void HAL_I2C_Apply(uint8_t p_ucModule)
{
	const HAL_I2C_MODULE_STATE_T *l_pstState = &g_astHAL_I2C_State[p_ucModule];
	I2C_TypeDef *l_pstI2c = g_astHAL_I2C_Device[p_ucModule].m_pstRegisters;

	/* Reset the module: this also frees its state machine after a bus fault. */
	l_pstI2c->CR1 = I2C_CR1_SWRST;
	l_pstI2c->CR1 = 0;

	l_pstI2c->CR2 = l_pstState->m_unFrequency;
	l_pstI2c->CCR = l_pstState->m_unClockControl;
	l_pstI2c->TRISE = l_pstState->m_unRiseTime;

	if (l_pstState->m_ucEnabled)
	{
		l_pstI2c->CR1 = I2C_CR1_PE;
	}
}

/**************************************************************************************************
 * @brief     Sends the address and a buffer
 * @param[in] p_ucModule - the I2C module, already checked
 * @param[in] p_ucAddress - the 7 bit slave address
 * @param[in] p_pucBuff - the buffer to be written
 * @param[in] p_unSize - the buffer size; 0 sends the address only
 * @param[in] p_ucStop - HAL_I2C_STOP or HAL_I2C_NO_STOP
 * @return    HAL_SUCCESS, HAL_ERROR__NACK, HAL_ERROR__BUS or HAL_ERROR__TIMEOUT
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_Send(uint8_t p_ucModule, uint8_t p_ucAddress, const uint8_t *p_pucBuff, uint16_t p_unSize, uint8_t p_ucStop)
{
	I2C_TypeDef *l_pstI2c = g_astHAL_I2C_Device[p_ucModule].m_pstRegisters;
	HAL_RETURN_CODE_T l_eReturnCode;
	uint16_t l_unIndex;

	l_eReturnCode = HAL_I2C_Start(l_pstI2c, (uint8_t) (p_ucAddress << 1));

	if (l_eReturnCode == HAL_SUCCESS)
	{
		HAL_I2C_ClearAddress(l_pstI2c);
	}

	for (l_unIndex = 0; (l_unIndex < p_unSize) && (l_eReturnCode == HAL_SUCCESS); l_unIndex++)
	{
		/* Wait until the data register is free, then write the byte. */
		l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_TXE);

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_pstI2c->DR = p_pucBuff[l_unIndex];
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Wait until the last byte has been sent and acknowledged. */
		l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, (p_unSize != 0U) ? I2C_SR1_BTF : I2C_SR1_TXE);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_ucStop)
		{
			/* Generate a stop condition. */
			l_pstI2c->CR1 |= I2C_CR1_STOP;

			g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_DISABLE;

			l_eReturnCode = HAL_I2C_WaitStop(l_pstI2c);
		}
		else
		{
			g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_ENABLE;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      Sends the address and receives a buffer, then generates a stop condition
 * @param[in]  p_ucModule - the I2C module, already checked
 * @param[in]  p_ucAddress - the 7 bit slave address
 * @param[out] p_pucBuff - the buffer that receives the bytes
 * @param[in]  p_unSize - the number of bytes, at least 1
 * @return     HAL_SUCCESS, HAL_ERROR__NACK, HAL_ERROR__BUS or HAL_ERROR__TIMEOUT
 * @remarks    The master must refuse to acknowledge the last byte and program the stop before
 *             that byte is complete. Because the hardware works one byte ahead, this needs a
 *             different sequence for 1 byte, 2 bytes and more than 2 bytes. Interrupts are masked
 *             for the few instructions where a delay would let one byte too many through.
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_Receive(uint8_t p_ucModule, uint8_t p_ucAddress, uint8_t *p_pucBuff, uint16_t p_unSize)
{
	I2C_TypeDef *l_pstI2c = g_astHAL_I2C_Device[p_ucModule].m_pstRegisters;
	HAL_RETURN_CODE_T l_eReturnCode;
	uint32_t l_ulState;
	uint16_t l_unIndex = 0;

	/* Acknowledge the received bytes, of the current byte position. */
	l_pstI2c->CR1 = (l_pstI2c->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;

	l_eReturnCode = HAL_I2C_Start(l_pstI2c, (uint8_t) ((p_ucAddress << 1) | 1U));

	g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_DISABLE;

	if (l_eReturnCode == HAL_SUCCESS)
	{
		if (p_unSize == 1U)
		{
			/* Refuse the acknowledge before the reception starts, and program the stop at once. */
			l_ulState = HAL_PRIVATE_EnterCritical();

			l_pstI2c->CR1 &= ~I2C_CR1_ACK;
			HAL_I2C_ClearAddress(l_pstI2c);
			l_pstI2c->CR1 |= I2C_CR1_STOP;

			HAL_PRIVATE_ExitCritical(l_ulState);

			l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_RXNE);

			if (l_eReturnCode == HAL_SUCCESS)
			{
				p_pucBuff[0] = (uint8_t) l_pstI2c->DR;
			}
		}
		else if (p_unSize == 2U)
		{
			/* The refusal is aimed at the second byte: the one in the shift register. */
			l_ulState = HAL_PRIVATE_EnterCritical();

			l_pstI2c->CR1 = (l_pstI2c->CR1 & ~I2C_CR1_ACK) | I2C_CR1_POS;
			HAL_I2C_ClearAddress(l_pstI2c);

			HAL_PRIVATE_ExitCritical(l_ulState);

			/* Wait until both bytes are received: the clock is then held low. */
			l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_BTF);

			if (l_eReturnCode == HAL_SUCCESS)
			{
				l_ulState = HAL_PRIVATE_EnterCritical();

				l_pstI2c->CR1 |= I2C_CR1_STOP;
				p_pucBuff[0] = (uint8_t) l_pstI2c->DR;

				HAL_PRIVATE_ExitCritical(l_ulState);

				p_pucBuff[1] = (uint8_t) l_pstI2c->DR;
			}
		}
		else
		{
			HAL_I2C_ClearAddress(l_pstI2c);

			/* All the bytes but the last three are read as they come. */
			while ((l_eReturnCode == HAL_SUCCESS) && ((uint16_t) (p_unSize - l_unIndex) > 3U))
			{
				l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_RXNE);

				if (l_eReturnCode == HAL_SUCCESS)
				{
					p_pucBuff[l_unIndex++] = (uint8_t) l_pstI2c->DR;
				}
			}

			/* Third byte from the end received, second from the end in the shift register. */
			if (l_eReturnCode == HAL_SUCCESS)
			{
				l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_BTF);
			}

			if (l_eReturnCode == HAL_SUCCESS)
			{
				/* Refuse the acknowledge of the last byte, then let the reception go on. */
				l_pstI2c->CR1 &= ~I2C_CR1_ACK;
				p_pucBuff[l_unIndex++] = (uint8_t) l_pstI2c->DR;

				/* Second byte from the end received, last byte in the shift register. */
				l_eReturnCode = HAL_I2C_WaitFlag(l_pstI2c, I2C_SR1_BTF);
			}

			if (l_eReturnCode == HAL_SUCCESS)
			{
				l_ulState = HAL_PRIVATE_EnterCritical();

				l_pstI2c->CR1 |= I2C_CR1_STOP;
				p_pucBuff[l_unIndex++] = (uint8_t) l_pstI2c->DR;

				HAL_PRIVATE_ExitCritical(l_ulState);

				p_pucBuff[l_unIndex] = (uint8_t) l_pstI2c->DR;
			}
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_I2C_WaitStop(l_pstI2c);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_pstI2c->CR1 &= ~I2C_CR1_POS;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Ends a transfer that failed and leaves the bus free
 * @param[in] p_ucModule - the I2C module, already checked
 **************************************************************************************************/
static void HAL_I2C_Abort(uint8_t p_ucModule)
{
	I2C_TypeDef *l_pstI2c = g_astHAL_I2C_Device[p_ucModule].m_pstRegisters;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	bool l_bBusy = false;

	g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_DISABLE;

	/* Release the bus. The stop bit is written last: the register must not be written after it. */
	HAL_I2C_ClearAddress(l_pstI2c);
	l_pstI2c->CR1 = (l_pstI2c->CR1 & ~(I2C_CR1_POS | I2C_CR1_START)) | I2C_CR1_STOP;

	/* Clear the error flags: write 0 to them and 1 to all the others. */
	l_pstI2c->SR1 = (uint32_t) ~HAL_I2C_ERROR_FLAGS;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_I2C_TIMEOUT_MS);

	while (l_pstI2c->SR2 & I2C_SR2_BUSY)
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_bBusy = true;

			break;
		}
	}

	if (l_bBusy || (l_pstI2c->CR1 & I2C_CR1_STOP))
	{
		(void) HAL_I2C_Recover(p_ucModule);
	}
}

/**************************************************************************************************
 * @brief     Checks the module and waits for the bus to be free
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in] p_ucAddress - the 7 bit slave address
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED or HAL_ERROR__BUSY
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_I2C_Begin(uint8_t p_ucModule, uint8_t p_ucAddress)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	const I2C_TypeDef *l_pstI2c;

	if ((p_ucModule >= HAL_I2C_NO) || (p_ucAddress > 0x7FU))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if ((!g_astHAL_I2C_State[p_ucModule].m_ucConfigured) || (!g_astHAL_I2C_State[p_ucModule].m_ucEnabled))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else if (!g_astHAL_I2C_State[p_ucModule].m_ucHolding)
	{
		l_pstI2c = g_astHAL_I2C_Device[p_ucModule].m_pstRegisters;

		HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_I2C_TIMEOUT_MS);

		while (l_pstI2c->SR2 & I2C_SR2_BUSY)
		{
			if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
			{
				/* Nobody should hold the bus this long: try to free it once. */
				if (HAL_I2C_Recover(p_ucModule) != HAL_SUCCESS)
				{
					l_eReturnCode = HAL_ERROR__BUSY;
				}

				break;
			}
		}
	}
	else
	{
		/* The bus is held by the previous transfer: a repeated start follows. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     I2C configuration
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in] p_pstFlags - the I2C speed and pins
 * @return    HAL_SUCCESS, or HAL_ERROR__PARAMETER (bad argument, or a speed that the bus clock
 *            cannot produce: it must be at least 2 MHz, 4 MHz above 100 kHz)
 * @remarks   The pins are open drain with the internal pull-up, which is too weak to be the only
 *            one: the bus needs external pull-up resistors. The module is left disabled.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_Config(uint8_t p_ucModule, const HAL_I2C_FLAGS_T *p_pstFlags)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	const HAL_I2C_DEVICE_T *l_pstDevice = NULL;
	HAL_I2C_MODULE_STATE_T *l_pstState;

	if ((p_ucModule < HAL_I2C_NO) && (p_pstFlags != NULL) && (p_pstFlags->m_stSclPin.m_ucModule != HAL_GPIO_NONE) && (p_pstFlags->m_stSdaPin.m_ucModule != HAL_GPIO_NONE))
	{
		l_pstDevice = &g_astHAL_I2C_Device[p_ucModule];
		l_pstState = &g_astHAL_I2C_State[p_ucModule];

		l_pstState->m_ucConfigured = HAL_DISABLE;

		/* Set the timing, from the clock of the bus the module sits on. */
		l_eReturnCode = HAL_CALC_I2cTiming(HAL_CLOCK_GetBus(l_pstDevice->m_ucBus), p_pstFlags->m_ulSpeed, &l_pstState->m_unFrequency, &l_pstState->m_unClockControl, &l_pstState->m_unRiseTime);

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stSclPin, &g_stHAL_I2C_PinFlags);
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_eReturnCode = HAL_GPIO_ConfigPin(&p_pstFlags->m_stSdaPin, &g_stHAL_I2C_PinFlags);
		}

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_pstState->m_stSclPin = p_pstFlags->m_stSclPin;
			l_pstState->m_stSdaPin = p_pstFlags->m_stSdaPin;
			l_pstState->m_ucEnabled = HAL_DISABLE;
			l_pstState->m_ucHolding = HAL_DISABLE;

			/* Enable the module peripheral clock. */
			HAL_PRIVATE_ClockEnable(&l_pstDevice->m_stClock);

			HAL_I2C_Apply(p_ucModule);

			l_pstState->m_ucConfigured = HAL_ENABLE;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     I2C set state
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in] p_ucState - the I2C state (e.g. HAL_ENABLE)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER or HAL_ERROR__NOT_CONFIGURED
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_SetState(uint8_t p_ucModule, uint8_t p_ucState)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if (p_ucModule >= HAL_I2C_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_I2C_State[p_ucModule].m_ucConfigured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		g_astHAL_I2C_State[p_ucModule].m_ucEnabled = p_ucState ? HAL_ENABLE : HAL_DISABLE;
		g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_DISABLE;

		g_astHAL_I2C_Device[p_ucModule].m_pstRegisters->CR1 = p_ucState ? I2C_CR1_PE : 0U;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     I2C write
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in] p_ucAddress - the 7 bit slave address (e.g. 0x50)
 * @param[in] p_pucBuff - the buffer to be written
 * @param[in] p_unSize - the buffer size; 0 sends the address only
 * @param[in] p_ucStop - HAL_I2C_STOP, or HAL_I2C_NO_STOP to follow with a repeated start
 * @return    HAL_SUCCESS, HAL_ERROR__NACK (no device at this address, or a byte was refused),
 *            HAL_ERROR__BUS (bus error or arbitration lost), HAL_ERROR__TIMEOUT, HAL_ERROR__BUSY
 *            (the bus is held by something else), HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED
 * @remarks   After any error a stop condition is generated and the bus is released.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_Write(uint8_t p_ucModule, uint8_t p_ucAddress, const uint8_t *p_pucBuff, uint16_t p_unSize, uint8_t p_ucStop)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_I2C_Begin(p_ucModule, p_ucAddress);

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pucBuff == NULL) && (p_unSize != 0U))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_I2C_Send(p_ucModule, p_ucAddress, p_pucBuff, p_unSize, p_ucStop);

		if (l_eReturnCode != HAL_SUCCESS)
		{
			HAL_I2C_Abort(p_ucModule);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      I2C read
 * @param[in]  p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in]  p_ucAddress - the 7 bit slave address (e.g. 0x50)
 * @param[out] p_pucBuff - the buffer that receives the bytes
 * @param[in]  p_unSize - the number of bytes to read, at least 1
 * @return     Same as HAL_I2C_Write
 * @remarks    The transfer always ends with a stop condition.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_Read(uint8_t p_ucModule, uint8_t p_ucAddress, uint8_t *p_pucBuff, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_I2C_Begin(p_ucModule, p_ucAddress);

	if ((l_eReturnCode == HAL_SUCCESS) && ((p_pucBuff == NULL) || (p_unSize == 0U)))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_I2C_Receive(p_ucModule, p_ucAddress, p_pucBuff, p_unSize);

		if (l_eReturnCode != HAL_SUCCESS)
		{
			HAL_I2C_Abort(p_ucModule);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief      I2C write then read, with a repeated start in between
 * @param[in]  p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in]  p_ucAddress - the 7 bit slave address (e.g. 0x50)
 * @param[in]  p_pucTxBuff - the bytes to be written first (typically a register or memory address)
 * @param[in]  p_unTxSize - the number of bytes to write, at least 1
 * @param[out] p_pucRxBuff - the buffer that receives the bytes
 * @param[in]  p_unRxSize - the number of bytes to read, at least 1
 * @return     Same as HAL_I2C_Write
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_WriteRead(uint8_t p_ucModule, uint8_t p_ucAddress, const uint8_t *p_pucTxBuff, uint16_t p_unTxSize, uint8_t *p_pucRxBuff, uint16_t p_unRxSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_I2C_Begin(p_ucModule, p_ucAddress);

	if ((l_eReturnCode == HAL_SUCCESS) && ((p_pucTxBuff == NULL) || (p_unTxSize == 0U) || (p_pucRxBuff == NULL) || (p_unRxSize == 0U)))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_I2C_Send(p_ucModule, p_ucAddress, p_pucTxBuff, p_unTxSize, HAL_I2C_NO_STOP);

		if (l_eReturnCode == HAL_SUCCESS)
		{
			l_eReturnCode = HAL_I2C_Receive(p_ucModule, p_ucAddress, p_pucRxBuff, p_unRxSize);
		}

		if (l_eReturnCode != HAL_SUCCESS)
		{
			HAL_I2C_Abort(p_ucModule);
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     I2C device check: sends the address and looks for the acknowledge
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @param[in] p_ucAddress - the 7 bit slave address (e.g. 0x50)
 * @return    HAL_SUCCESS when a device answers, HAL_ERROR__NACK when none does, or another error
 *            of HAL_I2C_Write
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_IsDeviceReady(uint8_t p_ucModule, uint8_t p_ucAddress)
{
	return HAL_I2C_Write(p_ucModule, p_ucAddress, NULL, 0, HAL_I2C_STOP);
}

/**************************************************************************************************
 * @brief     I2C bus recovery
 * @param[in] p_ucModule - the I2C module (e.g. HAL_I2C_0)
 * @return    HAL_SUCCESS when both lines are high afterwards, HAL_ERROR__BUS when a line is still
 *            held low, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED
 * @remarks   A slave that was interrupted in the middle of a byte keeps the data line low. It is
 *            clocked by hand, up to nine times, until it lets go; then a stop condition is made
 *            and the module is reset and configured again. Called automatically when the bus
 *            stays busy.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_I2C_Recover(uint8_t p_ucModule)
{
	static const HAL_GPIO_FLAGS_T l_stPinFlags = { HAL_GPIO_MODE__OUTPUT, HAL_GPIO_TYPE__OPEN_DRAIN, HAL_GPIO_PULL__PULL_UP, HAL_GPIO_SPEED__MEDIUM, 0 };
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	const HAL_I2C_MODULE_STATE_T *l_pstState;
	uint8_t l_ucIndex;

	if (p_ucModule >= HAL_I2C_NO)
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (!g_astHAL_I2C_State[p_ucModule].m_ucConfigured)
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else
	{
		l_pstState = &g_astHAL_I2C_State[p_ucModule];

		g_astHAL_I2C_Device[p_ucModule].m_pstRegisters->CR1 = 0;

		/* Take the pins as open drain outputs, both released. */
		HAL_GPIO_Write(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, HAL_GPIO_HIGH);
		HAL_GPIO_Write(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin, HAL_GPIO_HIGH);
		(void) HAL_GPIO_Config(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, &l_stPinFlags);
		(void) HAL_GPIO_Config(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin, &l_stPinFlags);
		HAL_CLOCK_DelayUs(10);

		/* Clock the slave until it releases the data line. */
		for (l_ucIndex = 0; (l_ucIndex < 9U) && (HAL_GPIO_Read(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin) == HAL_GPIO_LOW); l_ucIndex++)
		{
			HAL_GPIO_Write(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, HAL_GPIO_LOW);
			HAL_CLOCK_DelayUs(10);
			HAL_GPIO_Write(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, HAL_GPIO_HIGH);
			HAL_CLOCK_DelayUs(10);
		}

		/* Stop condition: the data line rises while the clock line is high. */
		HAL_GPIO_Write(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, HAL_GPIO_LOW);
		HAL_CLOCK_DelayUs(10);
		HAL_GPIO_Write(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin, HAL_GPIO_LOW);
		HAL_CLOCK_DelayUs(10);
		HAL_GPIO_Write(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin, HAL_GPIO_HIGH);
		HAL_CLOCK_DelayUs(10);
		HAL_GPIO_Write(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin, HAL_GPIO_HIGH);
		HAL_CLOCK_DelayUs(10);

		if ((HAL_GPIO_Read(l_pstState->m_stSclPin.m_ucModule, l_pstState->m_stSclPin.m_ucPin) == HAL_GPIO_LOW)
			|| (HAL_GPIO_Read(l_pstState->m_stSdaPin.m_ucModule, l_pstState->m_stSdaPin.m_ucPin) == HAL_GPIO_LOW))
		{
			l_eReturnCode = HAL_ERROR__BUS;
		}

		/* Give the pins back to the module, then reset and configure it again. */
		(void) HAL_GPIO_ConfigPin(&l_pstState->m_stSclPin, &g_stHAL_I2C_PinFlags);
		(void) HAL_GPIO_ConfigPin(&l_pstState->m_stSdaPin, &g_stHAL_I2C_PinFlags);

		g_astHAL_I2C_State[p_ucModule].m_ucHolding = HAL_DISABLE;

		HAL_I2C_Apply(p_ucModule);
	}

	return l_eReturnCode;
}
