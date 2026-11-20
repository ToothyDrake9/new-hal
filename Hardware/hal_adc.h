/**************************************************************************************************
 * @file    hal_adc.h
 * @brief   Microprocessor ADC module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: status returns and timeouts, the conversion callback is given at
 *          configuration, the oscillator the converter needs is kept alive across clock changes
 **************************************************************************************************/
#ifndef HAL_ADC_H_
#define HAL_ADC_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

enum HAL_ADC_Resolution
{
	HAL_ADC_RESOLUTION__12_BITS = ((uint8_t) 0x00),
	HAL_ADC_RESOLUTION__10_BITS,
	HAL_ADC_RESOLUTION__8_BITS,
	HAL_ADC_RESOLUTION__6_BITS
};

enum HAL_ADC_Align
{
	HAL_ADC_ALIGN__RIGHT = ((uint8_t) 0x00),
	HAL_ADC_ALIGN__LEFT
};

/* Sampling time in ADC clock cycles. A high impedance source or an internal channel needs a long one. */
enum HAL_ADC_SampleTime
{
	HAL_ADC_SAMPLE_TIME__4 = ((uint8_t) 0x00),
	HAL_ADC_SAMPLE_TIME__9,
	HAL_ADC_SAMPLE_TIME__16,
	HAL_ADC_SAMPLE_TIME__24,
	HAL_ADC_SAMPLE_TIME__48,
	HAL_ADC_SAMPLE_TIME__96,
	HAL_ADC_SAMPLE_TIME__192,
	HAL_ADC_SAMPLE_TIME__384
};

/* Timeout of one conversion, in milliseconds. */
#ifndef HAL_ADC_TIMEOUT_MS
#define HAL_ADC_TIMEOUT_MS 10U
#endif

/* Runs in interrupt context. */
typedef void (*HAL_ADC_CALLBACK) (void *p_pvContext, uint16_t p_unValue);

typedef struct
{
	uint8_t m_ucResolution : 2;           // e.g. HAL_ADC_RESOLUTION__12_BITS
	uint8_t m_ucAlign : 1;                // e.g. HAL_ADC_ALIGN__RIGHT
	HAL_Interrupt m_stInterrupt;          // Priority of the ADC interrupt
	HAL_ADC_CALLBACK m_pfConversionCallback; // NULL - conversions are read with HAL_ADC_ReadChannelRaw only
	void *m_pvContext;                    // Given back to the callback
}HAL_ADC_Flags;

typedef struct
{
	uint8_t m_ucSampleTime;               // e.g. HAL_ADC_SAMPLE_TIME__48
	HAL_GPIO_Pin m_stPin;                 // The analog input pin; module HAL_GPIO_NONE for an internal channel
}HAL_ADC_ChannelFlags;

HAL_Return_Code HAL_ADC_Config(uint8_t p_ucModule, const HAL_ADC_Flags *p_pstFlags);
HAL_Return_Code HAL_ADC_ConfigChannel(uint8_t p_ucModule, uint8_t p_ucChannel, const HAL_ADC_ChannelFlags *p_pstFlags);
HAL_Return_Code HAL_ADC_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_Return_Code HAL_ADC_ReadChannelRaw(uint8_t p_ucModule, uint8_t p_ucChannel, uint16_t *p_punValue);
HAL_Return_Code HAL_ADC_StartConversion(uint8_t p_ucModule, uint8_t p_ucChannel);

#endif /* HAL_ADC_H_ */
