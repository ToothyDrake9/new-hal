/**************************************************************************************************
 * @file    hal_flash.c
 * @brief   Microprocessor FLASH module for the STM32L1 family (program memory)
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the control bits are cleared after each operation, the error flags are
 *          read and reported, the half page write is executed from RAM with interrupts masked as
 *          the reference manual requires, an operation that is not available says so
 **************************************************************************************************/
#include "hal_flash.h"
#include "hal_clock.h"
#include "hal_private.h"

#define HAL_FLASH_ERROR_FLAGS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR | FLASH_SR_OPTVERRUSR)
#define HAL_FLASH_HALF_PAGE_WORDS (HAL_FLASH_HALF_PAGE_SIZE / 4U)
#define HAL_FLASH_PAGE_COUNT ((uint16_t) ((FLASH_END + 1UL - FLASH_BASE) / HAL_FLASH_PAGE_SIZE))

/* Unlock keys of the reference manual. The device header does not define them. */
#define HAL_FLASH_PEKEY1  0x89ABCDEFUL
#define HAL_FLASH_PEKEY2  0x02030405UL
#define HAL_FLASH_PRGKEY1 0x8C9DAEBFUL
#define HAL_FLASH_PRGKEY2 0x13141516UL

/* Code placed in RAM: it runs while the flash memory cannot be read. */
#define HAL_FLASH_RAM_FUNCTION __attribute__((section(".ramfunc"), noinline, long_call))

/**************************************************************************************************
 * @brief   Waits until the flash memory is not busy, then reads and clears the error flags
 * @return  HAL_SUCCESS, HAL_ERROR__TIMEOUT, or HAL_ERROR__BUS when an error flag was set (write
 *          protection, alignment, size, option validity)
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_FLASH_Wait(void)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	HAL_CLOCK_TIMEOUT_T l_stTimeout;

	HAL_CLOCK_TimeoutStart(&l_stTimeout, HAL_FLASH_TIMEOUT_MS);

	while (FLASH->SR & FLASH_SR_BSY)
	{
		if (HAL_CLOCK_TimeoutExpired(&l_stTimeout))
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;

			break;
		}
	}

	if ((l_eReturnCode == HAL_SUCCESS) && (FLASH->SR & HAL_FLASH_ERROR_FLAGS))
	{
		l_eReturnCode = HAL_ERROR__BUS;
	}

	/* The flags are cleared by writing 1. */
	FLASH->SR = HAL_FLASH_ERROR_FLAGS | FLASH_SR_EOP;

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Checks that the memory is unlocked, idle and that an address can be programmed
 * @param[in] p_ulAddress - the first address
 * @param[in] p_ulSize - the number of bytes
 * @param[in] p_ulAlignment - the required alignment of the address in bytes
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED (locked) or HAL_ERROR__BUSY
 **************************************************************************************************/
