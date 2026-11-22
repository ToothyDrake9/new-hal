# new-hal

A hardware abstraction layer for STM32 microcontrollers. CMSIS only, bare metal, direct register
access. The first target is the STM32L152RE (NUCLEO-L152RE).

It replaces the HAL that was copied by hand into each project (`Core/Hardware/hal_*.c`). The
naming conventions of that HAL are kept: `HAL_MODULE_Function`, `p_`/`l_`/`g_`/`m_` scope
prefixes, `uc`/`un`/`ul`/`st`/`pf` type prefixes, `HAL_X__VALUE` constants. Every type defined
with `typedef` (structures, enumerations, function pointers) is written in capitals with a `_T`
suffix: `HAL_UART_FLAGS_T`, `HAL_RETURN_CODE_T`, `HAL_UART_RX_CALLBACK_T`.

## Status

| What | State |
|---|---|
| STM32L1 family (STM32L152RE) | All modules written |
| Builds with `arm-none-eabi-gcc` 13.3.1 | Clean with `-Wall -Wextra -Wconversion -Werror` at `-O0`, `-Og`, `-O1`, `-O2`, `-O3`, `-Os` |
| Calculation tests on PC | Pass (`Tests/host/test_calc.c`) |
| Register level tests on PC | 341 checks pass (`Tests/host/sim`) |
| **Run on a real board** | **Not done yet.** Nothing here has been flashed. See "To verify on hardware". |
| STM32L4, F7, H7 | Not ported. The structure is ready for them, see "Porting to another family". |

Size of the example (clock, GPIO, EXTI, UART echo, timer, sleep) at `-Os`: 7.6 KB of flash,
0.7 KB of RAM. With every module linked: 16.1 KB of flash, 1.2 KB of RAM.

## Layout

```
Globals/typedefs.h                 Return codes and common types. No device header.
Hardware/hal_*.h                   The API. Family independent, no register in sight.
Hardware/hal_calc.h                Divider and timing calculations, testable on a PC.
Hardware/Drivers/                  One driver per peripheral version, shared between families.
    hal_gpio_v2.c                    GPIO with MODER/AFR registers
    hal_uart_v1.c                    USART with SR/DR registers
    hal_spi_v1.c  hal_i2c_v1.c  hal_timer_v1.c  hal_dma_v1.c  hal_dac_v1.c
    hal_private.h                    Shared by the HAL sources only
Hardware/Family/STM32L1/           What is specific to one family.
    hal_port.h                       Device header and module instances, visible to the application
    hal_family.h                     Family differences and limits, HAL sources only
    hal_family_irq.h                 Interrupt vectors of each driver
    hal_devices.c                    Where each instance is: registers, clock gate, IRQ
    hal_clock.c hal_adc.c hal_flash.c hal_crc.c hal_lpm.c hal_unsupported.c
Platform/STM32L152RE/              CMSIS headers, startup file, linker script.
Board/NUCLEO_L152RE/board.h        Pins and clock of one board.
Example/main.c                     A small application.
Tests/host/                        Tests that run on the PC.
Tests/link/link_all.c              Build check that calls every public function.
```

## Building

```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build
```

This builds `libhal.a`, `example.elf` (with `.hex`, `.bin` and `.map`) and `link_all.elf`.
`-DCMAKE_BUILD_TYPE=Debug` selects `-Og -g3`; the default is Release (`-Os`). Set
`ARM_TOOLCHAIN_DIR` when the compiler is not on the `PATH`.

The PC tests use the compiler of the development machine:

```
cmake -S Tests/host -B build-host
cmake --build build-host
ctest --test-dir build-host
```

In STM32CubeIDE: add `Globals`, `Hardware`, `Platform/STM32L152RE` and one `Board` folder to a
project, define `STM32L152xE`, add the include paths listed in `CMakeLists.txt`, and exclude
`Tests` and the CubeIDE generated startup file from the build.

## Using it

