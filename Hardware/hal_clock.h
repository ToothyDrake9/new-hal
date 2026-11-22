/**************************************************************************************************
 * @file    hal_clock.h
 * @brief   Microprocessor clock module: clock tree, time base, delays and timeouts
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the clock tree is described by the application, every wait has a timeout,
 *          bus frequencies are reported, a 1 ms time base replaces the one shot SysTick flag
 **************************************************************************************************/
#ifndef HAL_CLOCK_H_
#define HAL_CLOCK_H_

#include "typedefs.h"
#include "hal_port.h"

enum HAL_CLOCK_Source
{
	HAL_CLOCK_SOURCE__MSI = ((uint8_t) 0x00),
	HAL_CLOCK_SOURCE__HSI,
	HAL_CLOCK_SOURCE__HSE,
	HAL_CLOCK_SOURCE__PLL
};

enum HAL_CLOCK_Oscillator
{
	HAL_CLOCK_OSCILLATOR__OFF = ((uint8_t) 0x00),
	HAL_CLOCK_OSCILLATOR__CRYSTAL,
	HAL_CLOCK_OSCILLATOR__BYPASS   // External clock signal (e.g. the 8 MHz of the ST-LINK on a Nucleo)
};

/* Core voltage. Range 1 allows 32 MHz, range 2 allows 16 MHz, range 3 allows 4.2 MHz. */
enum HAL_CLOCK_Voltage
{
	HAL_CLOCK_VOLTAGE__RANGE_1 = ((uint8_t) 0x00),
	HAL_CLOCK_VOLTAGE__RANGE_2,
	HAL_CLOCK_VOLTAGE__RANGE_3
};

enum HAL_CLOCK_MSI
{
	HAL_CLOCK_MSI__65_KHZ = ((uint8_t) 0x00),
	HAL_CLOCK_MSI__131_KHZ,
	HAL_CLOCK_MSI__262_KHZ,
	HAL_CLOCK_MSI__524_KHZ,
	HAL_CLOCK_MSI__1048_KHZ,
	HAL_CLOCK_MSI__2097_KHZ,
	HAL_CLOCK_MSI__4194_KHZ
};

enum HAL_CLOCK_PLL_Multiplier
{
	HAL_CLOCK_PLL_MULTIPLIER__3 = ((uint8_t) 0x00),
	HAL_CLOCK_PLL_MULTIPLIER__4,
	HAL_CLOCK_PLL_MULTIPLIER__6,
	HAL_CLOCK_PLL_MULTIPLIER__8,
	HAL_CLOCK_PLL_MULTIPLIER__12,
	HAL_CLOCK_PLL_MULTIPLIER__16,
	HAL_CLOCK_PLL_MULTIPLIER__24,
	HAL_CLOCK_PLL_MULTIPLIER__32,
	HAL_CLOCK_PLL_MULTIPLIER__48
};

enum HAL_CLOCK_PLL_Divider
{
	HAL_CLOCK_PLL_DIVIDER__2 = ((uint8_t) 0x00),
	HAL_CLOCK_PLL_DIVIDER__3,
	HAL_CLOCK_PLL_DIVIDER__4
};

enum HAL_CLOCK_AHB_Prescaler
{
	HAL_CLOCK_AHB__1 = ((uint8_t) 0x00),
	HAL_CLOCK_AHB__2,
	HAL_CLOCK_AHB__4,
	HAL_CLOCK_AHB__8,
	HAL_CLOCK_AHB__16,
	HAL_CLOCK_AHB__64,
	HAL_CLOCK_AHB__128,
	HAL_CLOCK_AHB__256,
	HAL_CLOCK_AHB__512
};

enum HAL_CLOCK_APB_Prescaler
{
	HAL_CLOCK_APB__1 = ((uint8_t) 0x00),
	HAL_CLOCK_APB__2,
	HAL_CLOCK_APB__4,
	HAL_CLOCK_APB__8,
	HAL_CLOCK_APB__16
};

enum HAL_CLOCK_Bus
{
	HAL_CLOCK_BUS__AHB = ((uint8_t) 0x00),
	HAL_CLOCK_BUS__APB1,
	HAL_CLOCK_BUS__APB2
};

