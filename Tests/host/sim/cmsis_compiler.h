/**************************************************************************************************
 * @file    cmsis_compiler.h
 * @brief   PC replacement of the CMSIS compiler header, for the register level tests
 * @author  Andustria
 * @date    November 2026
 * @changes Created
 * @remarks The real header contains Arm instructions. Here the core functions the HAL uses are
 *          modelled in C, so the HAL sources compile and run on the development machine. The
 *          interrupt mask is a variable that the tests check after every call.
 **************************************************************************************************/
#ifndef __CMSIS_COMPILER_H
#define __CMSIS_COMPILER_H

#include <stdint.h>

#define __ASM                 __asm
#define __INLINE              inline
#define __STATIC_INLINE       static inline
#define __STATIC_FORCEINLINE  __attribute__((always_inline)) static inline
#define __NO_RETURN           __attribute__((__noreturn__))
#define __USED                __attribute__((used))
#define __WEAK                __attribute__((weak))
#define __PACKED              __attribute__((packed, aligned(1)))
#define __PACKED_STRUCT       struct __attribute__((packed, aligned(1)))
#define __PACKED_UNION        union __attribute__((packed, aligned(1)))
#define __ALIGNED(x)          __attribute__((aligned(x)))
#define __RESTRICT            __restrict
#define __COMPILER_BARRIER()  ((void) 0)

extern volatile uint32_t g_ulSIM_Primask;
extern volatile uint32_t g_ulSIM_WaitForInterrupt;

#define __NOP()  ((void) 0)
#define __ISB()  ((void) 0)
#define __DSB()  ((void) 0)
#define __DMB()  ((void) 0)
#define __WFI()  (g_ulSIM_WaitForInterrupt++)
#define __CLZ(x) ((uint8_t) __builtin_clz(x))

static inline void __enable_irq(void) { g_ulSIM_Primask = 0; }
static inline void __disable_irq(void) { g_ulSIM_Primask = 1; }
static inline uint32_t __get_PRIMASK(void) { return g_ulSIM_Primask; }
static inline void __set_PRIMASK(uint32_t p_ulValue) { g_ulSIM_Primask = p_ulValue; }

#endif /* __CMSIS_COMPILER_H */
