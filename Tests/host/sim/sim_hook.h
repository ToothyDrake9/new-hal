/**************************************************************************************************
 * @file    sim_hook.h
 * @brief   Register level tests: makes the simulated hardware advance with the program
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks Force-included in every source of the PC test build. Each access to the SysTick
 *          registers first advances the simulated hardware by one step. Every wait of the HAL
 *          polls a timeout, and a timeout reads the SysTick counter, so the simulated hardware
 *          moves exactly while the HAL waits for it: the test is single threaded and repeatable.
 **************************************************************************************************/
#ifndef SIM_HOOK_H_
#define SIM_HOOK_H_

#include "stm32l1xx.h"

SysTick_Type *SIM_SysTick(void);

#define SIM_SYSTICK ((SysTick_Type *) SysTick_BASE)

#undef SysTick
#define SysTick (SIM_SysTick())

#endif /* SIM_HOOK_H_ */
