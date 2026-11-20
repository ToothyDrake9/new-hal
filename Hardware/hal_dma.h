/**************************************************************************************************
 * @file    hal_dma.h
 * @brief   Microprocessor DMA module
 * @author  Andustria
 * @date    November 2026
 * @changes Rewritten: callbacks are given at configuration, the addresses and the length are
 *          given when a transfer starts
 **************************************************************************************************/
#ifndef HAL_DMA_H_
#define HAL_DMA_H_

#include "typedefs.h"
#include "hal_port.h"

enum HAL_DMA_Channel
{
	HAL_DMA_CHANNEL_0 = ((uint8_t) 0x00), // Channel 1 in the reference manual
	HAL_DMA_CHANNEL_1,
	HAL_DMA_CHANNEL_2,
	HAL_DMA_CHANNEL_3,
	HAL_DMA_CHANNEL_4,
	HAL_DMA_CHANNEL_5,
	HAL_DMA_CHANNEL_6
};

enum HAL_DMA_Direction
{
	HAL_DMA_DIRECTION__PERIPHERAL_TO_MEMORY = ((uint8_t) 0x00),
	HAL_DMA_DIRECTION__MEMORY_TO_PERIPHERAL
};

enum HAL_DMA_Size
{
	HAL_DMA_SIZE__8 = ((uint8_t) 0x00),
	HAL_DMA_SIZE__16,
	HAL_DMA_SIZE__32
};

enum HAL_DMA_Priority
{
	HAL_DMA_PRIORITY__LOW = ((uint8_t) 0x00),
	HAL_DMA_PRIORITY__MEDIUM,
	HAL_DMA_PRIORITY__HIGH,
	HAL_DMA_PRIORITY__VERY_HIGH
};

enum HAL_DMA_Interrupts
{
	HAL_DMA_COMPLETE = ((uint8_t) 0x00), // The whole buffer was transferred
	HAL_DMA_HALF                         // Half of the buffer was transferred
};

/* Runs in interrupt context. */
typedef void (*HAL_DMA_CALLBACK) (void *p_pvContext);

typedef struct
{
	uint8_t m_ucDirection : 1;            // e.g. HAL_DMA_DIRECTION__PERIPHERAL_TO_MEMORY
	uint8_t m_ucCircular : 1;             // HAL_ENABLE - the transfer restarts by itself
	uint8_t m_ucPeripheralIncrement : 1;  // HAL_ENABLE - the peripheral address advances
	uint8_t m_ucMemoryIncrement : 1;      // HAL_ENABLE - the memory address advances
	uint8_t m_ucPeripheralSize : 2;       // e.g. HAL_DMA_SIZE__8
	uint8_t m_ucMemorySize : 2;           // e.g. HAL_DMA_SIZE__8
	uint8_t m_ucPriority : 2;             // e.g. HAL_DMA_PRIORITY__LOW
	HAL_Interrupt m_stInterrupt;          // Priority of the channel interrupt
	HAL_DMA_CALLBACK m_pfCompleteCallback;  // NULL - not used
	HAL_DMA_CALLBACK m_pfHalfCallback;      // NULL - not used
	HAL_DMA_CALLBACK m_pfErrorCallback;     // NULL - not used; when given, its interrupt is always on
	void *m_pvContext;                    // Given back to the callbacks
}HAL_DMA_Flags;

HAL_Return_Code HAL_DMA_Config(uint8_t p_ucModule, uint8_t p_ucChannel, const HAL_DMA_Flags *p_pstFlags);
HAL_Return_Code HAL_DMA_Start(uint8_t p_ucModule, uint8_t p_ucChannel, uint32_t p_ulPeripheralAddress, uint32_t p_ulMemoryAddress, uint16_t p_unCount);
HAL_Return_Code HAL_DMA_Stop(uint8_t p_ucModule, uint8_t p_ucChannel);
HAL_Return_Code HAL_DMA_SetInterrupt(uint8_t p_ucModule, uint8_t p_ucChannel, uint8_t p_ucInterrupt, uint8_t p_ucState);
HAL_Return_Code HAL_DMA_GetRemaining(uint8_t p_ucModule, uint8_t p_ucChannel, uint16_t *p_punCount);

#endif /* HAL_DMA_H_ */
