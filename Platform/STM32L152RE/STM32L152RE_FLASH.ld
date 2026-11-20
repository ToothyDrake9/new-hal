/**************************************************************************************************
 * @file    STM32L152RE_FLASH.ld
 * @brief   STM32L152RE linker script: 512 KB of flash, 80 KB of RAM, program in flash
 * @author  Andustria
 * @date    November 2026
 **************************************************************************************************/
ENTRY(Reset_Handler)

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
	RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 80K
}

/* Smallest heap and stack the link accepts. */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x800;

/* The stack starts at the end of RAM and grows down. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
	.isr_vector :
	{
		. = ALIGN(4);
		KEEP(*(.isr_vector))
		. = ALIGN(4);
	} > FLASH

	.text :
	{
		. = ALIGN(4);
		*(.text)
		*(.text*)
		*(.glue_7)
		*(.glue_7t)
		*(.eh_frame)
		KEEP(*(.init))
		KEEP(*(.fini))
		. = ALIGN(4);
		_etext = .;
	} > FLASH

	.rodata :
	{
		. = ALIGN(4);
		*(.rodata)
		*(.rodata*)
		. = ALIGN(4);
	} > FLASH

	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASH

	.ARM.exidx :
	{
		__exidx_start = .;
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > FLASH

	.preinit_array :
	{
		PROVIDE_HIDDEN(__preinit_array_start = .);
		KEEP(*(.preinit_array*))
		PROVIDE_HIDDEN(__preinit_array_end = .);
	} > FLASH

	.init_array :
	{
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array*))
		PROVIDE_HIDDEN(__init_array_end = .);
	} > FLASH

	.fini_array :
	{
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array*))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} > FLASH

	/* Where the initial values of .data are stored in flash. */
	_sidata = LOADADDR(.data);

	/* Initialized data, and the functions that must run from RAM (flash programming). */
	.data :
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data)
		*(.data*)
		. = ALIGN(4);
		*(.ramfunc)
		*(.ramfunc*)
		. = ALIGN(4);
		_edata = .;
	} > RAM AT > FLASH

	.bss :
	{
		. = ALIGN(4);
		_sbss = .;
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		__bss_end__ = _ebss;
	} > RAM

	/* Fails the link when RAM cannot hold the smallest heap and stack. */
	._user_heap_stack :
	{
		. = ALIGN(8);
		PROVIDE(end = .);
		PROVIDE(_end = .);
		. = . + _Min_Heap_Size;
		. = . + _Min_Stack_Size;
		. = ALIGN(8);
	} > RAM

	.ARM.attributes 0 : { *(.ARM.attributes) }
}
