/**************************************************************************************************
 * @file    main.c
 * @brief   Example for the NUCLEO-L152RE: clock with fallback, LED on a timer callback, button
 *          on an EXTI callback, echo on the virtual COM port with receive and transmit callbacks
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Application code uses the APP_ prefix. The HAL_ prefix belongs to the HAL.
 **************************************************************************************************/
#include "board.h"

#define APP_ECHO_SIZE 64U

/* Bytes received and not yet sent back. Written by the receive callback, read by the transmit one. */
static volatile uint8_t g_aucAPP_Echo[APP_ECHO_SIZE];
static volatile uint8_t g_ucAPP_EchoHead;
static volatile uint8_t g_ucAPP_EchoTail;
static volatile uint32_t g_ulAPP_ButtonCount;
static volatile uint8_t g_ucAPP_UartErrors;

/**************************************************************************************************
 * @brief     Reports a fatal error: the LED blinks the error code, forever
 * @param[in] p_eCode - the return code that stopped the application
 **************************************************************************************************/
static void APP_Fail(HAL_RETURN_CODE_T p_eCode)
{
	uint8_t l_ucIndex;

	for (;;)
	{
		for (l_ucIndex = 0; l_ucIndex < (uint8_t) p_eCode; l_ucIndex++)
		{
			HAL_GPIO_Write(BOARD_LED_MODULE, BOARD_LED_PIN, HAL_GPIO_HIGH);
			HAL_CLOCK_Delay(150);
			HAL_GPIO_Write(BOARD_LED_MODULE, BOARD_LED_PIN, HAL_GPIO_LOW);
			HAL_CLOCK_Delay(250);
		}

		HAL_CLOCK_Delay(1500);
	}
}

/**************************************************************************************************
 * @brief     Timer update callback: one second has passed
 * @param[in] p_pvContext - not used
 **************************************************************************************************/
static void APP_SecondCallback(void *p_pvContext)
{
	(void) p_pvContext;

	HAL_GPIO_Write(BOARD_LED_MODULE, BOARD_LED_PIN, HAL_GPIO_TOGGLE);
}

/**************************************************************************************************
 * @brief     Button callback: the button was pressed
 * @param[in] p_pvContext - the counter to increment
 **************************************************************************************************/
static void APP_ButtonCallback(void *p_pvContext)
{
	volatile uint32_t *l_pulCount = (volatile uint32_t *) p_pvContext;

	(*l_pulCount)++;
}

/**************************************************************************************************
 * @brief     UART receive callback: stores the byte and starts the transmission
 * @param[in] p_pvContext - not used
 * @param[in] p_ucByte - the received byte
 **************************************************************************************************/
static void APP_RxCallback(void *p_pvContext, uint8_t p_ucByte)
{
	uint8_t l_ucNext = (uint8_t) ((g_ucAPP_EchoHead + 1U) % APP_ECHO_SIZE);

	(void) p_pvContext;

	if (l_ucNext != g_ucAPP_EchoTail)
	{
		g_aucAPP_Echo[g_ucAPP_EchoHead] = p_ucByte;
		g_ucAPP_EchoHead = l_ucNext;
	}

	(void) HAL_UART_SetInterrupt(BOARD_VCP_UART, HAL_UART_TX, HAL_ENABLE);
}

/**************************************************************************************************
 * @brief      UART transmit callback: gives the next byte to send
 * @param[in]  p_pvContext - not used
 * @param[out] p_pucByte - the byte to send
 * @return     HAL_UART_TX__SEND, or HAL_UART_TX__DONE when the buffer is empty
 **************************************************************************************************/
static uint8_t APP_TxCallback(void *p_pvContext, uint8_t *p_pucByte)
{
	uint8_t l_ucAction = HAL_UART_TX__DONE;

	(void) p_pvContext;

	if (g_ucAPP_EchoTail != g_ucAPP_EchoHead)
	{
		*p_pucByte = g_aucAPP_Echo[g_ucAPP_EchoTail];
		g_ucAPP_EchoTail = (uint8_t) ((g_ucAPP_EchoTail + 1U) % APP_ECHO_SIZE);

		l_ucAction = HAL_UART_TX__SEND;
	}

	return l_ucAction;
}

/**************************************************************************************************
 * @brief     UART error callback: remembers what went wrong
 * @param[in] p_pvContext - not used
 * @param[in] p_ucErrors - HAL_UART_ERROR__ bits
 **************************************************************************************************/
static void APP_ErrorCallback(void *p_pvContext, uint8_t p_ucErrors)
{
	(void) p_pvContext;

	g_ucAPP_UartErrors |= p_ucErrors;
}

/**************************************************************************************************
 * @brief   Starts the clock: the external clock first, the internal oscillator when it is missing
 * @return  HAL_SUCCESS when the system runs at 32 MHz
 **************************************************************************************************/
