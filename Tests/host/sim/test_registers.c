/**************************************************************************************************
 * @file    test_registers.c
 * @brief   PC test of the HAL drivers against simulated STM32L152RE registers
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Linux only. The peripheral address ranges are mapped as plain memory, a small model
 *          plays the few hardware reactions the drivers wait for (oscillator ready bits, clock
 *          switch status, the SysTick counter, the ADC), and the real driver sources run on top.
 *          The model advances whenever the HAL polls a timeout (see sim_hook.h).
 *          What this proves: the values the drivers compute and the bits they write. What it
 *          cannot prove: timing, electrical behaviour, and anything the hardware does by itself.
 *          A register that is cleared by writing 1 is plain memory here: after the driver wrote
 *          to it, it holds exactly what was written, which is what the checks look at.
 **************************************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "board.h"
#include "hal_private.h"

/* The interrupt vectors of the drivers are ordinary functions here: the tests call them. */
void SysTick_Handler(void);
void USART2_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void TIM2_IRQHandler(void);
void SPI1_IRQHandler(void);
void ADC1_IRQHandler(void);
void DMA1_Channel1_IRQHandler(void);

volatile uint32_t g_ulSIM_Primask;
volatile uint32_t g_ulSIM_WaitForInterrupt;

static int g_nSIM_NoHse;
static int g_nSIM_NoLse;
static int g_nFailures;
static int g_nChecks;

#define TEST_CHECK(p_condition) \
	do \
	{ \
		g_nChecks++; \
		if (!(p_condition)) \
		{ \
			printf("FAILED line %d: %s\n", __LINE__, #p_condition); \
			g_nFailures++; \
		} \
		if (g_ulSIM_Primask != 0U) \
		{ \
			printf("FAILED line %d: interrupts left masked\n", __LINE__); \
			g_nFailures++; \
			g_ulSIM_Primask = 0; \
		} \
	} while (0)

/**************************************************************************************************
 * @brief     Makes a status bit of a register follow a condition
 * @param[in] p_pulRegister - the register
 * @param[in] p_bSet - true to set the status bit, false to clear it
 * @param[in] p_ulStatus - the status bit
 **************************************************************************************************/
static void SIM_Follow(volatile uint32_t *p_pulRegister, bool p_bSet, uint32_t p_ulStatus)
{
	*p_pulRegister = p_bSet ? (*p_pulRegister | p_ulStatus) : (*p_pulRegister & ~p_ulStatus);
}

/**************************************************************************************************
 * @brief   One step of the simulated hardware
 **************************************************************************************************/
static void SIM_Step(void)
{
	uint32_t l_ulStep;
	uint32_t l_ulValue;

	/* Oscillators become ready when they are switched on. */
	SIM_Follow(&RCC->CR, (RCC->CR & RCC_CR_MSION) != 0U, RCC_CR_MSIRDY);
	SIM_Follow(&RCC->CR, (RCC->CR & RCC_CR_HSION) != 0U, RCC_CR_HSIRDY);
	SIM_Follow(&RCC->CR, ((RCC->CR & RCC_CR_HSEON) != 0U) && (!g_nSIM_NoHse), RCC_CR_HSERDY);
	SIM_Follow(&RCC->CR, (RCC->CR & RCC_CR_PLLON) != 0U, RCC_CR_PLLRDY);
	SIM_Follow(&RCC->CSR, ((RCC->CSR & RCC_CSR_LSEON) != 0U) && (!g_nSIM_NoLse), RCC_CSR_LSERDY);

	/* The clock switch status follows the clock switch. */
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SWS) | ((RCC->CFGR & RCC_CFGR_SW) << 2);

	/* The regulator and the internal reference are always ready. */
	SIM_Follow(&PWR->CSR, true, PWR_CSR_VREFINTRDYF);
	SIM_Follow(&PWR->CSR, false, PWR_CSR_VOSF | PWR_CSR_REGLPF);

	/* The ADC is on when asked, and a started conversion completes. */
	SIM_Follow(&ADC1->SR, (ADC1->CR2 & ADC_CR2_ADON) != 0U, ADC_SR_ADONS);

	if (ADC1->CR2 & ADC_CR2_SWSTART)
	{
		ADC1->DR = 0x0ABCU;
		ADC1->SR |= ADC_SR_EOCS;
		ADC1->CR2 &= ~ADC_CR2_SWSTART;
	}

	/* The SysTick counter counts down and reloads. */
	if (SIM_SYSTICK->CTRL & SysTick_CTRL_ENABLE_Msk)
	{
		l_ulStep = (SIM_SYSTICK->LOAD / 16U) + 1U;
		l_ulValue = SIM_SYSTICK->VAL;

		SIM_SYSTICK->VAL = (l_ulValue >= l_ulStep) ? (l_ulValue - l_ulStep) : (SIM_SYSTICK->LOAD - (l_ulStep - l_ulValue - 1U));
	}
}

/**************************************************************************************************
 * @brief   Access to the SysTick registers, see sim_hook.h
 * @return  The SysTick registers, after one step of the simulated hardware
 **************************************************************************************************/
SysTick_Type *SIM_SysTick(void)
{
	SIM_Step();

	return SIM_SYSTICK;
}

/**************************************************************************************************
 * @brief   Puts the simulated registers in their state after reset
 **************************************************************************************************/
static void SIM_Reset(void)
{
	memset((void *) 0x40000000UL, 0, 0x30000);
	memset((void *) 0xE000E000UL, 0, 0x1000);

	RCC->CR = RCC_CR_MSION | RCC_CR_MSIRDY;
	RCC->ICSCR = 5UL << RCC_ICSCR_MSIRANGE_Pos;
	PWR->CR = PWR_CR_VOS_1;
	FLASH->PECR = FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK | FLASH_PECR_OPTLOCK;

	g_nSIM_NoHse = 0;
	g_nSIM_NoLse = 0;
}

/* Callbacks that record what they were called with. */
static uint32_t g_ulTEST_Calls;
static uint32_t g_ulTEST_Value;
static uint8_t g_ucTEST_Errors;
static void *g_pvTEST_Context;

static void TEST_Callback(void *p_pvContext)
{
	g_pvTEST_Context = p_pvContext;
	g_ulTEST_Calls++;
}

static void TEST_SecondCallback(void *p_pvContext)
{
	(void) p_pvContext;

	g_ulTEST_Value++;
}

static void TEST_RxCallback(void *p_pvContext, uint8_t p_ucByte)
{
	g_pvTEST_Context = p_pvContext;
	g_ulTEST_Value = p_ucByte;
	g_ulTEST_Calls++;
}

static void TEST_ErrorCallback(void *p_pvContext, uint8_t p_ucErrors)
{
	(void) p_pvContext;

	g_ucTEST_Errors = p_ucErrors;
}

static uint8_t g_ucTEST_TxLeft;

static uint8_t TEST_TxCallback(void *p_pvContext, uint8_t *p_pucByte)
{
	uint8_t l_ucAction = HAL_UART_TX__DONE;

	(void) p_pvContext;

	if (g_ucTEST_TxLeft != 0U)
	{
		g_ucTEST_TxLeft--;
		*p_pucByte = 0x5A;
		l_ucAction = HAL_UART_TX__SEND;
	}

	return l_ucAction;
}

