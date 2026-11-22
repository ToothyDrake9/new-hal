/**************************************************************************************************
 * @file    hal_dac.h
 * @brief   Microprocessor DAC module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: status returns, the pin is given at configuration
 **************************************************************************************************/
#ifndef HAL_DAC_H_
#define HAL_DAC_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

#define HAL_DAC_MAXIMUM 4095U

typedef struct
{
	uint8_t m_ucBuffer : 1;   // HAL_ENABLE - output buffer on (lower output impedance, output cannot reach the rails)
	HAL_GPIO_PIN_T m_stPin;     // The analog output pin; module HAL_GPIO_NONE to leave the pin alone
}HAL_DAC_FLAGS_T;

HAL_RETURN_CODE_T HAL_DAC_Config(uint8_t p_ucModule, const HAL_DAC_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_DAC_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_RETURN_CODE_T HAL_DAC_Write(uint8_t p_ucModule, uint16_t p_unValue);

#endif /* HAL_DAC_H_ */
