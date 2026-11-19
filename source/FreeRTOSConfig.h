/* FreeRTOSConfig.h: frdmmcxn236_m33_scratch, step 6.0 draft (session 26, 2026-11-19)
 * Kernel: SDK 26.06 rtos/freertos/freertos-kernel-upstream, V11.2.0 (task.h:56)  -- D1 = S
 * Port:   portable/GCC/ARM_CM33_NTZ/non_secure, Secure-only, no TrustZone, no MPU
 * Every value below is from our log / files or a decision D1-D7; no template values.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Clock and tick ---------------------------------------------------- */
#define configCPU_CLOCK_HZ                      48000000UL  /* 48 MHz core clock (log 1e.2a); port loads SysTick with this / tick rate - 1 */
#define configTICK_RATE_HZ                      1000U       /* 1 ms tick, same period as our 2.3b / 5.3 SysTick (RVR 47999) */
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS /* required: FreeRTOS.h errors if missing */

/* ---- Scheduler --------------------------------------------------------- */
#define configUSE_PREEMPTION                    1           /* required; tick may switch tasks (like our 5.3 slice) */
#define configUSE_TIME_SLICING                  1           /* equal-priority tasks share the CPU tick by tick */
#define configMAX_PRIORITIES                    5U          /* required; idle = 0, our tasks 1..4 */
#define configMINIMAL_STACK_SIZE                128U        /* required; WORDS (512 B), idle task stack; check high-water mark in 6.3 */
#define configMAX_TASK_NAME_LEN                 8U          /* "TaskA" / "TaskB" fit (our 5.1 TCB names) */
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TICKLESS_IDLE                 0           /* no low-power tick suppression in Phase 6 */

/* ---- Memory: D3 = T (static only) ------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         1           /* TCBs, stacks, queue storage = named .bss symbols, visible in nm */
#define configSUPPORT_DYNAMIC_ALLOCATION        0           /* no heap_x.c in the build (D2) */
#define configKERNEL_PROVIDED_STATIC_MEMORY     0           /* we write vApplicationGetIdleTaskMemory ourselves (D3) */

/* ---- Features kept OFF (D2: no timers.c / event_groups.c / stream_buffer.c) */
#define configUSE_TIMERS                        0           /* no timer task -> no vApplicationGetTimerTaskMemory */
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               0U
#define configUSE_TASK_NOTIFICATIONS            1           /* default; costs a few bytes per TCB, unused for now */
#define configUSE_CO_ROUTINES                   0

/* ---- Hooks and diagnostics -------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0           /* no heap */
#define configCHECK_FOR_STACK_OVERFLOW          0           /* PSPLIM per task (port) gives the hardware check, as our 3.3 MSPLIM did */
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0

/* ---- Cortex-M33 port: D6 ---------------------------------------------- */
#define configRUN_FREERTOS_SECURE_ONLY          1           /* we run Secure, SAU off -> portINITIAL_EXC_RETURN 0xfffffffd (port.c 308) */
#define configENABLE_TRUSTZONE                  0           /* must be 0 with SECURE_ONLY = 1 (port.c #error) */
#define configENABLE_MPU                        0           /* non-MPU PendSV (portasm.c 398) / SVC (508) */
#define configENABLE_FPU                        1           /* hard-float build + CPACR set in Reset_Handler; PendSV saves S16-S31 when FType = 0 */
#define configENABLE_MVE                        0           /* M55/M85 only */
#define configCHECK_HANDLER_INSTALLATION        1           /* asserts at port.c 1737/1738: our slots 11 and 14 must hold the port's handlers */

/* ---- Interrupt priority: D7 ------------------------------------------- */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x20U       /* level 1 of 8; 0x20 & 0xE0 = 0x20 (assert 1768), 0x20 & 0x1F = 0 */
/* configKERNEL_INTERRUPT_PRIORITY is not used by the ARMv8-M port: PendSV and
 * SysTick are forced to the lowest level by port.c 1818/1819 (reads back 0xE0). */

/* ---- API functions compiled in ---------------------------------------- */
#define INCLUDE_vTaskDelay                      1           /* 6.1 LED task */
#define INCLUDE_vTaskDelayUntil                 0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    0
#define INCLUDE_uxTaskGetStackHighWaterMark     1           /* 6.3 stack check */
#define INCLUDE_xTaskGetSchedulerState          0
#define INCLUDE_xTaskGetCurrentTaskHandle       0

/* ---- Assert: park with the failing line visible in GDB ----------------- */
#ifndef __ASSEMBLER__
#include <stdint.h>
extern volatile uint32_t g_assert_line;         /* defined in frdmmcxn236_m33_scratch.c (6.1) */
extern const char * volatile g_assert_file;     /* defined in frdmmcxn236_m33_scratch.c (6.1) */
#define configASSERT( x )                                        \
    do {                                                         \
        if( ( x ) == 0 ) {                                       \
            __asm volatile ( "cpsid i" ::: "memory" );           \
            g_assert_file = __FILE__;                            \
            g_assert_line = __LINE__;                            \
            for( ;; ) { }                                        \
        }                                                        \
    } while( 0 )
#endif

#endif /* FREERTOS_CONFIG_H */