```c
#include "board.h"

static void APP_RxCallback(void *p_pvContext, uint8_t p_ucByte)
{
	/* Interrupt context. The HAL has already read the data register. */
}

int main(void)
{
	static const HAL_CLOCK_FLAGS_T l_stClock = BOARD_CLOCK_FLAGS;
	static const HAL_UART_FLAGS_T l_stUart =
	{
		.m_ulBaud = 115200,
		.m_stTxPin = BOARD_VCP_TX_PIN,
		.m_stRxPin = BOARD_VCP_RX_PIN,
		.m_stInterrupt = { .m_ucPrePriority = 8 },
		.m_pfRxCallback = APP_RxCallback
	};

	if (HAL_CLOCK_Init(&l_stClock) != HAL_SUCCESS)
	{
		/* The system runs from the MSI at 2.097 MHz and can still report the problem. */
	}

	(void) HAL_UART_Config(BOARD_VCP_UART, &l_stUart);
	(void) HAL_UART_SetState(BOARD_VCP_UART, HAL_ENABLE);
	(void) HAL_UART_SetInterrupt(BOARD_VCP_UART, HAL_UART_RX, HAL_ENABLE);

	for (;;)
	{
		(void) HAL_LPM_Enter(HAL_LPM_MODE__SLEEP, NULL);
	}
}
```

`Example/main.c` is the complete version, with a clock fallback, a button and a timer.

## Rules the code follows

1. **Every function that can fail returns a `HAL_RETURN_CODE_T`.** `HAL_SUCCESS` is 0. The
   exceptions are the inline pin functions and `HAL_TIMER_GetCounter`, which are used in timing
   critical code and do not check their arguments.
2. **Every wait has a timeout.** Timeouts are measured on the SysTick counter itself, so they work
   with interrupts masked and inside interrupt handlers. `HAL_CLOCK_Init` must be called first:
   before it, a wait fails at once instead of hanging.
3. **Callbacks are given in the configuration structure**, with a context pointer. A `NULL`
   callback means the interrupt source is not used. No callback is mandatory.
4. **Pins, alternate functions and clock sources come from the application** (`board.h`). The
   HAL contains no pin number and no crystal frequency.
5. **A configuration function writes whole registers or whole fields.** A module can be
   configured again at any time and ends in the state the structure describes.
6. **Flags are cleared one at a time**, in the way each register requires: write 1 for the EXTI,
   DMA and flash registers; write 0 to the flag and 1 to all others for the timer, USART and ADC
   status registers; read sequences for the USART, SPI and I2C. Never a read-modify-write of a
   status register.
7. **One interrupt source can be disabled without touching the others.** The NVIC line of a
   module is enabled when it is configured with a callback.
8. **Registers shared with interrupt handlers are modified with interrupts masked.**
9. **The CMSIS device header is the only definition of the hardware.** Registers, addresses, bit
   names and field values are used by their device header names. The HAL defines a name of its
   own only for what the device header does not have (the flash unlock keys, the flash page
   size, the oscillator and clock limits) and for what the application chooses (module
   numbers, settings). `hal_port.h` includes the device header, so the application sees it too.
10. **Tables are `const`.** The RAM a module needs is its callbacks and a few status bytes.

## What changed for code written for the old HAL