static void TEST_AdcCallback(void *p_pvContext, uint16_t p_unValue)
{
	(void) p_pvContext;

	g_ulTEST_Value = p_unValue;
	g_ulTEST_Calls++;
}

static void TEST_Clear(void)
{
	g_ulTEST_Calls = 0;
	g_ulTEST_Value = 0;
	g_ucTEST_Errors = 0;
	g_pvTEST_Context = NULL;
}

/**************************************************************************************************
 * @brief   Clock: recommendations 1, 14, 22, 23, 28 and 39
 **************************************************************************************************/
static void TEST_Clock(void)
{
	static const HAL_CLOCK_FLAGS_T l_stExternal = BOARD_CLOCK_FLAGS;
	static const HAL_CLOCK_FLAGS_T l_stInternal = BOARD_CLOCK_FLAGS__INTERNAL;
	HAL_CLOCK_FLAGS_T l_stFlags = BOARD_CLOCK_FLAGS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;
	uint32_t l_ulTick;

	SIM_Reset();

	/* Before the time base runs, a wait fails at once instead of hanging. */
	HAL_CLOCK_TimeoutStart(&l_stTimeout, 1000);
	TEST_CHECK(HAL_CLOCK_TimeoutExpired(&l_stTimeout));

	TEST_CHECK(HAL_CLOCK_Init(NULL) == HAL_ERROR__PARAMETER);

	/* 32 MHz from the 8 MHz external clock. */
	TEST_CHECK(HAL_CLOCK_Init(&l_stExternal) == HAL_SUCCESS);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS_0);                         /* Voltage range 1 */
	TEST_CHECK((FLASH->ACR & (FLASH_ACR_ACC64 | FLASH_ACR_LATENCY)) == (FLASH_ACR_ACC64 | FLASH_ACR_LATENCY));
	TEST_CHECK((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLSRC) != 0U);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLMUL) == RCC_CFGR_PLLMUL8);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLDIV) == RCC_CFGR_PLLDIV2);
	TEST_CHECK((RCC->CR & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_PLLON)) == (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_PLLON));
	TEST_CHECK((RCC->CR & (RCC_CR_MSION | RCC_CR_HSION)) == 0U);
	TEST_CHECK((RCC->CSR & RCC_CSR_LSEON) != 0U);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);
	TEST_CHECK(HAL_CLOCK_GetBus(HAL_CLOCK_BUS__APB1) == 32000000UL);
	TEST_CHECK(SystemCoreClock == 32000000UL);
	TEST_CHECK(SIM_SYSTICK->LOAD == 31999UL);
	TEST_CHECK((SIM_SYSTICK->CTRL & SysTick_CTRL_TICKINT_Msk) != 0U);

	/* The tick counts interrupts; delays and timeouts work without it. */
	l_ulTick = HAL_CLOCK_GetTick();
	SysTick_Handler();
	TEST_CHECK(HAL_CLOCK_GetTick() == (l_ulTick + 1U));
	HAL_CLOCK_Delay(3);
	HAL_CLOCK_DelayUs(1500);
	TEST_CHECK(HAL_CLOCK_GetTick() == (l_ulTick + 1U));

	/* Bus prescalers are reported, and the timer clock is doubled when the bus is divided. */
	l_stFlags.m_ucApb1Prescaler = HAL_CLOCK_APB__4;
	l_stFlags.m_ucAhbPrescaler = HAL_CLOCK_AHB__2;
	TEST_CHECK(HAL_CLOCK_Update(&l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);
	TEST_CHECK(HAL_CLOCK_GetBus(HAL_CLOCK_BUS__AHB) == 16000000UL);
	TEST_CHECK(HAL_CLOCK_GetBus(HAL_CLOCK_BUS__APB1) == 4000000UL);
	TEST_CHECK(HAL_CLOCK_GetBus(HAL_CLOCK_BUS__APB2) == 16000000UL);
	TEST_CHECK(HAL_CLOCK_GetTimer(HAL_CLOCK_BUS__APB1) == 8000000UL);
	TEST_CHECK(HAL_CLOCK_GetTimer(HAL_CLOCK_BUS__APB2) == 16000000UL);
	TEST_CHECK(SIM_SYSTICK->LOAD == 15999UL);

	/* A configuration outside the limits of its voltage range is refused and nothing changes. */
	l_stFlags = (HAL_CLOCK_FLAGS_T) BOARD_CLOCK_FLAGS;
	l_stFlags.m_ucVoltage = HAL_CLOCK_VOLTAGE__RANGE_2;
	TEST_CHECK(HAL_CLOCK_Update(&l_stFlags) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS_0);

	/* 16 MHz in range 2 is accepted: the range field becomes 10, never 11 by accident. */
	l_stFlags.m_ucPllMultiplier = HAL_CLOCK_PLL_MULTIPLIER__4;
	TEST_CHECK(HAL_CLOCK_Update(&l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS_1);
	TEST_CHECK(HAL_CLOCK_Get() == 16000000UL);
	TEST_CHECK((FLASH->ACR & FLASH_ACR_LATENCY) != 0U);                         /* Above 8 MHz in range 2 */

	/* MSI in range 3: no wait state, 64 bit access off. */
	l_stFlags = (HAL_CLOCK_FLAGS_T) { .m_ucSource = HAL_CLOCK_SOURCE__MSI, .m_ucMsiRange = HAL_CLOCK_MSI__1048_KHZ, .m_ucVoltage = HAL_CLOCK_VOLTAGE__RANGE_3 };
	TEST_CHECK(HAL_CLOCK_Update(&l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS);
	TEST_CHECK(FLASH->ACR == 0U);
	TEST_CHECK(HAL_CLOCK_Get() == 1048576UL);
	TEST_CHECK((RCC->CR & (RCC_CR_HSEON | RCC_CR_PLLON | RCC_CR_HSION)) == 0U);

	/* No external clock: the call times out and the system is left running on the MSI. */
	g_nSIM_NoHse = 1;
	TEST_CHECK(HAL_CLOCK_Update(&l_stExternal) == HAL_ERROR__TIMEOUT);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_MSI);
	TEST_CHECK(HAL_CLOCK_Get() == 2097152UL);
	TEST_CHECK(SIM_SYSTICK->LOAD == 2096UL);
	TEST_CHECK((RCC->CR & RCC_CR_HSEON) == 0U);

	/* The fallback: 32 MHz from the internal oscillator. */
	TEST_CHECK(HAL_CLOCK_Update(&l_stInternal) == HAL_SUCCESS);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLSRC) == 0U);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLMUL) == RCC_CFGR_PLLMUL6);
	TEST_CHECK((RCC->CFGR & RCC_CFGR_PLLDIV) == RCC_CFGR_PLLDIV3);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);
	g_nSIM_NoHse = 0;

	/* No 32.768 kHz crystal: reported, with the system clock as requested. */
	g_nSIM_NoLse = 1;
	RCC->CSR &= ~(RCC_CSR_LSEON | RCC_CSR_LSERDY);
	TEST_CHECK(HAL_CLOCK_Update(&l_stExternal) == HAL_ERROR__TIMEOUT);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);
	TEST_CHECK((RCC->CSR & RCC_CSR_LSEON) == 0U);
	g_nSIM_NoLse = 0;

	/* A requested HSI survives a clock change that does not use it (recommendation 12). */
	TEST_CHECK(HAL_CLOCK_RequestHsi() == HAL_SUCCESS);
	TEST_CHECK(HAL_CLOCK_Update(&l_stExternal) == HAL_SUCCESS);
	TEST_CHECK((RCC->CR & RCC_CR_HSION) != 0U);
	HAL_CLOCK_ReleaseHsi();
	TEST_CHECK((RCC->CR & RCC_CR_HSION) == 0U);
}

