/**************************************************************************************************
 * @file    hal_rng.h
 * @brief   Microprocessor RNG module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: a real random number or an error, never a placeholder value
 **************************************************************************************************/
#ifndef HAL_RNG_H_
#define HAL_RNG_H_

#include "typedefs.h"
#include "hal_port.h"

HAL_RETURN_CODE_T HAL_RNG_Config(void);
HAL_RETURN_CODE_T HAL_RNG_Read(uint32_t *p_pulValue);

#endif /* HAL_RNG_H_ */
