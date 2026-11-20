/**************************************************************************************************
 * @file    hal_unsupported.c
 * @brief   STM32L1 family: modules that the STM32L15x does not have
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks The STM32L15x has no random number generator and no AES unit (the AES unit exists on
 *          the STM32L16x only). The functions exist so that portable code links, and they all
 *          fail: a caller can never mistake a missing module for a working one. Check
 *          HAL_HAS_RNG and HAL_HAS_AES at compile time to provide a software replacement.
 **************************************************************************************************/
#include "hal_rng.h"
#include "hal_aes.h"

/**************************************************************************************************
 * @brief   RNG configuration
 * @return  HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_RNG_Config(void)
{
	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief      RNG read
 * @param[out] p_pulValue - not written
 * @return     HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_RNG_Read(uint32_t *p_pulValue)
{
	(void) p_pulValue;

	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief     AES configuration
 * @param[in] p_ucMode - the AES mode (e.g. HAL_AES_MODE__ECB)
 * @return    HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_AES_Config(uint8_t p_ucMode)
{
	(void) p_ucMode;

	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief     AES set key
 * @param[in] p_pucKey - the key
 * @return    HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_AES_SetKey(const uint8_t *p_pucKey)
{
	(void) p_pucKey;

	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief      AES encrypt
 * @param[in]  p_pucData - the block to encrypt
 * @param[out] p_pucResult - not written
 * @return     HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_AES_Encrypt(const uint8_t *p_pucData, uint8_t *p_pucResult)
{
	(void) p_pucData;
	(void) p_pucResult;

	return HAL_ERROR__NOT_SUPPORTED;
}

/**************************************************************************************************
 * @brief      AES decrypt
 * @param[in]  p_pucData - the block to decrypt
 * @param[out] p_pucResult - not written
 * @return     HAL_ERROR__NOT_SUPPORTED
 **************************************************************************************************/
HAL_Return_Code HAL_AES_Decrypt(const uint8_t *p_pucData, uint8_t *p_pucResult)
{
	(void) p_pucData;
	(void) p_pucResult;

	return HAL_ERROR__NOT_SUPPORTED;
}
