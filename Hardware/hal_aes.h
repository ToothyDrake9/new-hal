/**************************************************************************************************
 * @file    hal_aes.h
 * @brief   Microprocessor AES module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: every function reports whether the hardware did the work
 **************************************************************************************************/
#ifndef HAL_AES_H_
#define HAL_AES_H_

#include "typedefs.h"
#include "hal_port.h"

#define HAL_AES_BLOCK_SIZE 16U
#define HAL_AES_KEY_SIZE   16U

enum HAL_AES_Mode
{
	HAL_AES_MODE__ECB = ((uint8_t) 0x00),
	HAL_AES_MODE__CBC,
	HAL_AES_MODE__CTR
};

HAL_Return_Code HAL_AES_Config(uint8_t p_ucMode);
HAL_Return_Code HAL_AES_SetKey(const uint8_t *p_pucKey);
HAL_Return_Code HAL_AES_Encrypt(const uint8_t *p_pucData, uint8_t *p_pucResult);
HAL_Return_Code HAL_AES_Decrypt(const uint8_t *p_pucData, uint8_t *p_pucResult);

#endif /* HAL_AES_H_ */
