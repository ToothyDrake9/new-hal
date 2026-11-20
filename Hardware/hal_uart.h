/**************************************************************************************************
 * @file    hal_uart.h
 * @brief   Microprocessor UART module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: pins and callbacks are given at configuration, the baud rate comes from
 *          the bus clock and is rounded, errors are reported, every wait has a timeout
 **************************************************************************************************/
#ifndef HAL_UART_H_
#define HAL_UART_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

/* Number of data bits, the parity bit not included. */
enum HAL_UART_DataBits
{
	HAL_UART_DATABITS__8 = ((uint8_t) 0x00),
	HAL_UART_DATABITS__7,   // Needs a parity bit
	HAL_UART_DATABITS__9    // Without parity; only the low 8 bits pass through this module
};

enum HAL_UART_Parity
{
	HAL_UART_PARITY__NONE = ((uint8_t) 0x00),
	HAL_UART_PARITY__ODD,
	HAL_UART_PARITY__EVEN
};

enum HAL_UART_StopBits
{
	HAL_UART_STOPBITS__1 = ((uint8_t) 0x00),
	HAL_UART_STOPBITS__2
};

enum HAL_UART_Interrupts
{
	HAL_UART_RX = ((uint8_t) 0x00),  // A byte was received (errors are reported with it)
	HAL_UART_TX,                     // The module can take the next byte
	HAL_UART_TX_COMPLETE             // The last bit left the pin (e.g. to release an RS-485 driver)
};

/* Bits of the error callback argument. */
#define HAL_UART_ERROR__OVERRUN ((uint8_t) 0x01)
#define HAL_UART_ERROR__FRAMING ((uint8_t) 0x02)
#define HAL_UART_ERROR__NOISE   ((uint8_t) 0x04)
#define HAL_UART_ERROR__PARITY  ((uint8_t) 0x08)

/* Return values of the transmit callback. */
enum HAL_UART_TxAction
{
	HAL_UART_TX__DONE = ((uint8_t) 0x00), // Nothing left: the module disables the HAL_UART_TX interrupt
	HAL_UART_TX__SEND                     // Send the byte stored by the callback
};

/* Timeout of the blocking functions, per byte, in milliseconds. */
#ifndef HAL_UART_TIMEOUT_MS
#define HAL_UART_TIMEOUT_MS 100U
#endif

/* The callbacks run in interrupt context. */
typedef void (*HAL_UART_RX_CALLBACK) (void *p_pvContext, uint8_t p_ucByte);
typedef uint8_t (*HAL_UART_TX_CALLBACK) (void *p_pvContext, uint8_t *p_pucByte);
typedef void (*HAL_UART_EVENT_CALLBACK) (void *p_pvContext);
typedef void (*HAL_UART_ERROR_CALLBACK) (void *p_pvContext, uint8_t p_ucErrors);

typedef struct
{
	uint32_t m_ulBaud;                 // e.g. 115200
	uint8_t m_ucDataBits : 2;          // e.g. HAL_UART_DATABITS__8
	uint8_t m_ucParity : 2;            // e.g. HAL_UART_PARITY__NONE
	uint8_t m_ucStopBits : 1;          // e.g. HAL_UART_STOPBITS__1
	uint8_t m_ucIrDA : 1;              // HAL_ENABLE - IrDA encoding
	HAL_GPIO_Pin m_stTxPin;            // Module HAL_GPIO_NONE - no transmitter
	HAL_GPIO_Pin m_stRxPin;            // Module HAL_GPIO_NONE - no receiver
	HAL_Interrupt m_stInterrupt;       // Priority of the UART interrupt
	HAL_UART_RX_CALLBACK m_pfRxCallback;                  // NULL - received bytes are polled
	HAL_UART_TX_CALLBACK m_pfTxCallback;                  // NULL - no interrupt driven transmission
	HAL_UART_EVENT_CALLBACK m_pfTxCompleteCallback;       // NULL - not used
	HAL_UART_ERROR_CALLBACK m_pfErrorCallback;            // NULL - errors are dropped
	void *m_pvContext;                 // Given back to the callbacks
}HAL_UART_Flags;

HAL_Return_Code HAL_UART_Config(uint8_t p_ucModule, const HAL_UART_Flags *p_pstFlags);
HAL_Return_Code HAL_UART_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_Return_Code HAL_UART_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState);
HAL_Return_Code HAL_UART_GetBaud(uint8_t p_ucModule, uint32_t *p_pulBaud);
HAL_Return_Code HAL_UART_ReadByte(uint8_t p_ucModule, uint8_t *p_pucByte, uint32_t p_ulTimeoutMs);
HAL_Return_Code HAL_UART_WriteByte(uint8_t p_ucModule, uint8_t p_ucByte);
HAL_Return_Code HAL_UART_WriteBuff(uint8_t p_ucModule, const uint8_t *p_pucBuff, uint16_t p_unSize);
HAL_Return_Code HAL_UART_Flush(uint8_t p_ucModule);

#endif /* HAL_UART_H_ */
