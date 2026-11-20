/**************************************************************************************************
 * @file    hal_family_irq.h
 * @brief   STM32L1 family: interrupt vectors of each driver
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Each driver includes this file once, at its end, after defining which block it wants.
 *          The vectors therefore live in the same object file as the driver: a driver that is
 *          linked brings its vectors, a driver that is not used costs nothing, and the weak
 *          default handlers of the startup file can never win by accident.
 **************************************************************************************************/

#ifdef HAL_FAMILY_IRQ_CLOCK
void SysTick_Handler(void);
void SysTick_Handler(void) { HAL_CLOCK_SysTick_Interrupt(); }
#endif

#ifdef HAL_FAMILY_IRQ_GPIO
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void EXTI0_IRQHandler(void) { HAL_GPIO_Interrupt(0x0001UL); }
void EXTI1_IRQHandler(void) { HAL_GPIO_Interrupt(0x0002UL); }
void EXTI2_IRQHandler(void) { HAL_GPIO_Interrupt(0x0004UL); }
void EXTI3_IRQHandler(void) { HAL_GPIO_Interrupt(0x0008UL); }
void EXTI4_IRQHandler(void) { HAL_GPIO_Interrupt(0x0010UL); }
void EXTI9_5_IRQHandler(void) { HAL_GPIO_Interrupt(0x03E0UL); }
void EXTI15_10_IRQHandler(void) { HAL_GPIO_Interrupt(0xFC00UL); }
#endif

#ifdef HAL_FAMILY_IRQ_UART
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART3_IRQHandler(void);
void UART4_IRQHandler(void);
void UART5_IRQHandler(void);
void USART1_IRQHandler(void) { HAL_UART_Interrupt(HAL_UART_0); }
void USART2_IRQHandler(void) { HAL_UART_Interrupt(HAL_UART_1); }
void USART3_IRQHandler(void) { HAL_UART_Interrupt(HAL_UART_2); }
void UART4_IRQHandler(void) { HAL_UART_Interrupt(HAL_UART_3); }
void UART5_IRQHandler(void) { HAL_UART_Interrupt(HAL_UART_4); }
#endif

#ifdef HAL_FAMILY_IRQ_SPI
void SPI1_IRQHandler(void);
void SPI2_IRQHandler(void);
void SPI3_IRQHandler(void);
void SPI1_IRQHandler(void) { HAL_SPI_Interrupt(HAL_SPI_0); }
void SPI2_IRQHandler(void) { HAL_SPI_Interrupt(HAL_SPI_1); }
void SPI3_IRQHandler(void) { HAL_SPI_Interrupt(HAL_SPI_2); }
#endif

#ifdef HAL_FAMILY_IRQ_TIMER
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);
void TIM4_IRQHandler(void);
void TIM5_IRQHandler(void);
void TIM6_IRQHandler(void);
void TIM7_IRQHandler(void);
void TIM9_IRQHandler(void);
void TIM10_IRQHandler(void);
void TIM11_IRQHandler(void);
void TIM2_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_0); }
void TIM3_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_1); }
void TIM4_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_2); }
void TIM5_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_3); }
void TIM6_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_4); }
void TIM7_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_5); }
void TIM9_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_6); }
void TIM10_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_7); }
void TIM11_IRQHandler(void) { HAL_TIMER_Interrupt(HAL_TIMER_8); }
#endif

#ifdef HAL_FAMILY_IRQ_ADC
void ADC1_IRQHandler(void);
void ADC1_IRQHandler(void) { HAL_ADC_Interrupt(HAL_ADC_0); }
#endif

#ifdef HAL_FAMILY_IRQ_DMA
void DMA1_Channel1_IRQHandler(void);
void DMA1_Channel2_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void);
void DMA1_Channel4_IRQHandler(void);
void DMA1_Channel5_IRQHandler(void);
void DMA1_Channel6_IRQHandler(void);
void DMA1_Channel7_IRQHandler(void);
void DMA2_Channel1_IRQHandler(void);
void DMA2_Channel2_IRQHandler(void);
void DMA2_Channel3_IRQHandler(void);
void DMA2_Channel4_IRQHandler(void);
void DMA2_Channel5_IRQHandler(void);
void DMA1_Channel1_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 0); }
void DMA1_Channel2_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 1); }
void DMA1_Channel3_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 2); }
void DMA1_Channel4_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 3); }
void DMA1_Channel5_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 4); }
void DMA1_Channel6_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 5); }
void DMA1_Channel7_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_0, 6); }
void DMA2_Channel1_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_1, 0); }
void DMA2_Channel2_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_1, 1); }
void DMA2_Channel3_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_1, 2); }
void DMA2_Channel4_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_1, 3); }
void DMA2_Channel5_IRQHandler(void) { HAL_DMA_Interrupt(HAL_DMA_1, 4); }
#endif
