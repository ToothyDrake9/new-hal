/**************************************************************************************************
 * @file    typedefs.h
 * @brief   Type definitions shared by the HAL and the application
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten for the new HAL: no device header, no NULL redefinition, 0 means success
 **************************************************************************************************/
#ifndef TYPEDEFS_H_
#define TYPEDEFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef float fp32_t;

/* Return codes. HAL_SUCCESS is 0, so "if (HAL_xxx())" reads as "if it failed". */
typedef enum
{
	HAL_SUCCESS = ((uint8_t) 0x00),
	HAL_ERROR,                  // Generic failure
	HAL_ERROR__PARAMETER,       // Argument out of range or NULL
	HAL_ERROR__TIMEOUT,         // The hardware did not answer in time
	HAL_ERROR__BUSY,            // The peripheral or the bus is busy
	HAL_ERROR__NOT_SUPPORTED,   // Not available on this microcontroller
	HAL_ERROR__NOT_CONFIGURED,  // HAL_xxx_Config was not called for this module
	HAL_ERROR__CONFLICT,        // The resource is already used with other settings
	HAL_ERROR__NACK,            // I2C address or data byte not acknowledged
	HAL_ERROR__BUS              // Bus error (arbitration, overrun, mode fault, flash error flags)
}HAL_Return_Code;

enum HAL_State
{
	HAL_DISABLE = ((uint8_t) 0x00),
	HAL_ENABLE
};

/* NVIC priority of a module. One priority per interrupt line, see the module remarks. */
typedef struct
{
	uint8_t m_ucPrePriority : 4;
	uint8_t m_ucSubPriority : 4;
}HAL_Interrupt;

#endif /* TYPEDEFS_H_ */