static HAL_RETURN_CODE_T APP_ClockInit(void)
{
	static const HAL_CLOCK_FLAGS_T l_stExternal = BOARD_CLOCK_FLAGS;
	static const HAL_CLOCK_FLAGS_T l_stInternal = BOARD_CLOCK_FLAGS__INTERNAL;
	HAL_RETURN_CODE_T l_eReturnCode = HAL_CLOCK_Init(&l_stExternal);

	if ((l_eReturnCode == HAL_ERROR__TIMEOUT) && (HAL_CLOCK_Get() != 32000000UL))
	{
		/* No clock on the HSE input: the system is on the MSI. Use the HSI instead. */
		l_eReturnCode = HAL_CLOCK_Init(&l_stInternal);
	}
	else if (l_eReturnCode == HAL_ERROR__TIMEOUT)
	{
		/* Only the 32.768 kHz crystal did not start: this example does not need it. */
		l_eReturnCode = HAL_SUCCESS;
	}
	else
	{
		/* Success, or an error to report. */
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Configures the LED and the button
 * @return  HAL_SUCCESS or the first error
 **************************************************************************************************/
static HAL_RETURN_CODE_T APP_GpioInit(void)
{
	static const HAL_GPIO_FLAGS_T l_stLed = { .m_ucMode = HAL_GPIO_MODE__OUTPUT };
	static const HAL_GPIO_FLAGS_T l_stButton = { .m_ucMode = HAL_GPIO_MODE__INPUT };
	static const HAL_GPIO_INTERRUPT_FLAGS_T l_stInterrupt =
	{
		.m_ucEdge = HAL_GPIO_FALLING,
		.m_stInterrupt = { .m_ucPrePriority = 15 },
		.m_pfCallback = APP_ButtonCallback,
		.m_pvContext = (void *) &g_ulAPP_ButtonCount
	};
	HAL_RETURN_CODE_T l_eReturnCode = HAL_GPIO_Config(BOARD_LED_MODULE, BOARD_LED_PIN, &l_stLed);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_Config(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN, &l_stButton);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_ConfigInterrupt(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN, &l_stInterrupt);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_GPIO_SetInterrupt(BOARD_BUTTON_MODULE, BOARD_BUTTON_PIN, HAL_ENABLE);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Configures the virtual COM port: 115200 baud, 8 data bits, no parity, 1 stop bit
 * @return  HAL_SUCCESS or the first error
 **************************************************************************************************/
static HAL_RETURN_CODE_T APP_UartInit(void)
{
	static const HAL_UART_FLAGS_T l_stFlags =
	{
		.m_ulBaud = 115200,
		.m_ucDataBits = HAL_UART_DATABITS__8,
		.m_ucParity = HAL_UART_PARITY__NONE,
		.m_ucStopBits = HAL_UART_STOPBITS__1,
		.m_stTxPin = BOARD_VCP_TX_PIN,
		.m_stRxPin = BOARD_VCP_RX_PIN,
		.m_stInterrupt = { .m_ucPrePriority = 8 },
		.m_pfRxCallback = APP_RxCallback,
		.m_pfTxCallback = APP_TxCallback,
		.m_pfErrorCallback = APP_ErrorCallback
	};
	HAL_RETURN_CODE_T l_eReturnCode = HAL_UART_Config(BOARD_VCP_UART, &l_stFlags);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_UART_SetState(BOARD_VCP_UART, HAL_ENABLE);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_UART_WriteBuff(BOARD_VCP_UART, (const uint8_t *) "new-hal echo\r\n", 14);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_UART_SetInterrupt(BOARD_VCP_UART, HAL_UART_RX, HAL_ENABLE);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Configures a timer that calls back once per second
 * @return  HAL_SUCCESS or the first error
 **************************************************************************************************/
static HAL_RETURN_CODE_T APP_TimerInit(void)
{
	HAL_TIMER_FLAGS_T l_stFlags = { .m_ulUpdate = 9999, .m_stInterrupt = { .m_ucPrePriority = 10 }, .m_pfUpdateCallback = APP_SecondCallback };
	uint32_t l_ulClock = 0;
	HAL_RETURN_CODE_T l_eReturnCode = HAL_TIMER_GetClock(HAL_TIMER_0, &l_ulClock);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Count at 10 kHz, whatever the bus clock is; 10000 counts make one second. */
		l_stFlags.m_unPrescaler = (uint16_t) ((l_ulClock / 10000U) - 1U);

		l_eReturnCode = HAL_TIMER_Config(HAL_TIMER_0, &l_stFlags);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_UPDATE, HAL_ENABLE);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = HAL_TIMER_SetState(HAL_TIMER_0, HAL_ENABLE);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   Application entry point
 * @return  Never
 **************************************************************************************************/
int main(void)
{
	static const HAL_LPM_FLAGS_T l_stSleep = { .m_ucKeepTick = HAL_ENABLE };
	HAL_RETURN_CODE_T l_eReturnCode = APP_ClockInit();

	/* The LED is configured even after a clock error, so the error can be shown. */
	if (APP_GpioInit() != HAL_SUCCESS)
	{
		for (;;)
		{
			/* Nothing to show the error with. */
		}
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = APP_UartInit();
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_eReturnCode = APP_TimerInit();
	}

	if (l_eReturnCode != HAL_SUCCESS)
	{
		APP_Fail(l_eReturnCode);
	}

	for (;;)
	{
		/* Everything happens in the callbacks: sleep until the next interrupt. */
		(void) HAL_LPM_Enter(HAL_LPM_MODE__SLEEP, &l_stSleep);
	}
}