| Old | New |
|---|---|
| `HAL_ERROR` is 0, `HAL_SUCCESS` is 1 | `HAL_SUCCESS` is 0. `if (HAL_xxx())` now means "if it failed". |
| `HAL_xxx_Config(module, ..., flags)` by value | `HAL_xxx_Config(module, &flags)` by `const` pointer |
| Type names such as `HAL_UART_Flags`, `HAL_Return_Code` | `HAL_UART_FLAGS_T`, `HAL_RETURN_CODE_T`: capitals and `_T` for every `typedef` |
| 71 mandatory `HAL_xxx_n_Callback` functions | Callbacks in the flags. Delete the empty ones. |
| `HAL_UART_n_RxCallback` then `HAL_UART_ReadByte` | The callback receives the byte. |
| `HAL_UART_n_TxCallback` | The callback returns the next byte, or `HAL_UART_TX__DONE`. |
| Pin `#define`s in `hal_uart.h`, `hal_spi.h`, ... | `m_stTxPin`, `m_stClkPin`, ... in the flags, from `board.h` |
| `HAL_CLOCK_Init(void)`, fixed 32 MHz from HSE bypass | `HAL_CLOCK_Init(&flags)` |
| `HAL_CLOCK_Update(source)` | `HAL_CLOCK_Update(&flags)` |
| `HAL_CLOCK_Get()` for baud rates and periods | `HAL_CLOCK_GetBus()`, `HAL_CLOCK_GetTimer()` |
| `HAL_CLOCK_SysTick_Enable` and `g_ucSysTick_Update` | `HAL_CLOCK_Delay`, `HAL_CLOCK_DelayUs`, `HAL_CLOCK_GetTick`, timeouts |
| `HAL_GPIO_Config` with interrupt priority, `HAL_GPIO_SetInterrupt(module, pin, edge, state)` | `HAL_GPIO_ConfigInterrupt(module, pin, &flags)`, then `HAL_GPIO_SetInterrupt(module, pin, state)` |
| `HAL_GPIO_Get/ClearInterruptFlag`, `HAL_xxx_Get/ClearInterruptFlag` | Gone. The handlers clear the flags. |
| `HAL_SPI_x_MHz` speed codes | `m_ulSpeed` in Hz; `HAL_SPI_GetSpeed` tells what was obtained |
| `HAL_SPI_WriteByte` | `HAL_SPI_TransferByte` |
| `HAL_SPI_WriteBuff` drives a fixed chip select | Drive the chip select around `HAL_SPI_Transfer` |
| `HAL_I2C_StartCondition`, `WriteByte`, `ReadByte`, `StopCondition` | `HAL_I2C_Write`, `HAL_I2C_Read`, `HAL_I2C_WriteRead` |
| `HAL_TIMER_Flags.m_stCompare` | `m_ulUpdate` in the flags, `HAL_TIMER_ConfigChannel` per channel |
| `HAL_LPM_n_Enter()`, `HAL_LPM_n_Exit()` and ten callbacks | `HAL_LPM_Enter(mode, &flags)`; the clock is restored by the module |
| `HAL_FLASH_PageErase` and others return 1 for success | Return codes; the memory must be unlocked with `HAL_FLASH_Unlock` |
| `HAL_RNG_Read()` returns a value | `HAL_RNG_Read(&value)`; `HAL_ERROR__NOT_SUPPORTED` on the STM32L152 |
| `HAL_CRC_Calculate` returns 16 bits | `HAL_CRC_Calculate(data, size, &crc)`, CRC-32/MPEG-2 |
| Buffer sizes are `uint8_t` | `uint16_t` |
| `Log_Init`, `Log` inside the library | Not part of the HAL. Use `HAL_UART_WriteBuff` from the application. |
| Application glue named `HAL_GPIO_Init`, `HAL_UART_Init` | Use another prefix (`APP_`, `BSP_`); `HAL_` belongs to the HAL. |

The radio driver and the TSCH stack of WirelessControl have not been migrated.

## The 46 review points

