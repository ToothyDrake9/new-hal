/**************************************************************************************************
 * @file    hal_devices.c
 * @brief   STM32L1 family: where each module instance is (registers, clock gate, interrupt line)
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Everything here is constant and stays in flash. The linker drops the tables of the
 *          drivers that are not used.
 **************************************************************************************************/
#include "hal_gpio.h"
#include "hal_clock.h"
#include "hal_private.h"

/* The address table of hal_port.h must agree with the device header. */
_Static_assert(GPIOA_BASE == 0x40020000UL, "GPIO address");
_Static_assert(GPIOB_BASE == 0x40020400UL, "GPIO address");
_Static_assert(GPIOC_BASE == 0x40020800UL, "GPIO address");
_Static_assert(GPIOD_BASE == 0x40020C00UL, "GPIO address");
_Static_assert(GPIOE_BASE == 0x40021000UL, "GPIO address");
_Static_assert(GPIOF_BASE == 0x40021800UL, "GPIO address");
_Static_assert(GPIOG_BASE == 0x40021C00UL, "GPIO address");
_Static_assert(GPIOH_BASE == 0x40021400UL, "GPIO address");

const HAL_ClockGate g_astHAL_GPIO_Clock[HAL_GPIO_NO] =
{
	{ &RCC->AHBENR, RCC_AHBENR_GPIOAEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOBEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOCEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIODEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOEEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOFEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOGEN },
	{ &RCC->AHBENR, RCC_AHBENR_GPIOHEN }
};

/* Port code in the EXTI routing register. It follows the memory order: A B C D E H F G. */
const uint8_t g_aucHAL_GPIO_ExtiCode[HAL_GPIO_NO] = { 0, 1, 2, 3, 4, 6, 7, 5 };

const IRQn_Type g_aeHAL_GPIO_IRQn[16] =
{
	EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn,
	EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,
	EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn
};

const HAL_UART_Device g_astHAL_UART_Device[HAL_UART_NO] =
{
	{ USART1, { &RCC->APB2ENR, RCC_APB2ENR_USART1EN }, USART1_IRQn, HAL_CLOCK_BUS__APB2 },
	{ USART2, { &RCC->APB1ENR, RCC_APB1ENR_USART2EN }, USART2_IRQn, HAL_CLOCK_BUS__APB1 },
	{ USART3, { &RCC->APB1ENR, RCC_APB1ENR_USART3EN }, USART3_IRQn, HAL_CLOCK_BUS__APB1 },
	{ UART4,  { &RCC->APB1ENR, RCC_APB1ENR_UART4EN },  UART4_IRQn,  HAL_CLOCK_BUS__APB1 },
	{ UART5,  { &RCC->APB1ENR, RCC_APB1ENR_UART5EN },  UART5_IRQn,  HAL_CLOCK_BUS__APB1 }
};

const HAL_SPI_Device g_astHAL_SPI_Device[HAL_SPI_NO] =
{
	{ SPI1, { &RCC->APB2ENR, RCC_APB2ENR_SPI1EN }, SPI1_IRQn, HAL_CLOCK_BUS__APB2 },
	{ SPI2, { &RCC->APB1ENR, RCC_APB1ENR_SPI2EN }, SPI2_IRQn, HAL_CLOCK_BUS__APB1 },
	{ SPI3, { &RCC->APB1ENR, RCC_APB1ENR_SPI3EN }, SPI3_IRQn, HAL_CLOCK_BUS__APB1 }
};

const HAL_I2C_Device g_astHAL_I2C_Device[HAL_I2C_NO] =
{
	{ I2C1, { &RCC->APB1ENR, RCC_APB1ENR_I2C1EN }, HAL_CLOCK_BUS__APB1 },
	{ I2C2, { &RCC->APB1ENR, RCC_APB1ENR_I2C2EN }, HAL_CLOCK_BUS__APB1 }
};

const HAL_TIMER_Device g_astHAL_TIMER_Device[HAL_TIMER_NO] =
{
	{ TIM2,  { &RCC->APB1ENR, RCC_APB1ENR_TIM2EN },  TIM2_IRQn,  HAL_CLOCK_BUS__APB1, 4, 0, 0, HAL_TIMER_LSE__NONE },
	{ TIM3,  { &RCC->APB1ENR, RCC_APB1ENR_TIM3EN },  TIM3_IRQn,  HAL_CLOCK_BUS__APB1, 4, 0, 0, HAL_TIMER_LSE__NONE },
	{ TIM4,  { &RCC->APB1ENR, RCC_APB1ENR_TIM4EN },  TIM4_IRQn,  HAL_CLOCK_BUS__APB1, 4, 0, 0, HAL_TIMER_LSE__NONE },
	{ TIM5,  { &RCC->APB1ENR, RCC_APB1ENR_TIM5EN },  TIM5_IRQn,  HAL_CLOCK_BUS__APB1, 4, 1, 0, HAL_TIMER_LSE__NONE },
	{ TIM6,  { &RCC->APB1ENR, RCC_APB1ENR_TIM6EN },  TIM6_IRQn,  HAL_CLOCK_BUS__APB1, 0, 0, 1, HAL_TIMER_LSE__NONE },
	{ TIM7,  { &RCC->APB1ENR, RCC_APB1ENR_TIM7EN },  TIM7_IRQn,  HAL_CLOCK_BUS__APB1, 0, 0, 1, HAL_TIMER_LSE__NONE },
	{ TIM9,  { &RCC->APB2ENR, RCC_APB2ENR_TIM9EN },  TIM9_IRQn,  HAL_CLOCK_BUS__APB2, 2, 0, 1, HAL_TIMER_LSE__ITR1 },
	{ TIM10, { &RCC->APB2ENR, RCC_APB2ENR_TIM10EN }, TIM10_IRQn, HAL_CLOCK_BUS__APB2, 1, 0, 1, HAL_TIMER_LSE__ETR },
	{ TIM11, { &RCC->APB2ENR, RCC_APB2ENR_TIM11EN }, TIM11_IRQn, HAL_CLOCK_BUS__APB2, 1, 0, 1, HAL_TIMER_LSE__ETR }
};

const HAL_DMA_Device g_astHAL_DMA_Device[HAL_DMA_NO] =
{
	{
		DMA1, { &RCC->AHBENR, RCC_AHBENR_DMA1EN }, 7,
		{ DMA1_Channel1_IRQn, DMA1_Channel2_IRQn, DMA1_Channel3_IRQn, DMA1_Channel4_IRQn, DMA1_Channel5_IRQn, DMA1_Channel6_IRQn, DMA1_Channel7_IRQn }
	},
	{
		DMA2, { &RCC->AHBENR, RCC_AHBENR_DMA2EN }, 5,
		{ DMA2_Channel1_IRQn, DMA2_Channel2_IRQn, DMA2_Channel3_IRQn, DMA2_Channel4_IRQn, DMA2_Channel5_IRQn, DMA2_Channel5_IRQn, DMA2_Channel5_IRQn }
	}
};

const HAL_ClockGate g_stHAL_DAC_Clock = { &RCC->APB1ENR, RCC_APB1ENR_DACEN };
const HAL_ClockGate g_stHAL_ADC_Clock = { &RCC->APB2ENR, RCC_APB2ENR_ADC1EN };
const HAL_ClockGate g_stHAL_CRC_Clock = { &RCC->AHBENR, RCC_AHBENR_CRCEN };
