/**************************************************************************************************
 * @file    startup_stm32l152re.c
 * @brief   STM32L152RE: vector table, reset handler and default interrupt handler
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Every handler is a weak alias of the default handler. The HAL drivers define the
 *          handlers of the modules they serve; the application may define any other one.
 **************************************************************************************************/
#include <stdint.h>
#include "stm32l1xx.h"

/* Symbols of the linker script. */
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern void (*__preinit_array_start[]) (void);
extern void (*__preinit_array_end[]) (void);
extern void (*__init_array_start[]) (void);
extern void (*__init_array_end[]) (void);

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);
void SystemInit(void);

#define HAL_WEAK_HANDLER(p_name) void p_name(void) __attribute__((weak, alias("Default_Handler")))

HAL_WEAK_HANDLER(NMI_Handler);
HAL_WEAK_HANDLER(HardFault_Handler);
HAL_WEAK_HANDLER(MemManage_Handler);
HAL_WEAK_HANDLER(BusFault_Handler);
HAL_WEAK_HANDLER(UsageFault_Handler);
HAL_WEAK_HANDLER(SVC_Handler);
HAL_WEAK_HANDLER(DebugMon_Handler);
HAL_WEAK_HANDLER(PendSV_Handler);
HAL_WEAK_HANDLER(SysTick_Handler);
HAL_WEAK_HANDLER(WWDG_IRQHandler);
HAL_WEAK_HANDLER(PVD_IRQHandler);
HAL_WEAK_HANDLER(TAMPER_STAMP_IRQHandler);
HAL_WEAK_HANDLER(RTC_WKUP_IRQHandler);
HAL_WEAK_HANDLER(FLASH_IRQHandler);
HAL_WEAK_HANDLER(RCC_IRQHandler);
HAL_WEAK_HANDLER(EXTI0_IRQHandler);
HAL_WEAK_HANDLER(EXTI1_IRQHandler);
HAL_WEAK_HANDLER(EXTI2_IRQHandler);
HAL_WEAK_HANDLER(EXTI3_IRQHandler);
HAL_WEAK_HANDLER(EXTI4_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel1_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel2_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel3_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel4_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel5_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel6_IRQHandler);
HAL_WEAK_HANDLER(DMA1_Channel7_IRQHandler);
HAL_WEAK_HANDLER(ADC1_IRQHandler);
HAL_WEAK_HANDLER(USB_HP_IRQHandler);
HAL_WEAK_HANDLER(USB_LP_IRQHandler);
HAL_WEAK_HANDLER(DAC_IRQHandler);
HAL_WEAK_HANDLER(COMP_IRQHandler);
HAL_WEAK_HANDLER(EXTI9_5_IRQHandler);
HAL_WEAK_HANDLER(LCD_IRQHandler);
HAL_WEAK_HANDLER(TIM9_IRQHandler);
HAL_WEAK_HANDLER(TIM10_IRQHandler);
HAL_WEAK_HANDLER(TIM11_IRQHandler);
HAL_WEAK_HANDLER(TIM2_IRQHandler);
HAL_WEAK_HANDLER(TIM3_IRQHandler);
HAL_WEAK_HANDLER(TIM4_IRQHandler);
HAL_WEAK_HANDLER(I2C1_EV_IRQHandler);
HAL_WEAK_HANDLER(I2C1_ER_IRQHandler);
HAL_WEAK_HANDLER(I2C2_EV_IRQHandler);
HAL_WEAK_HANDLER(I2C2_ER_IRQHandler);
HAL_WEAK_HANDLER(SPI1_IRQHandler);
HAL_WEAK_HANDLER(SPI2_IRQHandler);
HAL_WEAK_HANDLER(USART1_IRQHandler);
HAL_WEAK_HANDLER(USART2_IRQHandler);
HAL_WEAK_HANDLER(USART3_IRQHandler);
HAL_WEAK_HANDLER(EXTI15_10_IRQHandler);
HAL_WEAK_HANDLER(RTC_Alarm_IRQHandler);
HAL_WEAK_HANDLER(USB_FS_WKUP_IRQHandler);
HAL_WEAK_HANDLER(TIM6_IRQHandler);
HAL_WEAK_HANDLER(TIM7_IRQHandler);
HAL_WEAK_HANDLER(TIM5_IRQHandler);
HAL_WEAK_HANDLER(SPI3_IRQHandler);
HAL_WEAK_HANDLER(UART4_IRQHandler);
HAL_WEAK_HANDLER(UART5_IRQHandler);
HAL_WEAK_HANDLER(DMA2_Channel1_IRQHandler);
HAL_WEAK_HANDLER(DMA2_Channel2_IRQHandler);
HAL_WEAK_HANDLER(DMA2_Channel3_IRQHandler);
HAL_WEAK_HANDLER(DMA2_Channel4_IRQHandler);
HAL_WEAK_HANDLER(DMA2_Channel5_IRQHandler);
HAL_WEAK_HANDLER(COMP_ACQ_IRQHandler);

typedef void (*HAL_VECTOR_T) (void);