/**************************************************************************************************
 * @brief   GPIO: recommendations 3, 5, 6, 9, 19, 36 (through the timer) and 37
 **************************************************************************************************/
static void TEST_Gpio(void)
{
	HAL_GPIO_FLAGS_T l_stFlags = { .m_ucMode = HAL_GPIO_MODE__FUNCTION, .m_ucType = HAL_GPIO_TYPE__OPEN_DRAIN, .m_ucPull = HAL_GPIO_PULL__PULL_UP, .m_ucSpeed = HAL_GPIO_SPEED__VERY_HIGH, .m_ucFunction = HAL_GPIO_FUNCTION__7 };
	HAL_GPIO_INTERRUPT_FLAGS_T l_stEdge = { .m_ucEdge = HAL_GPIO_FALLING, .m_stInterrupt = { .m_ucPrePriority = 5 }, .m_pfCallback = TEST_Callback, .m_pvContext = &g_ulTEST_Value };
	uint32_t l_ulPriority;

	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_NO, HAL_GPIO_PIN_0, &l_stFlags) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_0, HAL_GPIO_PIN_NO, &l_stFlags) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_0, HAL_GPIO_PIN_0, NULL) == HAL_ERROR__PARAMETER);

	/* A pin of port B in the high half of the function register. */
	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_1, HAL_GPIO_PIN_9, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((RCC->AHBENR & RCC_AHBENR_GPIOBEN) != 0U);
	TEST_CHECK(GPIOB->AFR[1] == (7UL << 4));
	TEST_CHECK(GPIOB->MODER == (2UL << 18));
	TEST_CHECK(GPIOB->OTYPER == (1UL << 9));
	TEST_CHECK(GPIOB->OSPEEDR == (3UL << 18));
	TEST_CHECK(GPIOB->PUPDR == (1UL << 18));

	/* Configured again: every field is replaced, none is ORed with the old value. */
	l_stFlags.m_ucFunction = HAL_GPIO_FUNCTION__4;
	l_stFlags.m_ucType = HAL_GPIO_TYPE__PUSH_PULL;
	l_stFlags.m_ucPull = HAL_GPIO_PULL__PULL_DOWN;
	l_stFlags.m_ucSpeed = HAL_GPIO_SPEED__LOW;
	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_1, HAL_GPIO_PIN_9, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(GPIOB->AFR[1] == (4UL << 4));
	TEST_CHECK(GPIOB->OTYPER == 0U);
	TEST_CHECK(GPIOB->OSPEEDR == 0U);
	TEST_CHECK(GPIOB->PUPDR == (2UL << 18));

	/* Port H is the sixth port in memory, and the EXTI code 5. */
	l_stFlags.m_ucMode = HAL_GPIO_MODE__INPUT;
	TEST_CHECK(HAL_GPIO_Config(HAL_GPIO_7, HAL_GPIO_PIN_1, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((RCC->AHBENR & RCC_AHBENR_GPIOHEN) != 0U);
	TEST_CHECK((void *) HAL_GPIO_GetRegisters(HAL_GPIO_7) == (void *) GPIOH);
	TEST_CHECK((void *) HAL_GPIO_GetRegisters(HAL_GPIO_5) == (void *) GPIOF);

	/* Configuring plain pins does not touch the NVIC. */
	TEST_CHECK(NVIC->ISER[0] == 0U);
	TEST_CHECK(NVIC->ISER[1] == 0U);

	/* Pin access writes only the set/reset register. */
	GPIOA->ODR = 0;
	HAL_GPIO_Write(HAL_GPIO_0, HAL_GPIO_PIN_5, HAL_GPIO_HIGH);
	TEST_CHECK(GPIOA->BSRR == (1UL << 5));
	HAL_GPIO_Write(HAL_GPIO_0, HAL_GPIO_PIN_5, HAL_GPIO_LOW);
	TEST_CHECK(GPIOA->BSRR == (1UL << 21));
	GPIOA->ODR = 1UL << 5;
	HAL_GPIO_Write(HAL_GPIO_0, HAL_GPIO_PIN_5, HAL_GPIO_TOGGLE);
	TEST_CHECK(GPIOA->BSRR == (1UL << 21));
	TEST_CHECK(GPIOA->ODR == (1UL << 5));                                       /* Not written by the HAL */
	GPIOA->IDR = 1UL << 13;
	TEST_CHECK(HAL_GPIO_Read(HAL_GPIO_0, HAL_GPIO_PIN_13) == HAL_GPIO_HIGH);
	TEST_CHECK(HAL_GPIO_Read(HAL_GPIO_0, HAL_GPIO_PIN_12) == HAL_GPIO_LOW);

	/* Interrupt on PC13, falling edge. */
	TEST_CHECK(HAL_GPIO_SetInterrupt(HAL_GPIO_2, HAL_GPIO_PIN_13, HAL_ENABLE) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_GPIO_ConfigInterrupt(HAL_GPIO_2, HAL_GPIO_PIN_13, &l_stEdge) == HAL_SUCCESS);
	TEST_CHECK((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) != 0U);
	TEST_CHECK(SYSCFG->EXTICR[3] == (2UL << 4));
	TEST_CHECK(EXTI->FTSR == (1UL << 13));
	TEST_CHECK(EXTI->RTSR == 0U);
	TEST_CHECK(EXTI->IMR == 0U);                                                /* Masked until enabled */
	TEST_CHECK((NVIC->ISER[EXTI15_10_IRQn >> 5] & (1UL << (EXTI15_10_IRQn & 31))) != 0U);
	l_ulPriority = NVIC_GetPriority(EXTI15_10_IRQn);
	TEST_CHECK(l_ulPriority == 5U);
	TEST_CHECK(HAL_GPIO_SetInterrupt(HAL_GPIO_2, HAL_GPIO_PIN_13, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(EXTI->IMR == (1UL << 13));

	/* The same pin number on another port cannot take the line. */
	TEST_CHECK(HAL_GPIO_ConfigInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_13, &l_stEdge) == HAL_ERROR__CONFLICT);

	/* A pin that shares the interrupt line cannot silently change its priority. */
	l_stEdge.m_stInterrupt.m_ucPrePriority = 1;
	TEST_CHECK(HAL_GPIO_ConfigInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_10, &l_stEdge) == HAL_ERROR__CONFLICT);
	TEST_CHECK(NVIC_GetPriority(EXTI15_10_IRQn) == 5U);
	l_stEdge.m_stInterrupt.m_ucPrePriority = 5;
	l_stEdge.m_ucEdge = HAL_GPIO_BOTH;
	TEST_CHECK(HAL_GPIO_ConfigInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_10, &l_stEdge) == HAL_SUCCESS);
	TEST_CHECK(EXTI->RTSR == (1UL << 10));
	TEST_CHECK(EXTI->FTSR == ((1UL << 13) | (1UL << 10)));
	TEST_CHECK(HAL_GPIO_SetInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_10, HAL_ENABLE) == HAL_SUCCESS);

	/* Handling: only the pending and enabled line is served, and only its flag is written. */
	TEST_Clear();
	EXTI->PR = (1UL << 13) | (1UL << 6) | (1UL << 11);
	EXTI15_10_IRQHandler();
	TEST_CHECK(g_ulTEST_Calls == 1U);
	TEST_CHECK(g_pvTEST_Context == &g_ulTEST_Value);
	TEST_CHECK(EXTI->PR == (1UL << 13));                                        /* Last value written */

	/* Disabling one pin leaves the other pin and the NVIC line working. */
	TEST_CHECK(HAL_GPIO_SetInterrupt(HAL_GPIO_2, HAL_GPIO_PIN_13, HAL_DISABLE) == HAL_SUCCESS);
	TEST_CHECK(EXTI->IMR == (1UL << 10));
	TEST_CHECK(NVIC->ICER[1] == 0U);
	TEST_Clear();
	EXTI->PR = (1UL << 13) | (1UL << 10);
	EXTI15_10_IRQHandler();
	TEST_CHECK(g_ulTEST_Calls == 1U);
	TEST_CHECK(EXTI->PR == (1UL << 10));

	/* Releasing: the NVIC line is disabled with the last pin only. */
	TEST_CHECK(HAL_GPIO_ReleaseInterrupt(HAL_GPIO_2, HAL_GPIO_PIN_13) == HAL_SUCCESS);
	TEST_CHECK(NVIC->ICER[1] == 0U);
	TEST_CHECK(HAL_GPIO_ReleaseInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_10) == HAL_SUCCESS);
	TEST_CHECK((NVIC->ICER[1] & (1UL << (EXTI15_10_IRQn & 31))) != 0U);
	TEST_CHECK(HAL_GPIO_ConfigInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_13, &l_stEdge) == HAL_SUCCESS);
	TEST_CHECK(HAL_GPIO_ReleaseInterrupt(HAL_GPIO_0, HAL_GPIO_PIN_13) == HAL_SUCCESS);
}

