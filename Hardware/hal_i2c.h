/**************************************************************************************************
 * @file    hal_i2c.h
 * @brief   Microprocessor I2C module (master)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: whole transfers instead of start/byte/stop steps, so the acknowledge and
 *          stop sequencing follows the reference manual for every length; a missing device is
 *          reported as HAL_ERROR__NACK instead of blocking forever
 **************************************************************************************************/
#ifndef HAL_I2C_H_
#define HAL_I2C_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

enum HAL_I2C_Stop
{
	HAL_I2C_NO_STOP = ((uint8_t) 0x00), // Keep the bus: the next transfer begins with a repeated start
	HAL_I2C_STOP
};

/* Timeout of each step of a transfer (start, address, one byte), in milliseconds. */
#ifndef HAL_I2C_TIMEOUT_MS
#define HAL_I2C_TIMEOUT_MS 25U
#endif

typedef struct
{
	uint32_t m_ulSpeed;         // Bus speed in Hz, up to 400000 (e.g. 100000)
	HAL_GPIO_Pin m_stSclPin;
	HAL_GPIO_Pin m_stSdaPin;
}HAL_I2C_Flags;

HAL_Return_Code HAL_I2C_Config(uint8_t p_ucModule, const HAL_I2C_Flags *p_pstFlags);
HAL_Return_Code HAL_I2C_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_Return_Code HAL_I2C_Write(uint8_t p_ucModule, uint8_t p_ucAddress, const uint8_t *p_pucBuff, uint16_t p_unSize, uint8_t p_ucStop);
HAL_Return_Code HAL_I2C_Read(uint8_t p_ucModule, uint8_t p_ucAddress, uint8_t *p_pucBuff, uint16_t p_unSize);
HAL_Return_Code HAL_I2C_WriteRead(uint8_t p_ucModule, uint8_t p_ucAddress, const uint8_t *p_pucTxBuff, uint16_t p_unTxSize, uint8_t *p_pucRxBuff, uint16_t p_unRxSize);
HAL_Return_Code HAL_I2C_IsDeviceReady(uint8_t p_ucModule, uint8_t p_ucAddress);
HAL_Return_Code HAL_I2C_Recover(uint8_t p_ucModule);

#endif /* HAL_I2C_H_ */
