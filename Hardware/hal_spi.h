/**************************************************************************************************
 * @file    hal_spi.h
 * @brief   Microprocessor SPI module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: the speed is asked in Hz and derived from the bus clock, the chip select is
 *          left to the caller (one bus can serve several devices), transfers return the received
 *          data, every wait has a timeout
 **************************************************************************************************/
#ifndef HAL_SPI_H_
#define HAL_SPI_H_

#include "typedefs.h"
#include "hal_port.h"
#include "hal_gpio.h"

enum HAL_SPI_Mode
{
	HAL_SPI_MODE__MASTER = ((uint8_t) 0x00),
	HAL_SPI_MODE__SLAVE
};

enum HAL_SPI_Polarity
{
	HAL_SPI_POLARITY__LOW = ((uint8_t) 0x00),  // The clock idles low
	HAL_SPI_POLARITY__HIGH                     // The clock idles high
};

enum HAL_SPI_Phase
{
	HAL_SPI_PHASE__FIRST_EDGE = ((uint8_t) 0x00), // Data is sampled on the first clock edge
	HAL_SPI_PHASE__SECOND_EDGE                    // Data is sampled on the second clock edge
};

enum HAL_SPI_BitOrder
{
	HAL_SPI_BIT_ORDER__MSB_FIRST = ((uint8_t) 0x00),
	HAL_SPI_BIT_ORDER__LSB_FIRST
};

enum HAL_SPI_Interrupts
{
	HAL_SPI_RX = ((uint8_t) 0x00),  // A byte was received
	HAL_SPI_TX                      // The module can take the next byte
};

/* Bits of the error callback argument. */
#define HAL_SPI_ERROR__OVERRUN    ((uint8_t) 0x01)
#define HAL_SPI_ERROR__MODE_FAULT ((uint8_t) 0x02)

/* Return values of the transmit callback. */
enum HAL_SPI_TxAction
{
	HAL_SPI_TX__DONE = ((uint8_t) 0x00), // Nothing left: the module disables the HAL_SPI_TX interrupt
	HAL_SPI_TX__SEND                     // Send the byte stored by the callback
};

/* Timeout of the blocking functions, per byte, in milliseconds. */
#ifndef HAL_SPI_TIMEOUT_MS
#define HAL_SPI_TIMEOUT_MS 10U
#endif

/* The byte clocked out while only reading. */
#ifndef HAL_SPI_FILL_BYTE
#define HAL_SPI_FILL_BYTE 0xFFU
#endif

/* The callbacks run in interrupt context. They are meant for the slave mode. */
typedef void (*HAL_SPI_RX_CALLBACK) (void *p_pvContext, uint8_t p_ucByte);
typedef uint8_t (*HAL_SPI_TX_CALLBACK) (void *p_pvContext, uint8_t *p_pucByte);
typedef void (*HAL_SPI_ERROR_CALLBACK) (void *p_pvContext, uint8_t p_ucErrors);

typedef struct
{
	uint8_t m_ucMode : 1;              // e.g. HAL_SPI_MODE__MASTER
	uint8_t m_ucPolarity : 1;          // e.g. HAL_SPI_POLARITY__LOW
	uint8_t m_ucPhase : 1;             // e.g. HAL_SPI_PHASE__FIRST_EDGE
	uint8_t m_ucBitOrder : 1;          // e.g. HAL_SPI_BIT_ORDER__MSB_FIRST
	uint32_t m_ulSpeed;                // Master: the highest acceptable clock in Hz (e.g. 8000000)
	HAL_GPIO_Pin m_stClkPin;
	HAL_GPIO_Pin m_stMisoPin;          // Module HAL_GPIO_NONE when not used
	HAL_GPIO_Pin m_stMosiPin;          // Module HAL_GPIO_NONE when not used
	HAL_GPIO_Pin m_stSsPin;            // Slave only: hardware select input; HAL_GPIO_NONE - always selected
	HAL_Interrupt m_stInterrupt;       // Priority of the SPI interrupt
	HAL_SPI_RX_CALLBACK m_pfRxCallback;        // NULL - not used
	HAL_SPI_TX_CALLBACK m_pfTxCallback;        // NULL - not used
	HAL_SPI_ERROR_CALLBACK m_pfErrorCallback;  // NULL - errors are dropped
	void *m_pvContext;                 // Given back to the callbacks
}HAL_SPI_Flags;

HAL_Return_Code HAL_SPI_Config(uint8_t p_ucModule, const HAL_SPI_Flags *p_pstFlags);
HAL_Return_Code HAL_SPI_SetState(uint8_t p_ucModule, uint8_t p_ucState);
HAL_Return_Code HAL_SPI_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucInterrupt, uint8_t p_ucState);
HAL_Return_Code HAL_SPI_GetSpeed(uint8_t p_ucModule, uint32_t *p_pulSpeed);
HAL_Return_Code HAL_SPI_TransferByte(uint8_t p_ucModule, uint8_t p_ucByte, uint8_t *p_pucByte);
HAL_Return_Code HAL_SPI_Transfer(uint8_t p_ucModule, const uint8_t *p_pucTxBuff, uint8_t *p_pucRxBuff, uint16_t p_unSize);
HAL_Return_Code HAL_SPI_WriteBuff(uint8_t p_ucModule, const uint8_t *p_pucBuff, uint16_t p_unSize);
HAL_Return_Code HAL_SPI_ReadBuff(uint8_t p_ucModule, uint8_t *p_pucBuff, uint16_t p_unSize);

#endif /* HAL_SPI_H_ */
