# Phase 6 — FreeRTOS V11.2.0 ported by hand onto our own startup code

**Tag:** [`phase-6`](https://github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236/tree/phase-6) (31d7b5a) ·
**Log:** `learning_log.md`, blocks "6.0" … "Field Guide Phase 6" (sessions 26–27, 19–23 Nov 2026)

The Phase 5 switcher was replaced by the real FreeRTOS kernel — but not by importing an SDK
FreeRTOS example. The kernel sources were copied unmodified, and everything around them stayed
ours: the hand-written vector table and `Reset_Handler`, the hand-written linker script,
`-nostdlib`, our own `FreeRTOSConfig.h` (every value traced to a measurement or a decision).
Then the port was measured with the same debugger methods used for our own switcher, so the
two can be compared number for number.

## 1. Decisions before any code (6.0)

| # | Question | Decision | Why |
|---|---|---|---|
| D1 | Which kernel source | NXP SDK 26.06 `rtos/freertos/freertos-kernel-upstream` = FreeRTOS-Kernel **V11.2.0** | the other SDK folder is NXP CMake glue only; line numbers checked against tag V11.2.0 |
| D2 | Which files | `tasks.c`, `queue.c`, `list.c`, `include/` (21 headers), `portable/GCC/ARM_CM33_NTZ/non_secure/` (port.c, portasm.c, portasm.h, portmacro.h, portmacrocommon.h) — 29 files, SHA-256 checked | timers, event groups, stream buffers, co-routines and every `heap_x.c` left out |
| D3 | Allocation | static only (`configSUPPORT_DYNAMIC_ALLOCATION 0`), idle-task memory supplied by us | every TCB, stack and queue is a named `.bss` symbol visible in `nm` |
| D4 | Handler clash | delete our `PendSV_Handler` (.S), `SysTick_Handler` and `SVC_Handler` (.c) and main's SysTick setup | the port defines all three; our vector table names them, so the port's symbols link in by name |
| D5 | C library | keep `-nostdlib`, write `memset` / `memcpy` ourselves | the kernel needs only these two (12 call sites); dropping `-nostdlib` would pull in crt0 + newlib that our `.ld` does not support |
| D6 | Port flavour | `configRUN_FREERTOS_SECURE_ONLY 1`, `TRUSTZONE 0`, `MPU 0`, `FPU 1` | we run Secure with the SAU off; initial EXC_RETURN `0xFFFFFFFD` (port.c 308) |
| D7 | `configMAX_SYSCALL_INTERRUPT_PRIORITY` | `0x20` (level 1 of 8) | 3 priority bits measured in Phase 2 (0xFF reads back 0xE0); `0x20 & 0xE0 = 0x20` passes the port's assert (port.c 1768) |

`configCHECK_HANDLER_INSTALLATION 1` makes the port assert (port.c 1737/1738) that vector slots
11 and 14 really hold its SVC and PendSV handlers — the proof that a hand-written vector table
and the port agree.

## 2. First task on hardware (6.1)

Build: 0 errors, 0 warnings, 7 objects, `-nostdlib`. Debugger readings at the first start:

| Point | Measured | Meaning |
|---|---|---|
| `vStartFirstTask` before `msr msp` | MSP `0x2000_FFB8` | 72 B of MSP used by `main` and the scheduler start |
| after `msr msp` | MSP `0x2001_0000` | the port reloads MSP from vector word 0 — main's stack is reclaimed for interrupts |
| `SVC_Handler` entry | LR `0xFFFF_FFE9`, frame 104 B | `svc` from Thread/MSP with FPCA 1 (main did one float operation) → extended 26-word frame |
| first instruction of `task_a` | CONTROL_S `0x2`, BASEPRI_S `0x0`, PSPLIM_S = stack base | Thread on PSP, privileged, no FP context; kernel mask released; hardware stack limit armed per task |
| `g_assert_line` | 0 | handler-installation asserts passed |

Free run: VCOM `Hello` / `IRQ 39 taken` / `Scheduler start`; LED toggles every 500 ms (`vTaskDelay`).

## 3. Two tasks and a queue: event switch vs tick switch (6.2)

Task A (priority 1) sends its loop count into a static 4-deep queue, spins 3 ticks, then
`vTaskDelay(500)`. Task B blocks on `xQueueReceive(portMAX_DELAY)` and prints `B got N dt=T`,
where `dt` = ticks between A's send and B's receive.

| | Run 1: B priority 2 (above A) | Run 2: B priority 1 (equal to A) |
|---|---|---|
| UART | every line `dt=0` | every line `dt=1` |
| Where A was preempted (A's saved PC) | `vClearInterruptMask+8` (0x1E4E), LR = `vPortExitCritical+57` | `task_a+70` (0x1B5E), just back from `xTaskGetTickCount` |
| Why | the send unblocks a higher-priority task → `xQueueGenericSend` pends PendSV **inside its critical section** (BASEPRI 0x20 masks PendSV at 0xE0); it is taken the instant `vClearInterruptMask` writes BASEPRI 0 | equal priority: the send does not yield; the next SysTick time-slices to B (`configUSE_TIME_SLICING 1`) |
| A's loop period | exactly **504** ticks (1,165,248 / 2,312) | exactly **503** ticks (17,102 / 34) |

The extra tick in run 1: B's ~17-character print runs before A reads `t0`, so A's 3-tick spin
starts one tick late (1 + 3 + 500). A higher-priority task's run time stretches a lower-priority
loop. No item was lost: `g_cnt_a = g_cnt_b` in both runs.

A's saved context in its TCB (`pxTopOfStack`, both runs): PSPLIM, EXC_RETURN `0xFFFF_FFFD`,
R4–R11 (port fill values untouched except R7 = frame pointer), then the 8-word hardware frame.

## 4. Inside the FreeRTOS switch: what it costs (6.3)

Method (same as Phase 5): DWT CYCCNT read at the PendSV entry breakpoint and at the incoming
task's first resumed instruction (breakpoint at its saved PC); both halts excluded by CYCCNT
stopping in Debug state.

| | Our Phase 5 switcher | FreeRTOS V11.2.0 port (-O0) |
|---|---|---|
| Cycles, PendSV entry → incoming task | **70** | **250** (A→B) / **252** (B→A) |
| Software-saved words | 8 (R4–R11) | 10 (PSPLIM, EXC_RETURN, R4–R11) + S16–S31 only if the task used FP |
| Bytes per switched-out task (with the 8-word hardware frame) | 64 B | 72 B |
| Choosing the next task | compare with the other TCB (2 tasks) | `vTaskSwitchContext` in C, CLZ-based ready-list lookup (`configUSE_PORT_OPTIMISED_TASK_SELECTION`) |
| Interrupt masking during the switch | none | BASEPRI = 0x20 with `dsb`/`isb` around the C call |
| Per-task stack limit | none | PSPLIM loaded per task |

The extra ≈180 cycles are the -O0 C call to `vTaskSwitchContext` (0xC4 B) plus the BASEPRI
raise/clear with barriers — the assembly saves only two more words than ours. (The split between
the two was not measured.)

The two switch-outs also showed two different take points for a pended PendSV: B's saved PC
`0x1A0` is the `dsb` in `vPortYield` (taken after the ICSR store, before the `dsb`); A's is
`0x1E4E`, the `isb` in `vClearInterruptMask` (after the `dsb`).

## 5. Stack use and PSPLIM (6.3)

Stacks are filled with `0xA5` at creation; a scan up from the base counts untouched words.

| Task | Stack | Used | Free | Lowest touched word |
|---|---|---|---|---|
| A | 1024 B | 160 B | 864 B | `0x200084B8` = A's saved `pxTopOfStack` |
| B | 1024 B | 156 B | 868 B | `0x20008900` = B's saved `pxTopOfStack` |
| idle | 512 B | 100 B | 412 B | — |

- The deepest point of a task's stack is **the switch-out instant** (10 + 8 saved words on top of
  its deepest call), not its deepest `.su` call chain.
- `.su` + 72 B is not a safe bound: idle measured 100 B against a `.su` estimate of 92 B
  (4 B alignment pad on exception entry + a top word that is never written but counted).
- PSPLIM reads back 8-byte aligned (DDI0553 D1.2.218: LIMIT is bits [31:3], [2:0] RES0). A stack
  base that is only 4-aligned (B `0x2000859C`, idle `0x20008A3C`) becomes a limit 4 B lower
  (`0x20008598`, `0x20008A38`) — on the last word of the task's own TCB. Since SP may descend
  *to* the limit without a fault (DDI0553 B3.21 IKDPG), a one-word overflow would overwrite that
  TCB word before the check fires (inference, not tested).

## 6. Still open

- FP path of the FreeRTOS PendSV (`vstmdbeq`/`vldmiaeq` S16–S31) never run on hardware — the
  planned FP task (6.4) was skipped for time.
- Split of the ≈180 extra cycles between `vTaskSwitchContext` and the BASEPRI/barrier writes.
- Why one pended PendSV was taken before a `dsb` and the other after — no architectural source quoted.

## Reproduce

`git checkout phase-6`, Project → Clean, build `Debug` (FLASH 9028 B, RAMB 3136 B). Breakpoint
addresses for this image (from `nm`): `PendSV_Handler` 0x574, `vPortYield` 0x194,
`vClearInterruptMask` 0x1E46, `vTaskSwitchContext` 0x141C, `pxCurrentTCB` 0x2000800C.
