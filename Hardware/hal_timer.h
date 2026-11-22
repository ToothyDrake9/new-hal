/**************************************************************************************************
 * @file    hal_timer.h
 * @brief   Microprocessor TIMER module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: callbacks are given at configuration, compare channels are configured one
 *          by one (a compare value of 0 is valid), compare outputs and PWM were added
 **************************************************************************************************/
#ifndef HAL_TIMER_H_
#define HAL_TIMER_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

enum HAL_TIMER_Clock
{
	HAL_TIMER_CLOCK__NORMAL = ((uint8_t) 0x00),   // The bus clock of the timer
	HAL_TIMER_CLOCK__ALTERNATE                    // The LSE, on the timers that can count it
};

enum HAL_TIMER_Direction
{
	HAL_TIMER_DIRECTION__UP = ((uint8_t) 0x00),
	HAL_TIMER_DIRECTION__DOWN
};

enum HAL_TIMER_Interrupts
{
	HAL_TIMER_UPDATE = ((uint8_t) 0x00),
	HAL_TIMER_COMPARE1,
	HAL_TIMER_COMPARE2,
	HAL_TIMER_COMPARE3,
	HAL_TIMER_COMPARE4,

	HAL_TIMER_INTERRUPT_NO
};

enum HAL_TIMER_Output
{
	HAL_TIMER_OUTPUT__NONE = ((uint8_t) 0x00), // The compare event is only an interrupt source
	HAL_TIMER_OUTPUT__TOGGLE,                  // The pin toggles at each compare match
	HAL_TIMER_OUTPUT__PWM,                     // The pin is high while the counter is below the compare value
	HAL_TIMER_OUTPUT__PWM_INVERTED             // The pin is low while the counter is below the compare value
};

/* Runs in interrupt context. */
typedef void (*HAL_TIMER_CALLBACK_T) (void *p_pvContext);

typedef struct
{
	uint8_t m_ucClock : 1;               // e.g. HAL_TIMER_CLOCK__NORMAL
	uint8_t m_ucDirection : 1;           // e.g. HAL_TIMER_DIRECTION__UP
	uint8_t m_ucOnePulse : 1;            // HAL_ENABLE - the counter stops at the update event
	uint16_t m_unPrescaler;              // The clock is divided by this value plus 1
	uint32_t m_ulUpdate;                 // Auto-reload value: the period is this value plus 1
	HAL_INTERRUPT_T m_stInterrupt;         // Priority of the timer interrupt
	HAL_TIMER_CALLBACK_T m_pfUpdateCallback;       // NULL - not used
	HAL_TIMER_CALLBACK_T m_apfCompareCallback[4];  // NULL - not used; index 0 is HAL_TIMER_COMPARE1
	void *m_pvContext;                   // Given back to the callbacks
}HAL_TIMER_FLAGS_T;

typedef struct
{
	uint8_t m_ucOutput;                  // e.g. HAL_TIMER_OUTPUT__NONE
	uint32_t m_ulCompare;                // Compare value; 0 is valid
	HAL_GPIO_PIN_T m_stPin;                // Output pin; module HAL_GPIO_NONE when there is none
}HAL_TIMER_CHANNEL_FLAGS_T;

HAL_RETURN_CODE_T HAL_TIMER_Config(uint8_t p_ucModule, const HAL_TIMER_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_TIMER_ConfigChannel(uint8_t p_ucModule, uint8_t p_ucCompare, const HAL_TIMER_CHANNEL_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_TIMER_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_RETURN_CODE_T HAL_TIMER_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState);
HAL_RETURN_CODE_T HAL_TIMER_SetCounter(uint8_t p_ucModule, uint32_t p_ulCounter);
HAL_RETURN_CODE_T HAL_TIMER_SetCompare(uint8_t p_ucModule, uint8_t p_ucCompare, uint32_t p_ulValue);
HAL_RETURN_CODE_T HAL_TIMER_GetCompare(uint8_t p_ucModule, uint8_t p_ucCompare, uint32_t *p_pulValue);
HAL_RETURN_CODE_T HAL_TIMER_GetClock(uint8_t p_ucModule, uint32_t *p_pulClock);
uint32_t HAL_TIMER_GetCounter(uint8_t p_ucModule);

#endif /* HAL_TIMER_H_ */
