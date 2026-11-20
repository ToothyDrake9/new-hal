# Tests that run on the development machine. Build with its own compiler, not the Arm one:
#   cmake -S Tests/host -B build-host && cmake --build build-host && ctest --test-dir build-host
#
# test_calc       checks the divider and timing calculations of hal_calc.h.
# test_registers  runs the real driver sources against simulated STM32L152RE registers
#                 (Linux only: it maps the peripheral addresses as memory).
cmake_minimum_required(VERSION 3.20)

project(new_hal_host_tests C)

enable_testing()

set(HAL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(test_calc test_calc.c)
target_include_directories(test_calc PRIVATE ${HAL_ROOT}/Globals ${HAL_ROOT}/Hardware)
target_compile_options(test_calc PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_calc COMMAND test_calc)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# The core header is copied next to a PC replacement of the compiler header it includes.
	set(SIM_CMSIS ${CMAKE_CURRENT_BINARY_DIR}/sim_cmsis)
	foreach(l_header core_cm3.h cmsis_version.h mpu_armv7.h)
		configure_file(${HAL_ROOT}/Platform/STM32L152RE/CMSIS/${l_header} ${SIM_CMSIS}/${l_header} COPYONLY)
	endforeach()
	configure_file(sim/cmsis_compiler.h ${SIM_CMSIS}/cmsis_compiler.h COPYONLY)

	file(GLOB HAL_SOURCES ${HAL_ROOT}/Hardware/Drivers/*.c ${HAL_ROOT}/Hardware/Family/STM32L1/*.c)

	add_executable(test_registers sim/test_registers.c ${HAL_SOURCES})
	target_compile_definitions(test_registers PRIVATE STM32L152xE)
	target_include_directories(test_registers PRIVATE
		${SIM_CMSIS}
		${HAL_ROOT}/Platform/STM32L152RE
		${HAL_ROOT}/Globals
		${HAL_ROOT}/Hardware
		${HAL_ROOT}/Hardware/Drivers
		${HAL_ROOT}/Hardware/Family/STM32L1
		${HAL_ROOT}/Board/NUCLEO_L152RE
	)
	# Pointers are 64 bit on the PC and the register addresses fit in 32 bits: the casts are fine.
	target_compile_options(test_registers PRIVATE
		-include ${CMAKE_CURRENT_SOURCE_DIR}/sim/sim_hook.h
		-Wall -Wextra -Werror
		-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-attributes
	)
	add_test(NAME test_registers COMMAND test_registers)
endif()