/* Start-up timeouts, in milliseconds. They can be overridden on the compiler command line. */
#ifndef HAL_CLOCK_HSE_TIMEOUT_MS
#define HAL_CLOCK_HSE_TIMEOUT_MS 100U
#endif
#ifndef HAL_CLOCK_LSE_TIMEOUT_MS
#define HAL_CLOCK_LSE_TIMEOUT_MS 5000U
#endif
#ifndef HAL_CLOCK_TIMEOUT_MS
#define HAL_CLOCK_TIMEOUT_MS 20U
#endif

/* Runs in interrupt context, once per millisecond. */
typedef void (*HAL_CLOCK_TICK_CALLBACK_T) (void *p_pvContext);

typedef struct
{
	uint8_t m_ucSource;           // System clock source (e.g. HAL_CLOCK_SOURCE__PLL)
	uint8_t m_ucVoltage;          // Core voltage range (e.g. HAL_CLOCK_VOLTAGE__RANGE_1)
	uint8_t m_ucMsiRange;         // Used when the source is MSI (e.g. HAL_CLOCK_MSI__2097_KHZ)
	uint8_t m_ucHse;              // HSE oscillator mode (e.g. HAL_CLOCK_OSCILLATOR__BYPASS)
	uint32_t m_ulHseFrequency;    // HSE frequency in Hz (e.g. 8000000)
	uint8_t m_ucPllSource;        // HAL_CLOCK_SOURCE__HSI or HAL_CLOCK_SOURCE__HSE
	uint8_t m_ucPllMultiplier;    // e.g. HAL_CLOCK_PLL_MULTIPLIER__8
	uint8_t m_ucPllDivider;       // e.g. HAL_CLOCK_PLL_DIVIDER__2
	uint8_t m_ucAhbPrescaler;     // e.g. HAL_CLOCK_AHB__1
	uint8_t m_ucApb1Prescaler;    // e.g. HAL_CLOCK_APB__1
	uint8_t m_ucApb2Prescaler;    // e.g. HAL_CLOCK_APB__1
	uint8_t m_ucLse;              // LSE oscillator mode (e.g. HAL_CLOCK_OSCILLATOR__CRYSTAL)
	HAL_INTERRUPT_T m_stInterrupt;  // Priority of the 1 ms tick
	HAL_CLOCK_TICK_CALLBACK_T m_pfTickCallback; // NULL - no callback
	void *m_pvContext;            // Given back to the callback
}HAL_CLOCK_FLAGS_T;

/* State of one timeout. Poll it with HAL_CLOCK_TimeoutExpired at least once per millisecond. */
typedef struct
{
	uint32_t m_ulRemaining;
	uint32_t m_ulLast;
}HAL_CLOCK_TIMEOUT_T;

HAL_RETURN_CODE_T HAL_CLOCK_Init(const HAL_CLOCK_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_CLOCK_Update(const HAL_CLOCK_FLAGS_T *p_pstFlags);
void HAL_CLOCK_GetFlags(HAL_CLOCK_FLAGS_T *p_pstFlags);

uint32_t HAL_CLOCK_Get(void);
uint32_t HAL_CLOCK_GetBus(uint8_t p_ucBus);
uint32_t HAL_CLOCK_GetTimer(uint8_t p_ucBus);

HAL_RETURN_CODE_T HAL_CLOCK_RequestHsi(void);
void HAL_CLOCK_ReleaseHsi(void);

uint32_t HAL_CLOCK_GetTick(void);
void HAL_CLOCK_SetTickState(uint8_t p_ucState);
void HAL_CLOCK_Delay(uint32_t p_ulMilliseconds);
void HAL_CLOCK_DelayUs(uint32_t p_ulMicroseconds);
void HAL_CLOCK_TimeoutStart(HAL_CLOCK_TIMEOUT_T *p_pstTimeout, uint32_t p_ulMilliseconds);
bool HAL_CLOCK_TimeoutExpired(HAL_CLOCK_TIMEOUT_T *p_pstTimeout);

#endif /* HAL_CLOCK_H_ */