/**************************************************************************************************
 * @brief   UART: recommendations 4, 5, 7, 10, 15, 16, 18 and 21
 **************************************************************************************************/
static void TEST_Uart(void)
{
	HAL_UART_FLAGS_T l_stFlags =
	{
		.m_ulBaud = 921600, .m_stTxPin = BOARD_VCP_TX_PIN, .m_stRxPin = BOARD_VCP_RX_PIN, .m_stInterrupt = { .m_ucPrePriority = 8 },
		.m_pfRxCallback = TEST_RxCallback, .m_pfTxCallback = TEST_TxCallback, .m_pfErrorCallback = TEST_ErrorCallback, .m_pvContext = &g_ulTEST_Calls
	};
	uint32_t l_ulBaud = 0;
	uint8_t l_ucByte = 0;

	TEST_CHECK(HAL_UART_Config(HAL_UART_NO, &l_stFlags) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_UART_WriteByte(HAL_UART_NO, 0) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_UART_WriteByte(HAL_UART_1, 0) == HAL_ERROR__NOT_CONFIGURED);

	/* 921600 baud at 32 MHz: 35 (0.8 percent off), not the truncated 34 (2.1 percent off). */
	TEST_CHECK(HAL_UART_Config(HAL_UART_1, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((RCC->APB1ENR & RCC_APB1ENR_USART2EN) != 0U);
	TEST_CHECK(USART2->BRR == 35U);
	TEST_CHECK(USART2->CR1 == (USART_CR1_TE | USART_CR1_RE));
	TEST_CHECK(USART2->CR2 == 0U);
	TEST_CHECK((GPIOA->MODER & 0xF0U) == 0xA0U);
	TEST_CHECK((GPIOA->AFR[0] & 0xFF00U) == 0x7700U);
	TEST_CHECK(NVIC_GetPriority(USART2_IRQn) == 8U);
	TEST_CHECK(HAL_UART_GetBaud(HAL_UART_1, &l_ulBaud) == HAL_SUCCESS);
	TEST_CHECK(l_ulBaud == 914285UL);

	/* 8 data bits with parity need the 9 bit frame; 7 data bits need a parity bit. */
	l_stFlags.m_ulBaud = 115200;
	l_stFlags.m_ucParity = HAL_UART_PARITY__EVEN;
	l_stFlags.m_ucStopBits = HAL_UART_STOPBITS__2;
	TEST_CHECK(HAL_UART_Config(HAL_UART_1, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(USART2->BRR == 278U);
	TEST_CHECK(USART2->CR1 == (USART_CR1_TE | USART_CR1_RE | USART_CR1_M | USART_CR1_PCE));
	TEST_CHECK(USART2->CR2 == USART_CR2_STOP_1);
	l_stFlags.m_ucParity = HAL_UART_PARITY__NONE;
	l_stFlags.m_ucDataBits = HAL_UART_DATABITS__7;
	TEST_CHECK(HAL_UART_Config(HAL_UART_1, &l_stFlags) == HAL_ERROR__PARAMETER);
	l_stFlags.m_ucDataBits = HAL_UART_DATABITS__8;
	l_stFlags.m_ucStopBits = HAL_UART_STOPBITS__1;

	/* A baud rate the bus clock cannot make is an error, not a silently wrong divider. */
	l_stFlags.m_ulBaud = 300;
	TEST_CHECK(HAL_UART_Config(HAL_UART_1, &l_stFlags) == HAL_ERROR__PARAMETER);
	l_stFlags.m_ulBaud = 115200;

	/* USART1 sits on the other bus and takes its pins from the configuration. */
	l_stFlags.m_stTxPin = (HAL_GPIO_PIN_T) { HAL_GPIO_0, HAL_GPIO_PIN_9, HAL_GPIO_FUNCTION__7 };
	l_stFlags.m_stRxPin = (HAL_GPIO_PIN_T) BOARD_NO_PIN;
	TEST_CHECK(HAL_UART_Config(HAL_UART_0, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK((RCC->APB2ENR & RCC_APB2ENR_USART1EN) != 0U);
	TEST_CHECK(USART1->CR1 == USART_CR1_TE);                                    /* No receiver without a pin */
	TEST_CHECK((GPIOA->AFR[1] & 0xF0U) == 0x70U);

	l_stFlags.m_stTxPin = (HAL_GPIO_PIN_T) BOARD_VCP_TX_PIN;
	l_stFlags.m_stRxPin = (HAL_GPIO_PIN_T) BOARD_VCP_RX_PIN;
	TEST_CHECK(HAL_UART_Config(HAL_UART_1, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(HAL_UART_SetState(HAL_UART_1, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(HAL_UART_SetInterrupt(HAL_UART_1, HAL_UART_RX, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(HAL_UART_SetInterrupt(HAL_UART_1, HAL_UART_TX, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(HAL_UART_SetInterrupt(HAL_UART_1, HAL_UART_TX_COMPLETE, HAL_ENABLE) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK((USART2->CR1 & (USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_UE)) == (USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_UE));

	/* Disabling the transmit source keeps the receive source and the NVIC line. */
	TEST_CHECK(HAL_UART_SetInterrupt(HAL_UART_1, HAL_UART_TX, HAL_DISABLE) == HAL_SUCCESS);
	TEST_CHECK((USART2->CR1 & (USART_CR1_RXNEIE | USART_CR1_TXEIE)) == USART_CR1_RXNEIE);
	TEST_CHECK((NVIC->ICER[USART2_IRQn >> 5] & (1UL << (USART2_IRQn & 31))) == 0U);

	/* Reception: the handler reads the data register and hands the byte to the callback. */
	TEST_Clear();
	USART2->SR = USART_SR_RXNE;
	USART2->DR = 0x41;
	USART2_IRQHandler();
	TEST_CHECK(g_ulTEST_Calls == 1U);
	TEST_CHECK(g_ulTEST_Value == 0x41U);
	TEST_CHECK(g_pvTEST_Context == &g_ulTEST_Calls);
	TEST_CHECK(g_ucTEST_Errors == 0U);
	TEST_CHECK(USART2->SR == USART_SR_RXNE);                                    /* Never written by the handler */

	/* An overrun is reported and the byte that was received is still delivered. */
	TEST_Clear();
	USART2->SR = USART_SR_RXNE | USART_SR_ORE;
	USART2->DR = 0x42;
	USART2_IRQHandler();
	TEST_CHECK(g_ucTEST_Errors == HAL_UART_ERROR__OVERRUN);
	TEST_CHECK(g_ulTEST_Value == 0x42U);

	/* A framing error is reported and the byte is dropped. */
	TEST_Clear();
	USART2->SR = USART_SR_RXNE | USART_SR_FE;
	USART2_IRQHandler();
	TEST_CHECK(g_ucTEST_Errors == HAL_UART_ERROR__FRAMING);
	TEST_CHECK(g_ulTEST_Calls == 0U);

	/* Transmission: the callback is asked for bytes, then the source switches itself off. */
	g_ucTEST_TxLeft = 1;
	USART2->SR = USART_SR_TXE;
	USART2->DR = 0;
	TEST_CHECK(HAL_UART_SetInterrupt(HAL_UART_1, HAL_UART_TX, HAL_ENABLE) == HAL_SUCCESS);
	USART2_IRQHandler();
	TEST_CHECK(USART2->DR == 0x5AU);
	TEST_CHECK((USART2->CR1 & USART_CR1_TXEIE) != 0U);
	USART2_IRQHandler();
	TEST_CHECK((USART2->CR1 & USART_CR1_TXEIE) == 0U);
	TEST_CHECK((USART2->CR1 & USART_CR1_RXNEIE) != 0U);

	/* Polling: a byte, then a timeout when nothing arrives, then a stuck transmitter. */
	USART2->SR = USART_SR_RXNE | USART_SR_TXE | USART_SR_TC;
	USART2->DR = 0x43;
	TEST_CHECK(HAL_UART_ReadByte(HAL_UART_1, &l_ucByte, 5) == HAL_SUCCESS);
	TEST_CHECK(l_ucByte == 0x43U);
	USART2->SR = USART_SR_TXE | USART_SR_TC;
	TEST_CHECK(HAL_UART_ReadByte(HAL_UART_1, &l_ucByte, 5) == HAL_ERROR__TIMEOUT);
	TEST_CHECK(HAL_UART_WriteBuff(HAL_UART_1, (const uint8_t *) "ab", 2) == HAL_SUCCESS);
	TEST_CHECK(USART2->DR == 'b');
	TEST_CHECK(HAL_UART_Flush(HAL_UART_1) == HAL_SUCCESS);
	USART2->SR = 0;
	TEST_CHECK(HAL_UART_WriteByte(HAL_UART_1, 'c') == HAL_ERROR__TIMEOUT);
	TEST_CHECK(USART2->DR == 'b');
}

/**************************************************************************************************
 * @brief   Timer: recommendations 4, 5, 36 and 40
 **************************************************************************************************/
static void TEST_Timer(void)
{
	HAL_TIMER_FLAGS_T l_stFlags = { .m_ucDirection = HAL_TIMER_DIRECTION__DOWN, .m_unPrescaler = 31, .m_ulUpdate = 999, .m_stInterrupt = { .m_ucPrePriority = 3 }, .m_pfUpdateCallback = TEST_Callback, .m_apfCompareCallback = { TEST_SecondCallback } };
	HAL_TIMER_CHANNEL_FLAGS_T l_stChannel = { .m_ucOutput = HAL_TIMER_OUTPUT__NONE, .m_ulCompare = 0 };
	uint32_t l_ulValue = 1;

	TEST_CHECK(HAL_TIMER_SetState(HAL_TIMER_0, HAL_ENABLE) == HAL_ERROR__NOT_CONFIGURED);

	/* Down counting sets the direction bit and leaves the timer stopped. */
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_0, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CR1 == TIM_CR1_DIR);
	TEST_CHECK(TIM2->PSC == 31U);
	TEST_CHECK(TIM2->ARR == 999U);
	TEST_CHECK(TIM2->EGR == TIM_EGR_UG);
	TEST_CHECK(TIM2->DIER == 0U);
	TEST_CHECK((RCC->APB1ENR & RCC_APB1ENR_TIM2EN) != 0U);

	/* Timers that cannot count down, or have no channel, say so. */
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_4, &l_stFlags) == HAL_ERROR__NOT_SUPPORTED);
	l_stFlags.m_ucDirection = HAL_TIMER_DIRECTION__UP;
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_4, &l_stFlags) == HAL_ERROR__PARAMETER);    /* Compare callback on TIM6 */
	l_stFlags.m_ulUpdate = 0x10000UL;
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_0, &l_stFlags) == HAL_ERROR__PARAMETER);    /* 16 bit timer */
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_3, &l_stFlags) == HAL_SUCCESS);             /* TIM5 is 32 bit */
	l_stFlags.m_ulUpdate = 999;

	/* The LSE clock exists on TIM9, TIM10 and TIM11 only. */
	l_stFlags.m_ucClock = HAL_TIMER_CLOCK__ALTERNATE;
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_0, &l_stFlags) == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_6, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(TIM9->OR == TIM9_OR_ITR1_RMP);
	TEST_CHECK(TIM9->SMCR == (TIM_SMCR_TS_0 | TIM_SMCR_SMS));
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_7, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(TIM10->OR == TIM_OR_ETR_RMP);
	TEST_CHECK(TIM10->SMCR == TIM_SMCR_ECE);
	l_stFlags.m_ucClock = HAL_TIMER_CLOCK__NORMAL;
	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_7, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(TIM10->OR == 0U);
	TEST_CHECK(TIM10->SMCR == 0U);

	TEST_CHECK(HAL_TIMER_Config(HAL_TIMER_0, &l_stFlags) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CR1 == 0U);

	/* A compare value of 0 is a value like any other. */
	TIM2->CCR1 = 1234;
	TEST_CHECK(HAL_TIMER_ConfigChannel(HAL_TIMER_0, HAL_TIMER_COMPARE1, &l_stChannel) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CCR1 == 0U);
	TEST_CHECK(HAL_TIMER_SetCompare(HAL_TIMER_0, HAL_TIMER_COMPARE2, 500) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CCR2 == 500U);
	TEST_CHECK(HAL_TIMER_GetCompare(HAL_TIMER_0, HAL_TIMER_COMPARE2, &l_ulValue) == HAL_SUCCESS);
	TEST_CHECK(l_ulValue == 500U);
	TEST_CHECK(HAL_TIMER_SetCompare(HAL_TIMER_7, HAL_TIMER_COMPARE2, 1) == HAL_ERROR__PARAMETER);   /* TIM10 has one channel */

	/* PWM on channel 2: mode 6 in the second byte of the mode register, output enabled. */
	l_stChannel = (HAL_TIMER_CHANNEL_FLAGS_T) { .m_ucOutput = HAL_TIMER_OUTPUT__PWM, .m_ulCompare = 250, .m_stPin = { HAL_GPIO_0, HAL_GPIO_PIN_1, HAL_GPIO_FUNCTION__1 } };
	TEST_CHECK(HAL_TIMER_ConfigChannel(HAL_TIMER_0, HAL_TIMER_COMPARE2, &l_stChannel) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CCMR1 == (6UL << 12));
	TEST_CHECK(TIM2->CCER == (1UL << 4));
	TEST_CHECK(TIM2->CCR2 == 250U);
	TEST_CHECK((GPIOA->AFR[0] & 0xF0U) == 0x10U);

	/* Enabling one source clears its stale flag only; disabling one keeps the other. */
	TEST_CHECK(HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_UPDATE, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(TIM2->SR == (uint32_t) ~1UL);
	TEST_CHECK(HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_COMPARE1, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(TIM2->SR == (uint32_t) ~2UL);
	TEST_CHECK(TIM2->DIER == 3U);
	TEST_CHECK(HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_COMPARE2, HAL_ENABLE) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_TIMER_SetInterrupt(HAL_TIMER_0, HAL_TIMER_COMPARE1, HAL_DISABLE) == HAL_SUCCESS);
	TEST_CHECK(TIM2->DIER == 1U);
	TEST_CHECK((NVIC->ICER[0] & (1UL << TIM2_IRQn)) == 0U);

	TEST_CHECK(HAL_TIMER_SetState(HAL_TIMER_0, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(TIM2->CR1 == TIM_CR1_CEN);

	/* Handling: only the enabled source is served, and only its flag is cleared. */
	TEST_Clear();
	TIM2->SR = 0x03;
	TIM2_IRQHandler();
	TEST_CHECK(g_ulTEST_Calls == 1U);
	TEST_CHECK(g_ulTEST_Value == 0U);
	TEST_CHECK(TIM2->SR == (uint32_t) ~1UL);

	TIM2->CNT = 77;
	TEST_CHECK(HAL_TIMER_GetCounter(HAL_TIMER_0) == 77U);
	TEST_CHECK(HAL_TIMER_GetCounter(HAL_TIMER_NO) == 0U);
	TEST_CHECK(HAL_TIMER_GetClock(HAL_TIMER_0, &l_ulValue) == HAL_SUCCESS);
	TEST_CHECK(l_ulValue == 32000000UL);
}

/**************************************************************************************************
 * @brief   SPI and I2C configuration: recommendations 8, 9, 23, 24 and 38
 **************************************************************************************************/
static void TEST_Buses(void)
{
	HAL_SPI_FLAGS_T l_stSpi = { .m_ucPolarity = HAL_SPI_POLARITY__HIGH, .m_ucPhase = HAL_SPI_PHASE__SECOND_EDGE, .m_ulSpeed = 8000000, .m_stClkPin = BOARD_SPI_CLK_PIN, .m_stMisoPin = BOARD_SPI_MISO_PIN, .m_stMosiPin = BOARD_SPI_MOSI_PIN, .m_stSsPin = BOARD_NO_PIN, .m_pfRxCallback = TEST_RxCallback };
	HAL_I2C_FLAGS_T l_stI2c = { .m_ulSpeed = 100000, .m_stSclPin = BOARD_I2C_SCL_PIN, .m_stSdaPin = BOARD_I2C_SDA_PIN };
	uint32_t l_ulSpeed = 0;
	uint8_t l_ucByte = 0;

	/* 8 MHz at a 32 MHz bus is clock / 4. */
	TEST_CHECK(HAL_SPI_Config(HAL_SPI_0, &l_stSpi) == HAL_SUCCESS);
	TEST_CHECK(SPI1->CR1 == (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_CPOL | SPI_CR1_CPHA | (1UL << SPI_CR1_BR_Pos)));
	TEST_CHECK((NVIC->ISER[SPI1_IRQn >> 5] & (1UL << (SPI1_IRQn & 31))) != 0U);
	TEST_CHECK((GPIOB->AFR[0] & 0xF000U) == 0x5000U);

	/* Configured again with another speed and mode: replaced, not ORed. */
	l_stSpi.m_ulSpeed = 4000000;
	l_stSpi.m_ucPolarity = HAL_SPI_POLARITY__LOW;
	l_stSpi.m_ucPhase = HAL_SPI_PHASE__FIRST_EDGE;
	TEST_CHECK(HAL_SPI_Config(HAL_SPI_0, &l_stSpi) == HAL_SUCCESS);
	TEST_CHECK(SPI1->CR1 == (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (2UL << SPI_CR1_BR_Pos)));
	TEST_CHECK(HAL_SPI_GetSpeed(HAL_SPI_0, &l_ulSpeed) == HAL_SUCCESS);
	TEST_CHECK(l_ulSpeed == 4000000UL);

	/* The pins and their function come from the configuration: SPI3 gets function 6. */
	l_stSpi.m_stClkPin = (HAL_GPIO_PIN_T) { HAL_GPIO_2, HAL_GPIO_PIN_10, HAL_GPIO_FUNCTION__6 };
	l_stSpi.m_stMisoPin = (HAL_GPIO_PIN_T) { HAL_GPIO_2, HAL_GPIO_PIN_11, HAL_GPIO_FUNCTION__6 };
	l_stSpi.m_stMosiPin = (HAL_GPIO_PIN_T) { HAL_GPIO_2, HAL_GPIO_PIN_12, HAL_GPIO_FUNCTION__6 };
	TEST_CHECK(HAL_SPI_Config(HAL_SPI_2, &l_stSpi) == HAL_SUCCESS);
	TEST_CHECK((GPIOC->AFR[1] & 0x000FFF00UL) == 0x00066600UL);
	TEST_CHECK((RCC->APB1ENR & RCC_APB1ENR_SPI3EN) != 0U);

	/* The receive source sets its enable bit (the old code cleared the wrong register). */
	TEST_CHECK(HAL_SPI_SetInterrupt(HAL_SPI_0, HAL_SPI_RX, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(SPI1->CR2 == (SPI_CR2_RXNEIE | SPI_CR2_ERRIE));
	TEST_CHECK(SPI1->SR == 0U);
	TEST_Clear();
	SPI1->SR = SPI_SR_RXNE;
	SPI1->DR = 0x99;
	SPI1_IRQHandler();
	TEST_CHECK(g_ulTEST_Value == 0x99U);
	TEST_CHECK(HAL_SPI_SetInterrupt(HAL_SPI_0, HAL_SPI_RX, HAL_DISABLE) == HAL_SUCCESS);

	/* A transfer on a module that is not enabled times out instead of hanging. */
	SPI1->SR = SPI_SR_TXE;
	TEST_CHECK(HAL_SPI_TransferByte(HAL_SPI_0, 0x12, &l_ucByte) == HAL_ERROR__TIMEOUT);
	SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
	SPI1->DR = 0;
	TEST_CHECK(HAL_SPI_TransferByte(HAL_SPI_0, 0x34, &l_ucByte) == HAL_SUCCESS);
	TEST_CHECK(l_ucByte == 0x34U);                                              /* The simulated data register loops back */

	/* I2C timing from the bus clock. */
	TEST_CHECK(HAL_I2C_Config(HAL_I2C_0, &l_stI2c) == HAL_SUCCESS);
	TEST_CHECK(I2C1->CR2 == 32U);
	TEST_CHECK(I2C1->CCR == 160U);
	TEST_CHECK(I2C1->TRISE == 33U);
	TEST_CHECK((GPIOB->OTYPER & (3UL << 8)) == (3UL << 8));                     /* Open drain */
	TEST_CHECK(HAL_I2C_IsDeviceReady(HAL_I2C_0, 0x50) == HAL_ERROR__NOT_CONFIGURED);  /* Not enabled */
	TEST_CHECK(HAL_I2C_SetState(HAL_I2C_0, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(I2C1->CR1 == I2C_CR1_PE);
	TEST_CHECK(HAL_I2C_IsDeviceReady(HAL_I2C_0, 0x80) == HAL_ERROR__PARAMETER);

	/* A device that does not acknowledge its address is reported. */
	I2C1->SR1 = I2C_SR1_AF;
	GPIOB->IDR = (1UL << 8) | (1UL << 9);
	TEST_CHECK(HAL_I2C_IsDeviceReady(HAL_I2C_0, 0x50) == HAL_ERROR__NACK);
	TEST_CHECK(I2C1->CR1 == I2C_CR1_PE);                                        /* Module reset and enabled again */

	/* A bus that never answers is a timeout. */
	I2C1->SR1 = 0;
	TEST_CHECK(HAL_I2C_Read(HAL_I2C_0, 0x50, &l_ucByte, 1) == HAL_ERROR__TIMEOUT);
}

/**************************************************************************************************
 * @brief   ADC, DAC, DMA, CRC, flash, missing modules, low power: recommendations 2, 11, 12, 13, 30
 **************************************************************************************************/
static void TEST_Others(void)
{
	static const HAL_ADC_FLAGS_T l_stAdc = { .m_ucResolution = HAL_ADC_RESOLUTION__10_BITS, .m_pfConversionCallback = TEST_AdcCallback };
	static const HAL_ADC_CHANNEL_FLAGS_T l_stAdcChannel = { .m_ucSampleTime = HAL_ADC_SAMPLE_TIME__96, .m_stPin = { HAL_GPIO_0, HAL_GPIO_PIN_1, 0 } };
	static const HAL_DAC_FLAGS_T l_stDac = { .m_ucBuffer = HAL_DISABLE, .m_stPin = { HAL_GPIO_0, HAL_GPIO_PIN_5, 0 } };
	static const HAL_DMA_FLAGS_T l_stDma = { .m_ucDirection = HAL_DMA_DIRECTION__MEMORY_TO_PERIPHERAL, .m_ucMemoryIncrement = HAL_ENABLE, .m_ucMemorySize = HAL_DMA_SIZE__16, .m_ucPriority = HAL_DMA_PRIORITY__HIGH, .m_pfCompleteCallback = TEST_Callback };
	static const HAL_CLOCK_FLAGS_T l_stClock = BOARD_CLOCK_FLAGS;
	static const HAL_CRC_FLAGS_T l_stCrc16 = { .m_ulPolynomial = 0x1021, .m_ulInitialValue = 0xFFFF, .m_ucSize = 16 };
	static const HAL_LPM_FLAGS_T l_stStandby = { .m_ucWakeUpPins = HAL_LPM_WAKEUP_PIN__1 };
	uint32_t l_ulValue = 0x11111111UL;
	uint16_t l_unValue = 0;
	uint8_t l_aucBlock[16] = { 0 };

	/* ADC: enabling requests the HSI, which then survives a clock change. */
	TEST_CHECK(HAL_ADC_ReadChannelRaw(HAL_ADC_0, 1, &l_unValue) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_ADC_Config(HAL_ADC_0, &l_stAdc) == HAL_SUCCESS);
	TEST_CHECK(ADC1->CR1 == (1UL << ADC_CR1_RES_Pos));
	TEST_CHECK(HAL_ADC_ConfigChannel(HAL_ADC_0, 1, &l_stAdcChannel) == HAL_SUCCESS);
	TEST_CHECK(ADC1->SMPR3 == (5UL << 3));
	TEST_CHECK((GPIOA->MODER & 0x0CU) == 0x0CU);                                /* Analog */
	TEST_CHECK(HAL_ADC_ReadChannelRaw(HAL_ADC_0, 1, &l_unValue) == HAL_ERROR__NOT_CONFIGURED);    /* Not enabled */
	TEST_CHECK(HAL_ADC_SetState(HAL_ADC_0, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK((RCC->CR & RCC_CR_HSIRDY) != 0U);
	TEST_CHECK(HAL_ADC_ReadChannelRaw(HAL_ADC_0, 1, &l_unValue) == HAL_SUCCESS);
	TEST_CHECK(l_unValue == 0x0ABCU);
	TEST_CHECK(ADC1->SQR5 == 1U);
	TEST_CHECK(HAL_CLOCK_Update(&l_stClock) == HAL_SUCCESS);
	TEST_CHECK((RCC->CR & RCC_CR_HSION) != 0U);
	TEST_CHECK(HAL_ADC_ReadChannelRaw(HAL_ADC_0, HAL_ADC_CHANNEL_NO, &l_unValue) == HAL_ERROR__PARAMETER);
	TEST_Clear();
	TEST_CHECK(HAL_ADC_StartConversion(HAL_ADC_0, 1) == HAL_SUCCESS);
	TEST_CHECK((ADC1->CR1 & ADC_CR1_EOCSIE) != 0U);

	SIM_Step();
	ADC1_IRQHandler();
	TEST_CHECK(g_ulTEST_Value == 0x0ABCU);
	TEST_CHECK(HAL_ADC_SetState(HAL_ADC_0, HAL_DISABLE) == HAL_SUCCESS);
	TEST_CHECK((RCC->CR & RCC_CR_HSION) == 0U);

	/* DAC channel 2: its bits are 16 positions up, and the buffer bit means buffer off. */
	TEST_CHECK(HAL_DAC_Config(HAL_DAC_1, &l_stDac) == HAL_SUCCESS);
	TEST_CHECK(DAC->CR == DAC_CR_BOFF2);
	TEST_CHECK(HAL_DAC_SetState(HAL_DAC_1, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(DAC->CR == (DAC_CR_BOFF2 | DAC_CR_EN2));
	TEST_CHECK(HAL_DAC_Write(HAL_DAC_1, 2048) == HAL_SUCCESS);
	TEST_CHECK(DAC->DHR12R2 == 2048U);
	TEST_CHECK(HAL_DAC_Write(HAL_DAC_1, 4096) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_DAC_Write(HAL_DAC_0, 1) == HAL_ERROR__NOT_CONFIGURED);

	/* DMA1 channel 3 (index 2). */
	TEST_CHECK(HAL_DMA_Config(HAL_DMA_1, 5, &l_stDma) == HAL_ERROR__PARAMETER);      /* DMA2 has 5 channels */
	TEST_CHECK(HAL_DMA_Config(HAL_DMA_0, HAL_DMA_CHANNEL_2, &l_stDma) == HAL_SUCCESS);
	TEST_CHECK(DMA1_Channel3->CCR == (DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_MSIZE_0 | DMA_CCR_PL_1));
	TEST_CHECK(DMA1->IFCR == (0x0FUL << 8));
	TEST_CHECK(HAL_DMA_SetInterrupt(HAL_DMA_0, HAL_DMA_CHANNEL_2, HAL_DMA_COMPLETE, HAL_ENABLE) == HAL_SUCCESS);
	TEST_CHECK(HAL_DMA_SetInterrupt(HAL_DMA_0, HAL_DMA_CHANNEL_2, HAL_DMA_HALF, HAL_ENABLE) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_DMA_Start(HAL_DMA_0, HAL_DMA_CHANNEL_2, 0x40004404UL, 0x20000100UL, 0) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_DMA_Start(HAL_DMA_0, HAL_DMA_CHANNEL_2, 0x40004404UL, 0x20000100UL, 12) == HAL_SUCCESS);
	TEST_CHECK(DMA1_Channel3->CPAR == 0x40004404UL);
	TEST_CHECK(DMA1_Channel3->CMAR == 0x20000100UL);
	TEST_CHECK(DMA1_Channel3->CNDTR == 12U);
	TEST_CHECK((DMA1_Channel3->CCR & (DMA_CCR_EN | DMA_CCR_TCIE)) == (DMA_CCR_EN | DMA_CCR_TCIE));
	TEST_CHECK(HAL_DMA_GetRemaining(HAL_DMA_0, HAL_DMA_CHANNEL_2, &l_unValue) == HAL_SUCCESS);
	TEST_CHECK(l_unValue == 12U);
	TEST_CHECK(HAL_DMA_Stop(HAL_DMA_0, HAL_DMA_CHANNEL_2) == HAL_SUCCESS);
	TEST_CHECK((DMA1_Channel3->CCR & DMA_CCR_EN) == 0U);

	/* Modules this microcontroller does not have fail and leave the output untouched. */
	TEST_CHECK(HAL_RNG_Config() == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(HAL_RNG_Read(&l_ulValue) == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(l_ulValue == 0x11111111UL);
	TEST_CHECK(HAL_AES_Encrypt(l_aucBlock, l_aucBlock) == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(HAL_FLASH_MassErase() == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(HAL_CRC_Config(&l_stCrc16) == HAL_ERROR__NOT_SUPPORTED);
	TEST_CHECK(HAL_CRC_Calculate(l_aucBlock, 16, &l_ulValue) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_CRC_Config(NULL) == HAL_SUCCESS);
	TEST_CHECK((RCC->AHBENR & RCC_AHBENR_CRCEN) != 0U);

	/* Flash: locked and out of range requests are refused before anything is written. */
	TEST_CHECK(HAL_FLASH_Program(0x08000000UL, 0) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_FLASH_PageErase(HAL_FLASH_GetPageCount()) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_FLASH_GetPageCount() == 2048U);
	TEST_CHECK(HAL_FLASH_GetPageAddress(2047) == 0x0807FF00UL);
	FLASH->PECR = 0;
	TEST_CHECK(HAL_FLASH_Program(0x08000002UL, 0) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_FLASH_Program(0x08080000UL, 0) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_FLASH_ProgramBuff(0x0807FFFCUL, &l_ulValue, 2) == HAL_ERROR__PARAMETER);
	FLASH->SR = FLASH_SR_BSY;
	TEST_CHECK(HAL_FLASH_Program(0x08000000UL, 0) == HAL_ERROR__BUSY);
	FLASH->SR = 0;
	TEST_CHECK(HAL_FLASH_Lock() == HAL_SUCCESS);
	TEST_CHECK(FLASH->PECR == (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK));

	/* Low-power run: slow MSI, voltage range 2, then the regulator bits; and back. */
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__STANDBY + 1U, NULL) == HAL_ERROR__PARAMETER);
	TEST_CHECK(HAL_LPM_Exit(HAL_LPM_MODE__LOW_POWER_RUN) == HAL_ERROR__NOT_CONFIGURED);
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__LOW_POWER_RUN, NULL) == HAL_SUCCESS);
	TEST_CHECK(HAL_CLOCK_Get() == 131072UL);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS_1);
	TEST_CHECK((PWR->CR & (PWR_CR_LPRUN | PWR_CR_LPSDSR)) == (PWR_CR_LPRUN | PWR_CR_LPSDSR));
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__SLEEP, NULL) == HAL_ERROR__BUSY);
	TEST_CHECK(HAL_LPM_Exit(HAL_LPM_MODE__LOW_POWER_RUN) == HAL_SUCCESS);
	TEST_CHECK((PWR->CR & (PWR_CR_LPRUN | PWR_CR_LPSDSR)) == 0U);
	TEST_CHECK((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS_0);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);

	/* Sleep: the deep sleep bit is clear and the tick is paused around the wait. */
	g_ulSIM_WaitForInterrupt = 0;
	SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__SLEEP, NULL) == HAL_SUCCESS);
	TEST_CHECK(g_ulSIM_WaitForInterrupt == 1U);
	TEST_CHECK(SCB->SCR == 0U);
	TEST_CHECK((SIM_SYSTICK->CTRL & SysTick_CTRL_TICKINT_Msk) != 0U);

	/* Stop: the clear bit of the wake-up flag is written as 1, no low-power run bit, clock restored. */
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__STOP, NULL) == HAL_SUCCESS);
	TEST_CHECK(g_ulSIM_WaitForInterrupt == 2U);
	TEST_CHECK((PWR->CR & PWR_CR_CWUF) != 0U);
	TEST_CHECK((PWR->CR & (PWR_CR_LPRUN | PWR_CR_PDDS | PWR_CR_LPSDSR)) == 0U);
	TEST_CHECK(SCB->SCR == 0U);
	TEST_CHECK(HAL_CLOCK_Get() == 32000000UL);

	/* Standby (the simulated core returns from the wait, so the call reports that). */
	TEST_CHECK(HAL_LPM_Enter(HAL_LPM_MODE__STANDBY, &l_stStandby) == HAL_ERROR);
	TEST_CHECK((PWR->CSR & PWR_CSR_EWUP1) != 0U);
	TEST_CHECK((PWR->CR & PWR_CR_CWUF) != 0U);
}

int main(void)
{
	if ((mmap((void *) 0x40000000UL, 0x30000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED)
		|| (mmap((void *) 0xE000E000UL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED)
		|| (mmap((void *) 0x08000000UL, 0x80000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED))
	{
		printf("Cannot map the simulated register ranges on this machine\n");

		return 2;
	}

	SIM_Reset();

	TEST_Clock();
	TEST_Gpio();
	TEST_Uart();
	TEST_Timer();
	TEST_Buses();
	TEST_Others();

	printf("%d checks, %d failed\n", g_nChecks, g_nFailures);
	printf("%s\n", (g_nFailures == 0) ? "All register tests passed" : "Register tests FAILED");

	return (g_nFailures == 0) ? 0 : 1;
}
