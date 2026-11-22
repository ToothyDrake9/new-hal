/**************************************************************************************************
 * @file    hal_private.h
 * @brief   Definitions shared by the HAL sources. Not for the application.
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Only HAL source files include this header: it is the one place where the CMSIS device
 *          header enters the build.
 **************************************************************************************************/
#ifndef HAL_PRIVATE_H_
#define HAL_PRIVATE_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_family.h"

#define HAL_ARRAY_SIZE(p_array) (sizeof(p_array) / sizeof((p_array)[0]))

/* The bit that gates the clock of one peripheral. */
typedef struct
{
	volatile uint32_t *m_pulRegister;
	uint32_t m_ulMask;
}HAL_CLOCK_GATE_T;

typedef struct
{
	USART_TypeDef *m_pstRegisters;
	HAL_CLOCK_GATE_T m_stClock;
	IRQn_Type m_eIRQn;
	uint8_t m_ucBus;
}HAL_UART_DEVICE_T;

typedef struct
{
	SPI_TypeDef *m_pstRegisters;
	HAL_CLOCK_GATE_T m_stClock;
	IRQn_Type m_eIRQn;
	uint8_t m_ucBus;
}HAL_SPI_DEVICE_T;

typedef struct
{
	I2C_TypeDef *m_pstRegisters;
	HAL_CLOCK_GATE_T m_stClock;
	uint8_t m_ucBus;
}HAL_I2C_DEVICE_T;

enum HAL_TIMER_LseRoute
{
	HAL_TIMER_LSE__NONE = ((uint8_t) 0x00),
	HAL_TIMER_LSE__ITR1,   // LSE reaches the timer on internal trigger 1
	HAL_TIMER_LSE__ETR     // LSE reaches the timer on the external trigger input
};

typedef struct
{
	TIM_TypeDef *m_pstRegisters;
	HAL_CLOCK_GATE_T m_stClock;
	IRQn_Type m_eIRQn;
	uint8_t m_ucBus;
	uint8_t m_ucChannels;  // Number of compare channels
	uint8_t m_ucWide;      // 1 - 32 bit counter
	uint8_t m_ucUpOnly;    // 1 - the counter cannot count down
	uint8_t m_ucLseRoute;  // How the alternate (LSE) clock is connected
}HAL_TIMER_DEVICE_T;

typedef struct
{
	DMA_TypeDef *m_pstRegisters;
	HAL_CLOCK_GATE_T m_stClock;
	uint8_t m_ucChannels;
	DMA_Channel_TypeDef *m_apstChannel[HAL_DMA_CHANNEL_NO];
	IRQn_Type m_aeIRQn[HAL_DMA_CHANNEL_NO];
}HAL_DMA_DEVICE_T;

/* Device tables. They are constant, live in flash and are provided by the family. */
extern const HAL_CLOCK_GATE_T g_astHAL_GPIO_Clock[HAL_GPIO_NO];
extern const uint8_t g_aucHAL_GPIO_ExtiCode[HAL_GPIO_NO];
extern const IRQn_Type g_aeHAL_GPIO_IRQn[16];
extern const HAL_UART_DEVICE_T g_astHAL_UART_Device[HAL_UART_NO];
extern const HAL_SPI_DEVICE_T g_astHAL_SPI_Device[HAL_SPI_NO];
extern const HAL_I2C_DEVICE_T g_astHAL_I2C_Device[HAL_I2C_NO];
extern const HAL_TIMER_DEVICE_T g_astHAL_TIMER_Device[HAL_TIMER_NO];
extern const HAL_DMA_DEVICE_T g_astHAL_DMA_Device[HAL_DMA_NO];
extern const HAL_CLOCK_GATE_T g_stHAL_DAC_Clock;
extern const HAL_CLOCK_GATE_T g_stHAL_ADC_Clock;
extern const HAL_CLOCK_GATE_T g_stHAL_CRC_Clock;

/**************************************************************************************************
 * @brief   Masks the interrupts
 * @return  The previous state, to give to HAL_PRIVATE_ExitCritical
 * @remarks Used around read-modify-write of registers shared between interrupts and the main code.
 **************************************************************************************************/
static inline uint32_t HAL_PRIVATE_EnterCritical(void)
{
	uint32_t l_ulState = __get_PRIMASK();

	__disable_irq();

	return l_ulState;
}

/**************************************************************************************************
 * @brief     Restores the interrupt state
 * @param[in] p_ulState - the value returned by HAL_PRIVATE_EnterCritical
 **************************************************************************************************/
static inline void HAL_PRIVATE_ExitCritical(uint32_t p_ulState)
{
	__set_PRIMASK(p_ulState);
}

/**************************************************************************************************
 * @brief     Sets or clears bits of a register that interrupts may also modify
 * @param[in] p_pulRegister - the register
 * @param[in] p_ulClear - the bits to clear
 * @param[in] p_ulSet - the bits to set
 **************************************************************************************************/
static inline void HAL_PRIVATE_Modify(volatile uint32_t *p_pulRegister, uint32_t p_ulClear, uint32_t p_ulSet)
{
	uint32_t l_ulState = HAL_PRIVATE_EnterCritical();

	*p_pulRegister = (*p_pulRegister & ~p_ulClear) | p_ulSet;

	HAL_PRIVATE_ExitCritical(l_ulState);
}

/**************************************************************************************************
 * @brief     Enables the clock of a peripheral
 * @param[in] p_pstClock - the clock gate
 * @remarks   The register is read back: the peripheral is usable two clock cycles after the write.
 **************************************************************************************************/
static inline void HAL_PRIVATE_ClockEnable(const HAL_CLOCK_GATE_T *p_pstClock)
{
	volatile uint32_t l_ulDummy;

	HAL_PRIVATE_Modify(p_pstClock->m_pulRegister, 0, p_pstClock->m_ulMask);

	l_ulDummy = *p_pstClock->m_pulRegister;
	(void) l_ulDummy;
}

/**************************************************************************************************
 * @brief     Sets the priority of an interrupt line and enables it in the NVIC
 * @param[in] p_eIRQn - the interrupt line
 * @param[in] p_pstInterrupt - the priority
 **************************************************************************************************/
static inline void HAL_PRIVATE_EnableIRQ(IRQn_Type p_eIRQn, const HAL_INTERRUPT_T *p_pstInterrupt)
{
	NVIC_SetPriority(p_eIRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), p_pstInterrupt->m_ucPrePriority, p_pstInterrupt->m_ucSubPriority));
	NVIC_EnableIRQ(p_eIRQn);
}

#endif /* HAL_PRIVATE_H_ */
