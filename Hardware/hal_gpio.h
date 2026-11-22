/**************************************************************************************************
 * @file    hal_gpio.h
 * @brief   Microprocessor GPIO module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: status returns, interrupt callback given at configuration, inline pin access
 **************************************************************************************************/
#ifndef HAL_GPIO_H_
#define HAL_GPIO_H_

#include "typedefs.h"
#include "hal_port.h"

enum HAL_GPIO_Pins
{
	HAL_GPIO_PIN_0 = ((uint8_t) 0x00),
	HAL_GPIO_PIN_1,
	HAL_GPIO_PIN_2,
	HAL_GPIO_PIN_3,
	HAL_GPIO_PIN_4,
	HAL_GPIO_PIN_5,
	HAL_GPIO_PIN_6,
	HAL_GPIO_PIN_7,
	HAL_GPIO_PIN_8,
	HAL_GPIO_PIN_9,
	HAL_GPIO_PIN_10,
	HAL_GPIO_PIN_11,
	HAL_GPIO_PIN_12,
	HAL_GPIO_PIN_13,
	HAL_GPIO_PIN_14,
	HAL_GPIO_PIN_15,

	HAL_GPIO_PIN_NO
};

/* Marks an optional pin of a module (e.g. the MISO pin of a transmit only SPI) as not used. */
#define HAL_GPIO_NONE ((uint8_t) 0xFF)

enum HAL_GPIO_Mode
{
	HAL_GPIO_MODE__INPUT = ((uint8_t) 0x00),
	HAL_GPIO_MODE__OUTPUT,
	HAL_GPIO_MODE__FUNCTION,
	HAL_GPIO_MODE__ANALOG
};

enum HAL_GPIO_Type
{
	HAL_GPIO_TYPE__PUSH_PULL = ((uint8_t) 0x00),
	HAL_GPIO_TYPE__OPEN_DRAIN
};

enum HAL_GPIO_Pull
{
	HAL_GPIO_PULL__NO_PULL = ((uint8_t) 0x00),
	HAL_GPIO_PULL__PULL_UP,
	HAL_GPIO_PULL__PULL_DOWN
};

enum HAL_GPIO_Speed
{
	HAL_GPIO_SPEED__LOW = ((uint8_t) 0x00),
	HAL_GPIO_SPEED__MEDIUM,
	HAL_GPIO_SPEED__HIGH,
	HAL_GPIO_SPEED__VERY_HIGH
};

enum HAL_GPIO_Function
{
	HAL_GPIO_FUNCTION__0 = ((uint8_t) 0x00),
	HAL_GPIO_FUNCTION__1,
	HAL_GPIO_FUNCTION__2,
	HAL_GPIO_FUNCTION__3,
	HAL_GPIO_FUNCTION__4,
	HAL_GPIO_FUNCTION__5,
	HAL_GPIO_FUNCTION__6,
	HAL_GPIO_FUNCTION__7,
	HAL_GPIO_FUNCTION__8,
	HAL_GPIO_FUNCTION__9,
	HAL_GPIO_FUNCTION__10,
	HAL_GPIO_FUNCTION__11,
	HAL_GPIO_FUNCTION__12,
	HAL_GPIO_FUNCTION__13,
	HAL_GPIO_FUNCTION__14,
	HAL_GPIO_FUNCTION__15
};

enum HAL_GPIO_State
{
	HAL_GPIO_LOW = ((uint8_t) 0x00),
	HAL_GPIO_HIGH,
	HAL_GPIO_TOGGLE,
	HAL_GPIO_PULSE
};

enum HAL_GPIO_Edge
{
	HAL_GPIO_RISING = ((uint8_t) 0x01),
	HAL_GPIO_FALLING,
	HAL_GPIO_BOTH
};

typedef struct
{
	uint8_t m_ucMode : 2;
	uint8_t m_ucType : 1;
	uint8_t m_ucPull : 2;
	uint8_t m_ucSpeed : 2;
	uint8_t m_ucFunction : 4;
}HAL_GPIO_FLAGS_T;

/* A pin handed to another module (UART, SPI, ...): where it is and which function selects it. */
typedef struct
{
	uint8_t m_ucModule;   // e.g. HAL_GPIO_0, or HAL_GPIO_NONE when the pin is not used
	uint8_t m_ucPin;      // e.g. HAL_GPIO_PIN_2
	uint8_t m_ucFunction; // e.g. HAL_GPIO_FUNCTION__7
}HAL_GPIO_PIN_T;