| # | Point | Where it is addressed |
|---|---|---|
| 1 | Core voltage range not set for 32 MHz | `HAL_CLOCK_SetVoltage`: field written whole, `VOSF` awaited, limits checked in `HAL_CLOCK_Validate` |
| 2 | Stubs returned fake data | `hal_unsupported.c` returns `HAL_ERROR__NOT_SUPPORTED`; `hal_crc.c` is a real CRC; `HAL_FLASH_MassErase` says not supported |
| 3 | EXTI clear wiped all lines | `HAL_FAMILY_EXTI_PR = mask` in `hal_gpio_v2.c` |
| 4 | Racy flag clears on USART and timers | UART handler never writes `SR` for reception; timers use `SR = ~flag` |
| 5 | Disabling one source disabled the NVIC line | `HAL_xxx_SetInterrupt` only changes the enable bit of the source |
| 6 | `HAL_GPIO_Config` always set an NVIC priority | Priority moved to `HAL_GPIO_ConfigInterrupt`; a different priority on a shared line is `HAL_ERROR__CONFLICT` |
| 7 | Wrong pins for USART3 and UART5 | No pin in the HAL; `board.h` gives them |
| 8 | Wrong alternate function for SPI3 | The function travels with the pin (`HAL_GPIO_PIN_T`) |
| 9 | Config functions only ORed bits in | Whole register or cleared field, in every driver |
| 10 | Baud rate from SYSCLK, truncated | `HAL_CALC_UartDivider` on the bus clock, rounded, range checked |
| 11 | Wake-up flag not cleared, `LPRUN` misused | `hal_lpm.c`: `CWUF` written as 1; `LPRUN` only with MSI 131 kHz in range 2 |
| 12 | ADC died after a clock switch | `HAL_CLOCK_RequestHsi` / `ReleaseHsi`; restarted after stop mode |
| 13 | Half page programming from flash, no error check | `HAL_FLASH_WriteHalfPage` in `.ramfunc` with interrupts masked; error flags checked everywhere |
| 14 | No timeouts | `HAL_CLOCK_TIMEOUT_T` in every wait: clock, UART, SPI, I2C, ADC, flash, low power |
| 15 | No error reporting | Return codes; UART and SPI error callbacks; I2C NACK, bus error, arbitration loss |
| 16 | No parameter checks | Every public function with a status return checks its arguments |
| 17 | Worked only at `-O0` | No shared SysTick flag; builds and is tested at every optimisation level |
| 18 | TX and RX callbacks could lock the system | Handlers read the data register and switch the TX source off when there is nothing to send |
| 19 | Toggle used `ODR ^=` | `HAL_GPIO_Write` writes `BSRR` only |
| 20 | I2C read sequence | Three sequences in `HAL_I2C_Receive`: 1 byte, 2 bytes, more |
| 21 | 71 mandatory callbacks | Callbacks in the flags, none mandatory |
| 22 | Pins and board choices compiled in | `Board/NUCLEO_L152RE/board.h` |
| 23 | Clock assumptions | `HAL_CLOCK_Get` knows the PLL source; `GetBus`, `GetTimer`; SPI and I2C derive from the bus clock |
| 24 | SPI owned the chip select and dropped RX data | `HAL_SPI_Transfer(tx, rx, size)`, no chip select |
| 25 | `uint8_t` sizes | `uint16_t` |
| 26 | Structures passed by value | `const` pointers |
| 27 | Pin write was a function with a switch | `static inline` in `hal_gpio.h`; a toggle is 7 instructions |
| 28 | No time base | 1 ms tick, `HAL_CLOCK_Delay`, `HAL_CLOCK_DelayUs`, timeouts |
| 29 | Device header leaked into the application | Reversed on request: `hal_port.h` includes it, so that the HAL redefines nothing (rule 9) |
| 30 | One function per instance and per mode | `HAL_LPM_Enter(mode)`; one interrupt function per driver |
| 31 | Sources missing, Debug build shipped | Sources here; Release is `-Os` |
| 32 | Lookup tables in RAM | `const` tables in `hal_devices.c` |
| 33 | Duplicated platform files | One startup file, one linker script |
| 34 | Application glue used the `HAL_` prefix | The example uses `APP_` |
| 35 | Header clean-up | No `NULL` redefinition, 0 is success, nothing declared that does not exist |
| 36 | Timer direction set the enable bit | `TIM_CR1_DIR`; timers that cannot count down say so |
| 37 | `OTYPER` written with OR | Field cleared then set |
| 38 | SPI flag clear typo, NVIC setup commented out | Rewritten; the NVIC line is configured |
| 39 | Low-power exit selected voltage range 3 | The range is only written by `HAL_CLOCK_SetVoltage` |
| 40 | Compare value 0 could not be set | `HAL_TIMER_ConfigChannel` and `HAL_TIMER_SetCompare` accept 0 |
| 41 | L4/F7/H7 UART flag clears | Not applicable to the L1. Recorded as a rule for `hal_uart_v2.c`, see below. |
| 42 | L476 GPIO config enabled the EXTI line | Not applicable to the L1 driver, which never does it; `hal_gpio_v2.c` is the driver the L4 will use |
| 43 | One hand-edited copy per project | One repository; projects select a family and a board |
| 44 | Fixes did not propagate | One driver per peripheral version; the PC tests fail when an old defect comes back |
| 45 | Callback count kept growing | 0 mandatory |
| 46 | No timeouts, no `volatile`, no checks anywhere | See 14, 16, 17 |