/* The first entry is the initial stack pointer; the linker script keeps this section first. */
__attribute__((section(".isr_vector"), used))
const HAL_VECTOR_T g_apfVectors[] =
{
	(HAL_VECTOR_T) (uintptr_t) &_estack,
	Reset_Handler,
	NMI_Handler,
	HardFault_Handler,
	MemManage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	0,
	0,
	0,
	0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,
	WWDG_IRQHandler,          /*  0 */
	PVD_IRQHandler,           /*  1 */
	TAMPER_STAMP_IRQHandler,  /*  2 */
	RTC_WKUP_IRQHandler,      /*  3 */
	FLASH_IRQHandler,         /*  4 */
	RCC_IRQHandler,           /*  5 */
	EXTI0_IRQHandler,         /*  6 */
	EXTI1_IRQHandler,         /*  7 */
	EXTI2_IRQHandler,         /*  8 */
	EXTI3_IRQHandler,         /*  9 */
	EXTI4_IRQHandler,         /* 10 */
	DMA1_Channel1_IRQHandler, /* 11 */
	DMA1_Channel2_IRQHandler, /* 12 */
	DMA1_Channel3_IRQHandler, /* 13 */
	DMA1_Channel4_IRQHandler, /* 14 */
	DMA1_Channel5_IRQHandler, /* 15 */
	DMA1_Channel6_IRQHandler, /* 16 */
	DMA1_Channel7_IRQHandler, /* 17 */
	ADC1_IRQHandler,          /* 18 */
	USB_HP_IRQHandler,        /* 19 */
	USB_LP_IRQHandler,        /* 20 */
	DAC_IRQHandler,           /* 21 */
	COMP_IRQHandler,          /* 22 */
	EXTI9_5_IRQHandler,       /* 23 */
	LCD_IRQHandler,           /* 24 */
	TIM9_IRQHandler,          /* 25 */
	TIM10_IRQHandler,         /* 26 */
	TIM11_IRQHandler,         /* 27 */
	TIM2_IRQHandler,          /* 28 */
	TIM3_IRQHandler,          /* 29 */
	TIM4_IRQHandler,          /* 30 */
	I2C1_EV_IRQHandler,       /* 31 */
	I2C1_ER_IRQHandler,       /* 32 */
	I2C2_EV_IRQHandler,       /* 33 */
	I2C2_ER_IRQHandler,       /* 34 */
	SPI1_IRQHandler,          /* 35 */
	SPI2_IRQHandler,          /* 36 */
	USART1_IRQHandler,        /* 37 */
	USART2_IRQHandler,        /* 38 */
	USART3_IRQHandler,        /* 39 */
	EXTI15_10_IRQHandler,     /* 40 */
	RTC_Alarm_IRQHandler,     /* 41 */
	USB_FS_WKUP_IRQHandler,   /* 42 */
	TIM6_IRQHandler,          /* 43 */
	TIM7_IRQHandler,          /* 44 */
	0,                        /* 45 */
	TIM5_IRQHandler,          /* 46 */
	SPI3_IRQHandler,          /* 47 */
	UART4_IRQHandler,         /* 48 */
	UART5_IRQHandler,         /* 49 */
	DMA2_Channel1_IRQHandler, /* 50 */
	DMA2_Channel2_IRQHandler, /* 51 */
	DMA2_Channel3_IRQHandler, /* 52 */
	DMA2_Channel4_IRQHandler, /* 53 */
	DMA2_Channel5_IRQHandler, /* 54 */
	0,                        /* 55 */
	COMP_ACQ_IRQHandler       /* 56 */
};

/**************************************************************************************************
 * @brief   Early hardware setup, before the C environment exists
 * @remarks Weak: the application may replace it. It may not use initialized or zeroed variables.
 **************************************************************************************************/
__attribute__((weak)) void SystemInit(void)
{
	/* Point the core to the vector table. */
	SCB->VTOR = (uint32_t) (uintptr_t) g_apfVectors;
}

/**************************************************************************************************
 * @brief   Reset handler: prepares RAM, then calls main
 * @remarks The copy and clear loops are kept as loops, so this file does not need the C library.
 **************************************************************************************************/
__attribute__((noreturn, optimize("no-tree-loop-distribute-patterns")))
void Reset_Handler(void)
{
	const uint32_t *l_pulSource = &_sidata;
	uint32_t *l_pulDestination;
	uint32_t l_ulIndex;
	uint32_t l_ulCount;

	SystemInit();

	/* Copy the initialized data (and the functions that run from RAM) from flash. */
	for (l_pulDestination = &_sdata; l_pulDestination < &_edata; l_pulDestination++)
	{
		*l_pulDestination = *l_pulSource++;
	}

	/* Clear the zero initialized data. */
	for (l_pulDestination = &_sbss; l_pulDestination < &_ebss; l_pulDestination++)
	{
		*l_pulDestination = 0;
	}

	/* Run the constructors, if any. */
	l_ulCount = (uint32_t) (__preinit_array_end - __preinit_array_start);

	for (l_ulIndex = 0; l_ulIndex < l_ulCount; l_ulIndex++)
	{
		__preinit_array_start[l_ulIndex]();
	}

	l_ulCount = (uint32_t) (__init_array_end - __init_array_start);

	for (l_ulIndex = 0; l_ulIndex < l_ulCount; l_ulIndex++)
	{
		__init_array_start[l_ulIndex]();
	}

	(void) main();

	for (;;)
	{
		/* main returned. */
	}
}

/**************************************************************************************************
 * @brief   Default handler: an interrupt or a fault that nobody handles ends here
 * @remarks Read the active vector number in the IPSR register with the debugger.
 **************************************************************************************************/
void Default_Handler(void)
{
	for (;;)
	{
		/* Unexpected interrupt. */
	}
}
