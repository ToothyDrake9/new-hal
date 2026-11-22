/**************************************************************************************************
 * @file    link_all.c
 * @brief   Build check: calls every public function of the HAL once
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Not an application and not meant to be flashed. It makes the compiler check every
 *          prototype against its use and makes the linker resolve every module, the interrupt
 *          vectors and the RAM function included.
 **************************************************************************************************/
#include "board.h"

static volatile uint32_t g_ulTEST_Events;

static void TEST_Callback(void *p_pvContext)
{
	(void) p_pvContext;

	g_ulTEST_Events++;
}

static void TEST_ByteCallback(void *p_pvContext, uint8_t p_ucByte)
{
	(void) p_pvContext;

	g_ulTEST_Events += p_ucByte;
}

static uint8_t TEST_TxCallback(void *p_pvContext, uint8_t *p_pucByte)
{
	(void) p_pvContext;

	*p_pucByte = 0x55;

	return HAL_UART_TX__DONE;
}

static void TEST_AdcCallback(void *p_pvContext, uint16_t p_unValue)
{
	(void) p_pvContext;

	g_ulTEST_Events += p_unValue;
}

int main(void)
{
	static const HAL_CLOCK_FLAGS_T l_stClock = BOARD_CLOCK_FLAGS;
	static const HAL_GPIO_FLAGS_T l_stOutput = { .m_ucMode = HAL_GPIO_MODE__OUTPUT };
	static const HAL_GPIO_INTERRUPT_FLAGS_T l_stEdge = { .m_ucEdge = HAL_GPIO_BOTH, .m_pfCallback = TEST_Callback };
	static const HAL_UART_FLAGS_T l_stUart = { .m_ulBaud = 921600, .m_stTxPin = BOARD_VCP_TX_PIN, .m_stRxPin = BOARD_VCP_RX_PIN, .m_pfRxCallback = TEST_ByteCallback, .m_pfTxCallback = TEST_TxCallback, .m_pfTxCompleteCallback = TEST_Callback, .m_pfErrorCallback = TEST_ByteCallback };
	static const HAL_SPI_FLAGS_T l_stSpi = { .m_ulSpeed = 8000000, .m_stClkPin = BOARD_SPI_CLK_PIN, .m_stMisoPin = BOARD_SPI_MISO_PIN, .m_stMosiPin = BOARD_SPI_MOSI_PIN, .m_stSsPin = BOARD_NO_PIN, .m_pfRxCallback = TEST_ByteCallback, .m_pfTxCallback = TEST_TxCallback, .m_pfErrorCallback = TEST_ByteCallback };
	static const HAL_I2C_FLAGS_T l_stI2c = { .m_ulSpeed = 400000, .m_stSclPin = BOARD_I2C_SCL_PIN, .m_stSdaPin = BOARD_I2C_SDA_PIN };
	static const HAL_TIMER_FLAGS_T l_stTimer = { .m_unPrescaler = 31, .m_ulUpdate = 999, .m_pfUpdateCallback = TEST_Callback, .m_apfCompareCallback = { TEST_Callback } };
	static const HAL_TIMER_CHANNEL_FLAGS_T l_stChannel = { .m_ucOutput = HAL_TIMER_OUTPUT__PWM, .m_ulCompare = 0, .m_stPin = { HAL_GPIO_0, HAL_GPIO_PIN_0, HAL_GPIO_FUNCTION__1 } };
	static const HAL_ADC_FLAGS_T l_stAdc = { .m_pfConversionCallback = TEST_AdcCallback };
	static const HAL_ADC_CHANNEL_FLAGS_T l_stAdcChannel = { .m_ucSampleTime = HAL_ADC_SAMPLE_TIME__384, .m_stPin = BOARD_NO_PIN };
	static const HAL_DAC_FLAGS_T l_stDac = { .m_ucBuffer = HAL_ENABLE, .m_stPin = { HAL_GPIO_0, HAL_GPIO_PIN_4, 0 } };
	static const HAL_DMA_FLAGS_T l_stDma = { .m_ucMemoryIncrement = HAL_ENABLE, .m_pfCompleteCallback = TEST_Callback, .m_pfHalfCallback = TEST_Callback, .m_pfErrorCallback = TEST_Callback };
	static const HAL_LPM_FLAGS_T l_stLpm = { .m_pfEnterCallback = TEST_Callback, .m_pfExitCallback = TEST_Callback };
	static uint32_t l_aulHalfPage[HAL_FLASH_HALF_PAGE_SIZE / 4U];
	static uint8_t l_aucBuff[16];
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	HAL_CLOCK_FLAGS_T l_stRunning;
	HAL_GPIO_PIN_T l_stPin = BOARD_NO_PIN;
	uint32_t l_ulValue = 0;
	uint16_t l_unValue = 0;
	uint8_t l_ucValue = 0;
	uint32_t l_ulErrors = 0;

	/* Clock. */
	l_ulErrors += HAL_CLOCK_Init(&l_stClock);
	l_ulErrors += HAL_CLOCK_Update(&l_stClock);
	HAL_CLOCK_GetFlags(&l_stRunning);
	l_ulValue += HAL_CLOCK_Get() + HAL_CLOCK_GetBus(HAL_CLOCK_BUS__APB1) + HAL_CLOCK_GetTimer(HAL_CLOCK_BUS__APB2) + HAL_CLOCK_GetTick();
	l_ulErrors += HAL_CLOCK_RequestHsi();
	HAL_CLOCK_ReleaseHsi();
	HAL_CLOCK_SetTickState(HAL_ENABLE);
	HAL_CLOCK_Delay(1);
	HAL_CLOCK_DelayUs(1);
	HAL_CLOCK_TimeoutStart(&l_stTimeout, 1);
	l_ulValue += HAL_CLOCK_TimeoutExpired(&l_stTimeout);

	/* GPIO. */
	l_ulErrors += HAL_GPIO_Config(BOARD_LED_MODULE, BOARD_LED_PIN, &l_stOutput);
	l_ulErrors += HAL_GPIO_ConfigPin(&l_stPin, &l_stOutput);
	l_ulErrors += HAL_GPIO_ConfigInterrupt(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN, &l_stEdge);
	l_ulErrors += HAL_GPIO_SetInterrupt(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN, HAL_ENABLE);
	l_ulErrors += HAL_GPIO_ReleaseInterrupt(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN);
	HAL_GPIO_Write(BOARD_LED_MODULE, BOARD_LED_PIN, HAL_GPIO_TOGGLE);
	l_ulValue += HAL_GPIO_Read(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN);

	/* UART. */
	l_ulErrors += HAL_UART_Config(BOARD_VCP_UART, &l_stUart);
	l_ulErrors += HAL_UART_SetState(BOARD_VCP_UART, HAL_ENABLE);
	l_ulErrors += HAL_UART_SetInterrupt(BOARD_VCP_UART, HAL_UART_RX, HAL_ENABLE);
	l_ulErrors += HAL_UART_GetBaud(BOARD_VCP_UART, &l_ulValue);
	l_ulErrors += HAL_UART_ReadByte(BOARD_VCP_UART, &l_ucValue, 0);
	l_ulErrors += HAL_UART_WriteByte(BOARD_VCP_UART, l_ucValue);
	l_ulErrors += HAL_UART_WriteBuff(BOARD_VCP_UART, l_aucBuff, sizeof(l_aucBuff));
	l_ulErrors += HAL_UART_Flush(BOARD_VCP_UART);

	/* SPI. */
	l_ulErrors += HAL_SPI_Config(BOARD_SPI, &l_stSpi);
	l_ulErrors += HAL_SPI_SetState(BOARD_SPI, HAL_ENABLE);
	l_ulErrors += HAL_SPI_SetInterrupt(BOARD_SPI, HAL_SPI_RX, HAL_DISABLE);
	l_ulErrors += HAL_SPI_GetSpeed(BOARD_SPI, &l_ulValue);
	l_ulErrors += HAL_SPI_TransferByte(BOARD_SPI, 0x00, &l_ucValue);
	l_ulErrors += HAL_SPI_Transfer(BOARD_SPI, l_aucBuff, l_aucBuff, sizeof(l_aucBuff));
	l_ulErrors += HAL_SPI_WriteBuff(BOARD_SPI, l_aucBuff, sizeof(l_aucBuff));
	l_ulErrors += HAL_SPI_ReadBuff(BOARD_SPI, l_aucBuff, sizeof(l_aucBuff));

	/* I2C. */
	l_ulErrors += HAL_I2C_Config(BOARD_I2C, &l_stI2c);
	l_ulErrors += HAL_I2C_SetState(BOARD_I2C, HAL_ENABLE);
	l_ulErrors += HAL_I2C_IsDeviceReady(BOARD_I2C, 0x50);
	l_ulErrors += HAL_I2C_Write(BOARD_I2C, 0x50, l_aucBuff, 2, HAL_I2C_STOP);
	l_ulErrors += HAL_I2C_Read(BOARD_I2C, 0x50, l_aucBuff, 3);
	l_ulErrors += HAL_I2C_WriteRead(BOARD_I2C, 0x50, l_aucBuff, 1, &l_aucBuff[1], 8);
	l_ulErrors += HAL_I2C_Recover(BOARD_I2C);

	/* Timer. */
	l_ulErrors += HAL_TIMER_Config(HAL_TIMER_0, &l_stTimer);
	l_ulErrors += HAL_TIMER_ConfigChannel(HAL_TIMER_0, HAL_TIMER_COMPARE1, &l_stChannel);
	l_ulErrors += HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_UPDATE, HAL_ENABLE);
	l_ulErrors += HAL_TIMER_SetState(HAL_TIMER_0, HAL_ENABLE);
	l_ulErrors += HAL_TIMER_SetCounter(HAL_TIMER_0, 0);
	l_ulErrors += HAL_TIMER_SetCompare(HAL_TIMER_0, HAL_TIMER_COMPARE1, 0);
	l_ulErrors += HAL_TIMER_GetCompare(HAL_TIMER_0, HAL_TIMER_UPDATE, &l_ulValue);
	l_ulErrors += HAL_TIMER_GetClock(HAL_TIMER_0, &l_ulValue);
	l_ulValue += HAL_TIMER_GetCounter(HAL_TIMER_0);

	/* ADC and DAC. */
	l_ulErrors += HAL_ADC_Config(HAL_ADC_0, &l_stAdc);
	l_ulErrors += HAL_ADC_ConfigChannel(HAL_ADC_0, HAL_ADC_CHANNEL__VREFINT, &l_stAdcChannel);
	l_ulErrors += HAL_ADC_SetState(HAL_ADC_0, HAL_ENABLE);
	l_ulErrors += HAL_ADC_ReadChannelRaw(HAL_ADC_0, HAL_ADC_CHANNEL__VREFINT, &l_unValue);
	l_ulErrors += HAL_ADC_StartConversion(HAL_ADC_0, HAL_ADC_CHANNEL__VREFINT);
	l_ulErrors += HAL_DAC_Config(HAL_DAC_0, &l_stDac);
	l_ulErrors += HAL_DAC_SetState(HAL_DAC_0, HAL_ENABLE);
	l_ulErrors += HAL_DAC_Write(HAL_DAC_0, HAL_DAC_MAXIMUM / 2U);

	/* DMA. */
	l_ulErrors += HAL_DMA_Config(HAL_DMA_0, HAL_DMA_CHANNEL_0, &l_stDma);
	l_ulErrors += HAL_DMA_SetInterrupt(HAL_DMA_0, HAL_DMA_CHANNEL_0, HAL_DMA_COMPLETE, HAL_ENABLE);
	l_ulErrors += HAL_DMA_Start(HAL_DMA_0, HAL_DMA_CHANNEL_0, (uint32_t) (uintptr_t) &ADC1->DR, (uint32_t) (uintptr_t) l_aucBuff, sizeof(l_aucBuff));
	l_ulErrors += HAL_DMA_GetRemaining(HAL_DMA_0, HAL_DMA_CHANNEL_0, &l_unValue);
	l_ulErrors += HAL_DMA_Stop(HAL_DMA_0, HAL_DMA_CHANNEL_0);

	/* Flash. */
	l_ulErrors += HAL_FLASH_Unlock();
	l_ulErrors += HAL_FLASH_PageErase((uint16_t) (HAL_FLASH_GetPageCount() - 1U));
	l_ulErrors += HAL_FLASH_Program(HAL_FLASH_GetPageAddress((uint16_t) (HAL_FLASH_GetPageCount() - 1U)), 0x12345678UL);
	l_ulErrors += HAL_FLASH_ProgramBuff(HAL_FLASH_GetPageAddress((uint16_t) (HAL_FLASH_GetPageCount() - 1U)) + 4U, l_aulHalfPage, 4);
	l_ulErrors += HAL_FLASH_FastProgram(HAL_FLASH_GetPageAddress((uint16_t) (HAL_FLASH_GetPageCount() - 1U)) + HAL_FLASH_HALF_PAGE_SIZE, l_aulHalfPage);
	l_ulErrors += HAL_FLASH_MassErase();
	l_ulErrors += HAL_FLASH_Lock();

	/* CRC, RNG, AES. */
	l_ulErrors += HAL_CRC_Config(NULL);
	l_ulErrors += HAL_CRC_Calculate(l_aucBuff, sizeof(l_aucBuff), &l_ulValue);
	l_ulErrors += HAL_RNG_Config();
	l_ulErrors += HAL_RNG_Read(&l_ulValue);
	l_ulErrors += HAL_AES_Config(HAL_AES_MODE__ECB);
	l_ulErrors += HAL_AES_SetKey(l_aucBuff);
	l_ulErrors += HAL_AES_Encrypt(l_aucBuff, l_aucBuff);
	l_ulErrors += HAL_AES_Decrypt(l_aucBuff, l_aucBuff);

	/* Low power. */
	l_ulValue += HAL_LPM_WasInStandby();
	l_ulErrors += HAL_LPM_Enter(HAL_LPM_MODE__SLEEP, NULL);
	l_ulErrors += HAL_LPM_Enter(HAL_LPM_MODE__LOW_POWER_RUN, &l_stLpm);
	l_ulErrors += HAL_LPM_Exit(HAL_LPM_MODE__LOW_POWER_RUN);
	l_ulErrors += HAL_LPM_Enter(HAL_LPM_MODE__LOW_POWER_SLEEP, &l_stLpm);
	l_ulErrors += HAL_LPM_Enter(HAL_LPM_MODE__STOP, &l_stLpm);
	l_ulErrors += HAL_LPM_Enter(HAL_LPM_MODE__STANDBY, &l_stLpm);

	return (int) (l_ulErrors + l_ulValue + l_unValue + g_ulTEST_Events);
}
