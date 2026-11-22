/**************************************************************************************************
 * @file    hal_crc.h
 * @brief   Microprocessor CRC module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: a real result or an error, never a placeholder value
 **************************************************************************************************/
#ifndef HAL_CRC_H_
#define HAL_CRC_H_

#include "typedefs.h"
#include "hal_port.h"

#define HAL_CRC_POLYNOMIAL__CRC32 0x04C11DB7UL

typedef struct
{
	uint32_t m_ulPolynomial;    // e.g. HAL_CRC_POLYNOMIAL__CRC32
	uint32_t m_ulInitialValue;  // e.g. 0xFFFFFFFF
	uint8_t m_ucSize;           // Polynomial size in bits (e.g. 32)
}HAL_CRC_FLAGS_T;

HAL_RETURN_CODE_T HAL_CRC_Config(const HAL_CRC_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_CRC_Calculate(const uint8_t *p_pucData, uint16_t p_unSize, uint32_t *p_pulCrc);

#endif /* HAL_CRC_H_ */
