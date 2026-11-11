# Bare-metal Cortex-M33 from scratch — NXP FRDM-MCXN236

No SDK startup, no SDK linker script, no CMSIS headers: one hand-written linker script,
one hand-written vector table + `Reset_Handler`, and C that talks to registers through
its own address macros. Every step was predicted first, run on the board, and checked
against debugger readings. The full record is in [`learning_log.md`](learning_log.md).

**Board:** FRDM-MCXN236 (MCXN236, Arm Cortex-M33 r1p0, FPv5-SP, TrustZone-M; runs Secure,
48 MHz FIRC) · **Tools:** MCUXpresso IDE 25.6 (arm-none-eabi-gcc, -O0), LinkServer/GDB via the
on-board MCU-Link.

## What is proven on hardware

| Phase | Topic | Measured evidence (examples) |
|---|---|---|
| 1 | Boot from nothing | Own `.ld` + 56-word vector table; boot ROM calls our `Reset_Handler` (BLX); `.data` copy / `.bss` zero; FPU enabled via CPACR before any FP instruction (NOCP fault reproduced when skipped) |
| 1 | Register to pin | Red LED (P4_18, active-low) and LPUART4 at 115200 8N1 to the VCOM — clock gates, resets, pin mux, FLEXCOMM mode, BAUD/CTRL all written by our code |
| 2 | Exceptions and NVIC | SysTick period = 48,000 cycles (CYCCNT); 3 priority bits (write 0xFF → read 0xE0); preemption vs waiting vs tail-chaining, tail-chain proven by cycle count (23 vs ≥ 32); DSB/ISB placement measured |
| 3 | Faults on purpose | DIVBYZERO and UNALIGNED traps; stack overflow via MSPLIM (STKOF, SP = limit, no frame); precise BusFault (BFAR 0x500B_4030); MemManage via the MPU (MMFAR 0x2000_8040), and proof the faulting store never wrote memory; handlers trust MMFAR/BFAR only when the VALID bit is set |
| 4 | Modes and stacks | Thread mode moved to PSP (CONTROL 0xC → 0xE + ISB); a fault from Thread stacks its 26-word frame on the PSP (0x2000_B000 → 0x2000_AF98), EXC_RETURN 0xFFFF_FFED, handler on MSP |

## Layout

```
source/frdmmcxn236_m33_scratch.c   main, handlers, register macros (header lists every step)
source/startup_m33.S               vector table, Reset_Handler, weak handler aliases
m33_scratch.ld                     FLASH 1 MB @0x0, RAMB 32 KB @0x2000_8000
learning_log.md                    every step: prediction, raw result, grade, what was learned
```

## Build

Import into MCUXpresso IDE (File → Import → Existing Projects), build the `Debug`
configuration, debug with the MCU-Link probe. The current image intentionally ends in a
MemManage fault (step 3.5/4.1) that `MemManage_Handler` records and parks on.

## Method

Explain → compose → predict (values + a source for each) → lock → run on the board →
grade against hardware evidence only → log. References used: Arm DDI0553 (Armv8-M ARM),
Cortex-M33 TRM, NXP MCX N23x Reference Manual, FRDM-MCXN236 user manual and schematics
(not included here; available from Arm and NXP).

## Roadmap

Next: unprivileged Thread mode + SVC (Phase 4), a hand-written PendSV context switcher
(Phase 5), and a FreeRTOS CM33 port brought up on this same startup code (Phase 6).
