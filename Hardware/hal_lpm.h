/**************************************************************************************************
 * @file    hal_lpm.h
 * @brief   Microprocessor Low Power Mode module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: one enter function with the mode as argument, optional callbacks instead of
 *          ten mandatory ones, the clock configuration is saved and restored by the module
 **************************************************************************************************/
#ifndef HAL_LPM_H_
#define HAL_LPM_H_

#include "typedefs.h"
#include "hal_port.h"

enum HAL_LPM_Mode
{
	HAL_LPM_MODE__SLEEP = ((uint8_t) 0x00), // Core stopped, peripherals running; any interrupt wakes up
	HAL_LPM_MODE__LOW_POWER_RUN,            // Running from a slow clock with the regulator in low power
	HAL_LPM_MODE__LOW_POWER_SLEEP,          // Sleep, entered from the low-power run conditions
	HAL_LPM_MODE__STOP,                     // All clocks stopped, RAM kept; an EXTI line or the RTC wakes up
	HAL_LPM_MODE__STANDBY                   // Everything off; wake-up restarts the program from reset
};

/* Bits of m_ucWakeUpPins. */
#define HAL_LPM_WAKEUP_PIN__1 ((uint8_t) 0x01)
#define HAL_LPM_WAKEUP_PIN__2 ((uint8_t) 0x02)
#define HAL_LPM_WAKEUP_PIN__3 ((uint8_t) 0x04)

/* Runs in the context of the caller of HAL_LPM_Enter. */
typedef void (*HAL_LPM_CALLBACK) (void *p_pvContext);

typedef struct
{
	uint8_t m_ucKeepTick : 1;          // Sleep modes: HAL_ENABLE keeps the 1 ms tick, which wakes the core every millisecond
	uint8_t m_ucUltraLowPower : 1;     // Stop and standby: HAL_ENABLE switches the internal reference off
	uint8_t m_ucFastWakeUp : 1;        // With ultra low power: HAL_ENABLE does not wait for the reference at wake-up
	uint8_t m_ucWakeUpPins : 3;        // Standby: the wake-up pins to enable (e.g. HAL_LPM_WAKEUP_PIN__1)
	HAL_LPM_CALLBACK m_pfEnterCallback; // NULL - not used; called before the mode is entered
	HAL_LPM_CALLBACK m_pfExitCallback;  // NULL - not used; called after wake-up, with the clock restored
	void *m_pvContext;                 // Given back to the callbacks
}HAL_LPM_Flags;

HAL_Return_Code HAL_LPM_Enter(uint8_t p_ucMode, const HAL_LPM_Flags *p_pstFlags);
HAL_Return_Code HAL_LPM_Exit(uint8_t p_ucMode);
bool HAL_LPM_WasInStandby(void);

#endif /* HAL_LPM_H_ */