static HAL_RETURN_CODE_T HAL_FLASH_Check(uint32_t p_ulAddress, uint32_t p_ulSize, uint32_t p_ulAlignment)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;

	if ((p_ulAddress < FLASH_BASE) || (p_ulAddress > FLASH_END) || (p_ulSize > (FLASH_END + 1UL - p_ulAddress)) || ((p_ulAddress % p_ulAlignment) != 0U))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}
	else if (FLASH->PECR & (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK))
	{
		l_eReturnCode = HAL_ERROR__NOT_CONFIGURED;
	}
	else if (FLASH->SR & FLASH_SR_BSY)
	{
		l_eReturnCode = HAL_ERROR__BUSY;
	}
	else
	{
		/* Clear the flags of a previous operation. */
		FLASH->SR = HAL_FLASH_ERROR_FLAGS | FLASH_SR_EOP;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     Writes half a page. Runs from RAM, with interrupts masked by the caller.
 * @param[in] p_pulDestination - the first address of the half page
 * @param[in] p_pulSource - the 32 words to be written, in RAM
 * @return    The flash status register after the operation
 * @remarks   Nothing here may call code or read data located in flash: no function call, no
 *            timeout from the clock module. The wait is bounded by a loop counter instead.
 **************************************************************************************************/
static HAL_FLASH_RAM_FUNCTION uint32_t HAL_FLASH_WriteHalfPage(volatile uint32_t *p_pulDestination, const uint32_t *p_pulSource)
{
	uint32_t l_ulGuard = 0x00200000UL;
	uint32_t l_ulIndex;

	/* Set the program operation. */
	FLASH->PECR |= FLASH_PECR_FPRG;
	FLASH->PECR |= FLASH_PECR_PROG;

	/* Write the data at the specified address. */
	for (l_ulIndex = 0; l_ulIndex < HAL_FLASH_HALF_PAGE_WORDS; l_ulIndex++)
	{
		p_pulDestination[l_ulIndex] = p_pulSource[l_ulIndex];
	}

	/* Wait until half page program is complete. */
	while ((FLASH->SR & FLASH_SR_BSY) && (l_ulGuard != 0U))
	{
		l_ulGuard--;
	}

	/* Clear the program operation. */
	FLASH->PECR &= ~FLASH_PECR_PROG;
	FLASH->PECR &= ~FLASH_PECR_FPRG;

	return FLASH->SR;
}

/**************************************************************************************************
 * @brief   FLASH unlock
 * @return  HAL_SUCCESS, or HAL_ERROR when the memory stays locked (a wrong key sequence was
 *          written since the last reset)
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_Unlock(void)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_SUCCESS;
	uint32_t l_ulState = HAL_PRIVATE_EnterCritical();

	/* The two keys of a pair must be written one after the other. */
	if (FLASH->PECR & FLASH_PECR_PELOCK)
	{
		FLASH->PEKEYR = HAL_FLASH_PEKEY1;
		FLASH->PEKEYR = HAL_FLASH_PEKEY2;
	}

	if (FLASH->PECR & FLASH_PECR_PRGLOCK)
	{
		FLASH->PRGKEYR = HAL_FLASH_PRGKEY1;
		FLASH->PRGKEYR = HAL_FLASH_PRGKEY2;
	}

	HAL_PRIVATE_ExitCritical(l_ulState);

	if (FLASH->PECR & (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK))
	{
		l_eReturnCode = HAL_ERROR;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   FLASH lock
 * @return  HAL_SUCCESS
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_Lock(void)
{
	FLASH->PECR |= FLASH_PECR_PRGLOCK;
	FLASH->PECR |= FLASH_PECR_PELOCK;

	return HAL_SUCCESS;
}

/**************************************************************************************************
 * @brief   Gets the number of pages of the program memory
 * @return  The number of pages
 **************************************************************************************************/
uint16_t HAL_FLASH_GetPageCount(void)
{
	return HAL_FLASH_PAGE_COUNT;
}

/**************************************************************************************************
 * @brief     Gets the address of a page
 * @param[in] p_unPage - the page number, from 0
 * @return    The first address of the page; 0 when the page does not exist
 **************************************************************************************************/
uint32_t HAL_FLASH_GetPageAddress(uint16_t p_unPage)
{
	return (p_unPage < HAL_FLASH_PAGE_COUNT) ? (FLASH_BASE + ((uint32_t) p_unPage * HAL_FLASH_PAGE_SIZE)) : 0U;
}

/**************************************************************************************************
 * @brief     FLASH page erase
 * @param[in] p_unPage - the page number, from 0 (a page is HAL_FLASH_PAGE_SIZE bytes)
 * @return    HAL_SUCCESS, HAL_ERROR__PARAMETER, HAL_ERROR__NOT_CONFIGURED (locked),
 *            HAL_ERROR__BUSY, HAL_ERROR__TIMEOUT or HAL_ERROR__BUS (error flag, e.g. the page is
 *            write protected)
 * @remarks   The page that holds the running code can be erased too: check the page number.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_PageErase(uint16_t p_unPage)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_ERROR__PARAMETER;
	uint32_t l_ulAddress = HAL_FLASH_GetPageAddress(p_unPage);

	if (l_ulAddress != 0U)
	{
		l_eReturnCode = HAL_FLASH_Check(l_ulAddress, HAL_FLASH_PAGE_SIZE, HAL_FLASH_PAGE_SIZE);
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Set the erase operation. */
		FLASH->PECR |= FLASH_PECR_ERASE;
		FLASH->PECR |= FLASH_PECR_PROG;

		/* Writing zero to the first word of the page starts the erase. */
		*(volatile uint32_t *) l_ulAddress = 0;

		l_eReturnCode = HAL_FLASH_Wait();

		/* Clear the page erase operation: both bits. */
		FLASH->PECR &= ~FLASH_PECR_PROG;
		FLASH->PECR &= ~FLASH_PECR_ERASE;
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief   FLASH mass erase
 * @return  HAL_ERROR__NOT_SUPPORTED: this family has no mass erase command for the program
 *          memory. Erase the pages one by one with HAL_FLASH_PageErase.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_MassErase(void)
{
	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief     FLASH program: writes one word
 * @param[in] p_ulAddress - the address, a multiple of 4
 * @param[in] p_ulData - the word to be written
 * @return    Same as HAL_FLASH_PageErase
 * @remarks   The word must be erased (zero on this family) before it is written.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_Program(uint32_t p_ulAddress, uint32_t p_ulData)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_FLASH_Check(p_ulAddress, 4, 4);

	if (l_eReturnCode == HAL_SUCCESS)
	{
		/* Write the data at the specified address. */
		*(volatile uint32_t *) p_ulAddress = p_ulData;

		l_eReturnCode = HAL_FLASH_Wait();

		/* Read back: a word that was not erased before does not hold what was written. */
		if ((l_eReturnCode == HAL_SUCCESS) && (*(volatile uint32_t *) p_ulAddress != p_ulData))
		{
			l_eReturnCode = HAL_ERROR__BUS;
		}
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     FLASH program buffer: writes words one by one
 * @param[in] p_ulAddress - the first address, a multiple of 4
 * @param[in] p_pulData - the words to be written
 * @param[in] p_unSize - the number of words
 * @return    Same as HAL_FLASH_Program; the operation stops at the first error
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_ProgramBuff(uint32_t p_ulAddress, const uint32_t *p_pulData, uint16_t p_unSize)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_FLASH_Check(p_ulAddress, 4UL * p_unSize, 4);
	uint16_t l_unIndex;

	if ((l_eReturnCode == HAL_SUCCESS) && (p_pulData == NULL) && (p_unSize != 0U))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	for (l_unIndex = 0; (l_unIndex < p_unSize) && (l_eReturnCode == HAL_SUCCESS); l_unIndex++)
	{
		l_eReturnCode = HAL_FLASH_Program(p_ulAddress + (4UL * l_unIndex), p_pulData[l_unIndex]);
	}

	return l_eReturnCode;
}

/**************************************************************************************************
 * @brief     FLASH fast program: writes half a page (32 words) in one operation
 * @param[in] p_ulAddress - the first address, a multiple of HAL_FLASH_HALF_PAGE_SIZE
 * @param[in] p_pulData - the 32 words to be written; the buffer must be in RAM
 * @return    Same as HAL_FLASH_PageErase
 * @remarks   Interrupts are masked for the duration of the write (a few milliseconds): the flash
 *            memory cannot be read meanwhile, so no interrupt handler could run.
 **************************************************************************************************/
HAL_RETURN_CODE_T HAL_FLASH_FastProgram(uint32_t p_ulAddress, const uint32_t *p_pulData)
{
	HAL_RETURN_CODE_T l_eReturnCode = HAL_FLASH_Check(p_ulAddress, HAL_FLASH_HALF_PAGE_SIZE, HAL_FLASH_HALF_PAGE_SIZE);
	uint32_t l_ulStatus;
	uint32_t l_ulState;

	if ((l_eReturnCode == HAL_SUCCESS) && ((p_pulData == NULL) || ((uint32_t) p_pulData < SRAM_BASE)))
	{
		l_eReturnCode = HAL_ERROR__PARAMETER;
	}

	if (l_eReturnCode == HAL_SUCCESS)
	{
		l_ulState = HAL_PRIVATE_EnterCritical();

		l_ulStatus = HAL_FLASH_WriteHalfPage((volatile uint32_t *) p_ulAddress, p_pulData);

		HAL_PRIVATE_ExitCritical(l_ulState);

		if (l_ulStatus & FLASH_SR_BSY)
		{
			l_eReturnCode = HAL_ERROR__TIMEOUT;
		}
		else if (l_ulStatus & HAL_FLASH_ERROR_FLAGS)
		{
			l_eReturnCode = HAL_ERROR__BUS;
		}
		else
		{
			/* The half page was written. */
		}

		FLASH->SR = HAL_FLASH_ERROR_FLAGS | FLASH_SR_EOP;
	}

	return l_eReturnCode;
}
