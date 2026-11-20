/**************************************************************************************************
 * @file    hal_flash.h
 * @brief   Microprocessor FLASH module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: addresses are checked, the error flags are checked after each operation,
 *          every wait has a timeout, half page programming runs from RAM
 **************************************************************************************************/
#ifndef HAL_FLASH_H_
#define HAL_FLASH_H_

#include "typedefs.h"
#include "hal_port.h"

/* Timeout of one erase or program operation, in milliseconds. */
#ifndef HAL_FLASH_TIMEOUT_MS
#define HAL_FLASH_TIMEOUT_MS 50U
#endif

HAL_Return_Code HAL_FLASH_Unlock(void);
HAL_Return_Code HAL_FLASH_Lock(void);
HAL_Return_Code HAL_FLASH_PageErase(uint16_t p_unPage);
HAL_Return_Code HAL_FLASH_MassErase(void);
HAL_Return_Code HAL_FLASH_Program(uint32_t p_ulAddress, uint32_t p_ulData);
HAL_Return_Code HAL_FLASH_ProgramBuff(uint32_t p_ulAddress, const uint32_t *p_pulData, uint16_t p_unSize);
HAL_Return_Code HAL_FLASH_FastProgram(uint32_t p_ulAddress, const uint32_t *p_pulData);
uint32_t HAL_FLASH_GetPageAddress(uint16_t p_unPage);
uint16_t HAL_FLASH_GetPageCount(void);

#endif /* HAL_FLASH_H_ */