## Tests

`Tests/host/test_calc.c` checks the baud rate, SPI prescaler, I2C timing and CRC calculations.

`Tests/host/sim/test_registers.c` compiles the real driver sources for the PC and runs them
against simulated STM32L152RE registers: the peripheral address ranges are mapped as memory and a
small model plays the hardware reactions the drivers wait for (oscillator ready bits, clock switch
status, the SysTick counter, the ADC). It checks the values the drivers write, the error paths
and the interrupt handlers. Seven of the old defects were put back one at a time to confirm the
test fails for each (voltage range, EXTI clear, alternate function, timer direction, baud rate
rounding, NVIC line, SPI speed).

What these tests cannot show: timing, electrical behaviour, and what the hardware does by itself.

## To verify on hardware

Everything, since nothing has run on a board yet. In order of risk:

1. **Clock start-up**: 32 MHz from HSE bypass and from the HSI, the fallback when the HSE is
   missing, the LSE timeout on a board without the 32.768 kHz crystal.
2. **I2C**: reads of 1, 2 and 3 or more bytes against a real device (the AT24MAC602), a missing
   device, and `HAL_I2C_Recover` with the data line held low.
3. **Low power**: stop mode wake-up and clock restore, low-power run entry and exit, standby.
4. **Flash**: page erase, word program, and the half page write that runs from RAM.
5. **Timer LSE clock**: TIM9 through internal trigger 1, TIM10 and TIM11 through the external
   trigger remap. This follows the reference manual and has not been seen working.
6. **UART** at 921600 baud, overrun handling, interrupt driven transmission.
7. **SPI** against the AT86RF212B, **ADC** internal channels, **DAC**, **DMA**.

## Porting to another family

1. Create `Hardware/Family/<FAMILY>/` with `hal_port.h`, `hal_family.h`, `hal_family_irq.h`,
   `hal_devices.c` and the family specific modules (clock, flash, low power, ADC, CRC).
2. Reuse the drivers of `Hardware/Drivers` whose peripheral version matches. The L4, F7 and H7
   need two new ones: `hal_uart_v2.c` (USART with `ISR`/`ICR`/`RDR`/`TDR`) and `hal_i2c_v2.c`
   (I2C with `TIMINGR`).
3. Add the family to `CMakeLists.txt` and a directory under `Platform` and `Board`.
4. Take every register, address and bit from the device header of the new family (rule 9). When
   a name differs between families, `hal_family.h` gives it one alias; it never copies a value.

Rules for `hal_uart_v2.c`, from review points 41 and 42: clear flags by writing `ICR`, never by
writing `ISR`; do not use `RXFRQ` to acknowledge a byte that the callback still has to receive;
do not enable an EXTI line from `HAL_GPIO_Config`.

## Limits

- I2C is master only and blocking. SPI and UART have no DMA mode; the DMA driver is standalone.
- UART frames of 9 data bits pass their low 8 bits only.
- `HAL_CLOCK_GetTick` does not count the time spent in sleep (unless `m_ucKeepTick`) or stop.
- After `HAL_CLOCK_Update`, modules that derived a divider from a bus clock (UART, SPI, I2C)
  must be configured again.
- The RTC, the watchdogs, the comparators, the LCD controller and USB have no driver.

## Third party files

`Platform/STM32L152RE/CMSIS` is from Arm (Apache-2.0). `stm32l152xe.h`, `stm32l1xx.h` and
`system_stm32l1xx.h` are from STMicroelectronics (BSD 3-Clause). They are unchanged copies of the
files used in the earlier projects. No license has been chosen for the rest of the repository.