/* Runs in interrupt context. */
typedef void (*HAL_GPIO_CALLBACK_T) (void *p_pvContext);

typedef struct
{
	uint8_t m_ucEdge;                // e.g. HAL_GPIO_FALLING
	HAL_INTERRUPT_T m_stInterrupt;     // Priority of the EXTI line, shared by pins 5-9 and by pins 10-15
	HAL_GPIO_CALLBACK_T m_pfCallback;  // Must not be NULL
	void *m_pvContext;               // Given back to the callback
}HAL_GPIO_INTERRUPT_FLAGS_T;

HAL_RETURN_CODE_T HAL_GPIO_Config(uint8_t p_ucModule, uint8_t p_ucPin, const HAL_GPIO_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_GPIO_ConfigPin(const HAL_GPIO_PIN_T *p_pstPin, const HAL_GPIO_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_GPIO_ConfigInterrupt(uint8_t p_ucModule, uint8_t p_ucPin, const HAL_GPIO_INTERRUPT_FLAGS_T *p_pstFlags);
HAL_RETURN_CODE_T HAL_GPIO_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucPin, uint8_t p_ucState);
HAL_RETURN_CODE_T HAL_GPIO_ReleaseInterrupt(uint8_t p_ucModule, uint8_t p_ucPin);

/*
 * Pin access. These are inline so that a call with constant arguments becomes a single store.
 * They do not check their arguments: p_ucModule and p_ucPin must be valid.
 */
static GPIO_TypeDef *const g_apstHAL_GPIO_Port[HAL_GPIO_NO] = HAL_GPIO_PORT_TABLE;

/**************************************************************************************************
 * @brief     Gets the registers of a GPIO module
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @return    The register block
 **************************************************************************************************/
static inline GPIO_TypeDef *HAL_GPIO_GetRegisters(uint8_t p_ucModule)
{
	return g_apstHAL_GPIO_Port[p_ucModule];
}

/**************************************************************************************************
 * @brief     GPIO read
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @return    HAL_GPIO_LOW or HAL_GPIO_HIGH
 **************************************************************************************************/
static inline uint8_t HAL_GPIO_Read(uint8_t p_ucModule, uint8_t p_ucPin)
{
	return (uint8_t) ((HAL_GPIO_GetRegisters(p_ucModule)->IDR >> p_ucPin) & 1U);
}

/**************************************************************************************************
 * @brief     GPIO write
 * @param[in] p_ucModule - the GPIO module (e.g. HAL_GPIO_0)
 * @param[in] p_ucPin - the GPIO pin (e.g. HAL_GPIO_PIN_0)
 * @param[in] p_ucState - the GPIO state (e.g. HAL_GPIO_HIGH)
 * @remarks   Only the set/reset register is written, so pins of the same port that are driven
 *            from an interrupt are never disturbed. HAL_GPIO_PULSE inverts the pin twice.
 **************************************************************************************************/
static inline void HAL_GPIO_Write(uint8_t p_ucModule, uint8_t p_ucPin, uint8_t p_ucState)
{
	GPIO_TypeDef *l_pstPort = HAL_GPIO_GetRegisters(p_ucModule);
	uint32_t l_ulSet = 1UL << p_ucPin;
	uint32_t l_ulReset = 1UL << (p_ucPin + 16U);

	switch (p_ucState)
	{
		case HAL_GPIO_LOW:
		{
			l_pstPort->BSRR = l_ulReset;

			break;
		}
		case HAL_GPIO_HIGH:
		{
			l_pstPort->BSRR = l_ulSet;

			break;
		}
		case HAL_GPIO_TOGGLE:
		{
			l_pstPort->BSRR = (l_pstPort->ODR & l_ulSet) ? l_ulReset : l_ulSet;

			break;
		}
		case HAL_GPIO_PULSE:
		{
			if (l_pstPort->ODR & l_ulSet)
			{
				l_pstPort->BSRR = l_ulReset;
				l_pstPort->BSRR = l_ulSet;
			}
			else
			{
				l_pstPort->BSRR = l_ulSet;
				l_pstPort->BSRR = l_ulReset;
			}

			break;
		}
		default: break;
	}
}

#endif /* HAL_GPIO_H_ */
