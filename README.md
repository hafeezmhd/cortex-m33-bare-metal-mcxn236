# Bare-metal Cortex-M33 from scratch, up to a hand-ported FreeRTOS — NXP FRDM-MCXN236

No SDK startup, no SDK linker script, no CMSIS headers, no C library: one hand-written
linker script, one hand-written vector table + `Reset_Handler`, C that talks to registers
through its own address macros — then a hand-written PendSV context switcher, and finally
the FreeRTOS kernel brought up on that same startup code. Every step was predicted first,
run on the board, and checked against debugger readings. The full record (prediction, raw
result, grade, lesson) is in [`learning_log.md`](learning_log.md).

**Board:** FRDM-MCXN236 (MCXN236, Arm Cortex-M33 r1p0, FPv5-SP, TrustZone-M; runs Secure,
48 MHz FIRC) · **Tools:** MCUXpresso IDE 25.6 (GCC 14.2.1, `-O0`, `-nostdlib`), LinkServer/GDB
via the on-board MCU-Link · **Kernel:** FreeRTOS-Kernel V11.2.0, port `GCC/ARM_CM33_NTZ`
(Secure-only).

## What is proven on hardware

| Phase | Topic | Measured evidence (examples) | Write-up |
|---|---|---|---|
| 1 | Boot from nothing | Own `.ld` + 56-word vector table; boot ROM calls our `Reset_Handler` (BLX); `.data` copy / `.bss` zero; FPU enabled via CPACR before any FP instruction (NOCP fault reproduced when skipped) | [phase-0-1.md](docs/phase-0-1.md) |
| 1 | Register to pin | Red LED (P4_18, active-low) and LPUART4 at 115200 8N1 to the VCOM — clock gates, resets, pin mux, FLEXCOMM mode, BAUD/CTRL all written by our code | [phase-0-1.md](docs/phase-0-1.md) |
| 2 | Exceptions and NVIC | SysTick period = 48,000 cycles (CYCCNT); 3 priority bits (write 0xFF → read 0xE0); preemption vs waiting vs tail-chaining, tail-chain proven by cycle count (23 vs ≥ 32); DSB/ISB placement measured | [phase-2.md](docs/phase-2.md) |
| 3 | Faults on purpose | DIVBYZERO and UNALIGNED traps; stack overflow via MSPLIM (STKOF, SP = limit, no frame); precise BusFault (BFAR 0x500B_4030); MemManage via the MPU (MMFAR 0x2000_8040), and proof the faulting store never wrote memory; handlers trust MMFAR/BFAR only when the VALID bit is set | [phase-3.md](docs/phase-3.md) |
| 4 | Modes, privilege, SVC | Thread mode on PSP (CONTROL 0xC → 0xE + ISB; fault frame 0x2000_B000 → 0x2000_AF98, EXC_RETURN 0xFFFF_FFED); unprivileged Thread under the MPU faults on its first fetch (CFSR 0x11 = IACCVIOL + MSTKERR); `svc 0` handler reads imm8 at the stacked return address − 2 and clears nPRIV (CONTROL_S 0xF → 0x1 on entry → 0x0 → 0xE after return: only nPRIV survives the return) | [phase-4.md](docs/phase-4.md) |
| 5 | Own context switcher | PendSV handler in assembly, two tasks on their own PSP stacks with hand-built initial frames; SysTick pends PendSV every 100 ticks and tail-chains into it (LR 0xFFFF_FFFD); one switch = **70 cycles**; a SysTick forced to preempt PendSV nests on MSP (EXC_RETURN 0xFFFF_FFF1) and the switch still completes | [phase-5.md](docs/phase-5.md) |
| 6 | FreeRTOS port by hand | Kernel V11.2.0 on our startup + `.ld`, static allocation only, own `memset`/`memcpy`; port handlers own vector slots 11/14/15 (installation asserts pass); the start reclaims main's MSP (0x2000_FFB8 → 0x2001_0000); queue A → B: event switch (dt 0 ticks, loop 504) vs tick switch at equal priority (dt 1, loop 503); FreeRTOS switch = **250 / 252 cycles** vs our 70 (−O0 C call + BASEPRI window); stack used A 160 / B 156 / idle 100 B by 0xA5 scan; PSPLIM bits [2:0] read back 0, so the limit rounds down onto the TCB's last word | [phase-6.md](docs/phase-6.md) |

## Layout

```
source/frdmmcxn236_m33_scratch.c   main, tasks, fault handlers, register macros (header lists every step)
source/startup_m33.S               vector table, Reset_Handler, weak handler aliases
source/FreeRTOSConfig.h            our kernel configuration (every value from the log)
source/freertos/                   FreeRTOS-Kernel V11.2.0, unmodified (tasks.c, queue.c, list.c,
                                   include/, portable/GCC/ARM_CM33_NTZ/non_secure/)
m33_scratch.ld                     FLASH 1 MB @0x0, RAMB 32 KB @0x2000_8000
docs/phase-*.md                    one write-up per phase: what was built, what was measured, what it showed
docs/interview_drills.md           36 interview questions answered with the measured values (answers folded)
learning_log.md                    every step: prediction, raw result, grade, what was learned
```

## Build and run

Import into MCUXpresso IDE (File → Import → Existing Projects into Workspace), build the
`Debug` configuration (0 errors, 0 warnings; FLASH 9028 B, RAMB 3136 B), debug with the
MCU-Link probe. On the VCOM (115200 8N1): `Hello`, `IRQ 39 taken`, `Scheduler start`,
then one `B got N dt=0` line every 504 ticks (≈ 0.5 s) from task B; the red LED toggles
once per task-A loop (same 504-tick period).

Each finished phase is tagged, so an earlier image can be built as it was:

| Tag | Commit | Image |
|---|---|---|
| `phase-4` | 6375ed1 | SVC back to privileged (4.3) |
| `phase-5` | 2a202a9 | own PendSV switcher, SysTick time slicing (5.3a) |
| `phase-6` | 31d7b5a | FreeRTOS, two tasks + a queue (6.2) |

`git checkout phase-5`, then Project → Clean before building. Phases 0–3 have no tag of
their own (they predate the repository's first commit).

## Method

Explain → compose → predict (values + a source for each) → lock → run on the board →
grade against hardware evidence only → log. References used: Arm DDI0553 (Armv8-M ARM),
Cortex-M33 TRM, NXP MCX N23x Reference Manual, FRDM-MCXN236 user manual and schematics
(not included here; available from Arm and NXP).

## Licence

Own code and documents: MIT, see [`LICENSE`](LICENSE). The FreeRTOS kernel files in
`source/freertos/` are unmodified copies of FreeRTOS-Kernel V11.2.0 (from the NXP SDK 26.06
`freertos-kernel-upstream` folder) under their own MIT licence, see
[`source/freertos/LICENSE.md`](source/freertos/LICENSE.md).
