cmake_minimum_required(VERSION 3.20)

project(new_hal C)

# What the HAL is built for. Another family is another directory under Hardware/Family and
# another list of drivers below; another chip is another directory under Platform.
set(HAL_FAMILY STM32L1 CACHE STRING "Microcontroller family")
set(HAL_DEVICE STM32L152RE CACHE STRING "Microcontroller")
set(HAL_BOARD NUCLEO_L152RE CACHE STRING "Board")
set(HAL_EXTRA_C_FLAGS "" CACHE STRING "Compiler flags added after the default ones (e.g. -O2)")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g")

if(HAL_FAMILY STREQUAL "STM32L1")
	set(HAL_CPU_FLAGS -mcpu=cortex-m3 -mthumb)
	set(HAL_DEVICE_DEFINE STM32L152xE)
	set(HAL_DRIVER_SOURCES
		Hardware/Drivers/hal_gpio_v2.c
		Hardware/Drivers/hal_uart_v1.c
		Hardware/Drivers/hal_spi_v1.c
		Hardware/Drivers/hal_i2c_v1.c
		Hardware/Drivers/hal_timer_v1.c
		Hardware/Drivers/hal_dma_v1.c
		Hardware/Drivers/hal_dac_v1.c
	)
	set(HAL_FAMILY_SOURCES
		Hardware/Family/STM32L1/hal_devices.c
		Hardware/Family/STM32L1/hal_clock.c
		Hardware/Family/STM32L1/hal_adc.c
		Hardware/Family/STM32L1/hal_flash.c
		Hardware/Family/STM32L1/hal_crc.c
		Hardware/Family/STM32L1/hal_lpm.c
		Hardware/Family/STM32L1/hal_unsupported.c
	)
else()
	message(FATAL_ERROR "HAL_FAMILY ${HAL_FAMILY} is not ported yet")
endif()

set(HAL_COMPILE_OPTIONS
	${HAL_CPU_FLAGS}
	-ffunction-sections
	-fdata-sections
	-Wall
	-Wextra
	-Wshadow
	-Wundef
	-Wconversion
	-Wno-sign-conversion
	-Wstrict-prototypes
	-Wmissing-prototypes
	-Werror
)

separate_arguments(HAL_EXTRA_C_FLAGS_LIST NATIVE_COMMAND "${HAL_EXTRA_C_FLAGS}")
list(APPEND HAL_COMPILE_OPTIONS ${HAL_EXTRA_C_FLAGS_LIST})

# The HAL, as a static library. Only the drivers that the application calls are linked.
add_library(hal STATIC ${HAL_DRIVER_SOURCES} ${HAL_FAMILY_SOURCES})
# The public headers use the CMSIS device header, so the application gets it as well.
target_compile_definitions(hal PUBLIC ${HAL_DEVICE_DEFINE})
target_compile_options(hal PRIVATE ${HAL_COMPILE_OPTIONS})
target_include_directories(hal
	PUBLIC
		Globals
		Hardware
		Hardware/Family/${HAL_FAMILY}
		Platform/${HAL_DEVICE}
		Platform/${HAL_DEVICE}/CMSIS
	PRIVATE
		Hardware/Drivers
)

# The flash driver keeps one function in RAM, next to the data. Newer linkers warn about a
# segment that is both writable and executable; that is intended here.
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
	set(HAL_LINK_QUIET_RWX -Wl,--no-warn-rwx-segments)
endif()

# A program for the board: sources, startup file, linker script, map file, .hex and .bin.
function(hal_add_program p_name)
	add_executable(${p_name} ${ARGN} Platform/${HAL_DEVICE}/startup_stm32l152re.c)
	set_target_properties(${p_name} PROPERTIES SUFFIX .elf)
	target_compile_options(${p_name} PRIVATE ${HAL_COMPILE_OPTIONS})
	target_include_directories(${p_name} PRIVATE Board/${HAL_BOARD})
	target_link_libraries(${p_name} PRIVATE hal)
	target_link_options(${p_name} PRIVATE
		${HAL_CPU_FLAGS}
		-T${CMAKE_SOURCE_DIR}/Platform/${HAL_DEVICE}/${HAL_DEVICE}_FLASH.ld
		-nostartfiles
		--specs=nano.specs
		--specs=nosys.specs
		-Wl,--gc-sections
		-Wl,--fatal-warnings
		${HAL_LINK_QUIET_RWX}
		-Wl,-Map=$<TARGET_FILE_DIR:${p_name}>/${p_name}.map
	)
	add_custom_command(TARGET ${p_name} POST_BUILD
		COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${p_name}> $<TARGET_FILE_DIR:${p_name}>/${p_name}.hex
		COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${p_name}> $<TARGET_FILE_DIR:${p_name}>/${p_name}.bin
		COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${p_name}>
	)
endfunction()

hal_add_program(example Example/main.c)
hal_add_program(link_all Tests/link/link_all.c)
