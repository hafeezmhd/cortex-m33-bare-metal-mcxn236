/* startup_m33.S: vector table + Reset_Handler
 * Steps: 1a.4 table/Thumb bit, 1b.3 .data copy/.bss zero, 1b.4b FPU enable (CPACR)
 * Guide 1.4, 1.5, 1.10; RM Table 158 (boot ROM header slots) 
 * 2.4: the SysTick interrupt.
 * 2.5a table to vector 55 + own IRQ 39 handler (static)
 * 3.1c: UsageFault vector override to UsageFault_Handler
 * 3.4: a real BusFault, and a handler that checks BFARVALID before using BFAR
 * 3.5: MemManage via the MPU*/

    .syntax unified
    .cpu    cortex-m33
    .thumb

/* ---------------- Vector table: must be first in flash (0x0000_0000) ---- */
    .section .isr_vector, "a"
    .align  2
    .globl  __isr_vector
__isr_vector:
    .word   __stack_top        /* 0x00     initial MSP (from linker script)  */
    .word   Reset_Handler      /* 0x04  1  Reset                             */
    .word   Default_Handler    /* 0x08  2  NMI                               */
    .word   Default_Handler    /* 0x0C  3  HardFault                         */
    .word   MemManage_Handler  /* 0x10  4  MemManage                         */
    .word   BusFault_Handler   /* 0x14  5  BusFault                          */
    .word   UsageFault_Handler /* 0x18  6  UsageFault                        */
    .word   Default_Handler    /* 0x1C  7  SecureFault                       */
    .word   0                  /* 0x20  8  reserved | ROM: image length      */
    .word   0                  /* 0x24  9  reserved | ROM: imageType 0=plain */
    .word   0                  /* 0x28 10  reserved | ROM: CRC / ext header  */
    .word   Default_Handler    /* 0x2C 11  SVCall                            */
    .word   Default_Handler    /* 0x30 12  DebugMonitor                      */
    .word   0                  /* 0x34 13  reserved | ROM: exec addr 0=XIP   */
    .word   Default_Handler    /* 0x38 14  PendSV                            */
    .word   SysTick_Handler    /* 0x3C 15  SysTick                           */
    /* 16..55 external IRQs: exception = 16 + IRQ (DDI0553 B3.9 RCMTC p.80); word offset = exception number (B3.30 RWPRT p.141); 
    IRQ 39 = LP_FLEXCOMM4 @0xDC (RM Table 15 p.33); past 0x38 no ROM header fields (RM Table 158 p.221) */
    .word   Default_Handler    /* 0x40 16  IRQ0  (LP_FLEXCOMM4 is IRQ39 -> slot 55) */
    .word   Default_Handler    /* 0x44 17  IRQ1                              */
    .word   Default_Handler    /* 0x48 18  IRQ2                              */
    .word   Default_Handler    /* 0x4C 19  IRQ3                              */
    .word   Default_Handler    /* 0x50 20  IRQ4                              */
    .word   Default_Handler    /* 0x54 21  IRQ5                              */
    .word   Default_Handler    /* 0x58 22  IRQ6                              */
    .word   Default_Handler    /* 0x5C 23  IRQ7                              */
    .word   Default_Handler    /* 0x60 24  IRQ8                              */
    .word   Default_Handler    /* 0x64 25  IRQ9                              */
    .word   Default_Handler    /* 0x68 26  IRQ10                             */
    .word   Default_Handler    /* 0x6C 27  IRQ11                             */
    .word   Default_Handler    /* 0x70 28  IRQ12                             */
    .word   Default_Handler    /* 0x74 29  IRQ13                             */
    .word   Default_Handler    /* 0x78 30  IRQ14                             */
    .word   Default_Handler    /* 0x7C 31  IRQ15                             */
    .word   Default_Handler    /* 0x80 32  IRQ16                             */
    .word   Default_Handler    /* 0x84 33  IRQ17                             */
    .word   Default_Handler    /* 0x88 34  IRQ18                             */
    .word   Default_Handler    /* 0x8C 35  IRQ19                             */
    .word   Default_Handler    /* 0x90 36  IRQ20                             */
    .word   Default_Handler    /* 0x94 37  IRQ21                             */
    .word   Default_Handler    /* 0x98 38  IRQ22                             */
    .word   Default_Handler    /* 0x9C 39  IRQ23                             */
    .word   Default_Handler    /* 0xA0 40  IRQ24                             */
    .word   Default_Handler    /* 0xA4 41  IRQ25                             */
    .word   Default_Handler    /* 0xA8 42  IRQ26                             */
    .word   Default_Handler    /* 0xAC 43  IRQ27                             */
    .word   Default_Handler    /* 0xB0 44  IRQ28                             */
    .word   Default_Handler    /* 0xB4 45  IRQ29                             */
    .word   Default_Handler    /* 0xB8 46  IRQ30                             */
    .word   Default_Handler    /* 0xBC 47  IRQ31                             */
    .word   Default_Handler    /* 0xC0 48  IRQ32                             */
    .word   Default_Handler    /* 0xC4 49  IRQ33                             */
    .word   Default_Handler    /* 0xC8 50  IRQ34                             */
    .word   Default_Handler    /* 0xCC 51  IRQ35                             */
    .word   Default_Handler    /* 0xD0 52  IRQ36                             */
    .word   Default_Handler    /* 0xD4 53  IRQ37                             */
    .word   Default_Handler    /* 0xD8 54  IRQ38                             */
    .word   LP_FLEXCOMM4_IRQHandler /* 0xDC 55  IRQ39 LP_FLEXCOMM4           */

/* ---------------- Code ---------------------------------------------------- */
    .text

    .thumb_func
    .globl  Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* enable FPU: CPACR CP10+CP11 = full access (bits 23:20)  step 1b.4b, log 0.7 */
    ldr     r0, =0xE000ED88    /* CPACR */
    ldr     r1, [r0]
    orr     r1, r1, #(0xF << 20)
    str     r1, [r0]
    dsb                        /* write completes ...          G1.10 */
    isb                        /* ... before any FP instruction is fetched */
    /* copy .data: flash (LMA) -> RAM (VMA) */
    ldr     r0, =__data_load__
    ldr     r1, =__data_start__
    ldr     r2, =__data_end__
1:  cmp     r1, r2
    bhs     2f
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    b       1b
2:
    /* zero .bss */
    ldr     r1, =__bss_start__
    ldr     r2, =__bss_end__
    movs    r3, #0
3:  cmp     r1, r2
    bhs     4f
    str     r3, [r1], #4
    b       3b
4:
    bl      main
    b       .
    .ltorg                     /* literal pool for the ldr =symbol loads */

    .thumb_func
    .globl  Default_Handler
    .type   Default_Handler, %function

Default_Handler:
    b       .                  /* any unexpected exception parks here */

/* ---- Weak default for SysTick: link-time override by user ISR ---------- */
    .weak   SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

/* ---- Weak default for LP_FLEXCOMM4 (IRQ39, slot 55): override by C ---- */
    .weak   LP_FLEXCOMM4_IRQHandler
    .thumb_set LP_FLEXCOMM4_IRQHandler, Default_Handler

/* ---- Weak default for UsageFault ---- */
 	.weak UsageFault_Handler
 	.thumb_set UsageFault_Handler, Default_Handler


/* ---- Weak default for BusFault ---- */
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

/* ---- Weak default for MemManage ---- */
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    