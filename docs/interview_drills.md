# Interview drill bank — Cortex-M33 + FreeRTOS on the FRDM-MCXN236

36 questions drawn from the steps in [`learning_log.md`](../learning_log.md). Every answer uses a
value measured on this board and names the step it came from (for example "2.4" or "6.2 run 1"),
so each one can be checked in the log. Each answer ends with a one-line version for an
interviewer who doesn't do firmware.

**How to drill:** read the question, answer aloud or on paper, then open the answer. ★ marks
questions answered wrongly in a phase self-test — drill those first.

| Group | Topic | Write-up |
|---|---|---|
| A | Boot and memory | [phase-0-1.md](phase-0-1.md) |
| B | Exceptions and the NVIC | [phase-2.md](phase-2.md) |
| C | Faults | [phase-3.md](phase-3.md) |
| D | Modes, privilege, SVC | [phase-4.md](phase-4.md) |
| E | Context switching | [phase-5.md](phase-5.md) |
| F | FreeRTOS on the M33 | [phase-6.md](phase-6.md) |

## A. Boot and memory

**A1. What does the core read first after reset, and how did you prove the boot ROM handed control to your code?**

<details><summary>Answer</summary>

Word 0 of the vector table → MSP (`0x2001_0000` = `__stack_top`), word 1 → the reset vector (`0x49` = `Reset_Handler` 0x48 + Thumb bit). Words 8/9/10/13 must be 0 for a plain image (RM Table 158).
Proof (1a.5b): breakpoint on `Reset_Handler` after a real reset → MSP `0x2001_0000`, LR `0x1302_D94D` = inside the Secure boot ROM, so the ROM *called* us with BLX.

*Plain words:* The chip's built-in boot program reads two numbers from the start of my flash — where my stack is and where my code starts — and jumps there.

</details>

**A2. Your first linked image was 0 bytes. Why, and what fixed it?**

<details><summary>Answer</summary>

`--gc-sections` keeps only what is reachable from `ENTRY` and `KEEP()`. With neither, `main` was discarded (map: `.text.main 0x8` under discarded sections). `ENTRY(Reset_Handler)` + `KEEP(*(.isr_vector))` + `bl main` in `Reset_Handler` → 80 B image (1a.3, 1a.4). The linker has no special "main".

*Plain words:* The linker throws away anything nobody calls — and at first nothing called anything.

</details>

**A3. Why does `Reset_Handler` copy `.data` and zero `.bss`?**

<details><summary>Answer</summary>

RAM is not zero at reset: `g_zero` read `0x0000_000E` before the zero loop (1b.3). Initialised globals live in flash (LMA) and must be copied to RAM (VMA): `.data > RAMB AT > FLASH`. Without these rules the globals became orphan sections placed in flash, so a store would have targeted flash (1b.1).

*Plain words:* Memory wakes up full of junk; C promises zeros and initial values, so the startup code sets them before `main` runs.

</details>

**A4. What happens when the first floating-point instruction runs with the FPU disabled, and what is the fix?**

<details><summary>Answer</summary>

NOCP UsageFault — CFSR `0x0008_0000` (UFSR bit 3); UsageFault was disabled, so it escalated: HardFault, HFSR `0x4000_0000` FORCED, EXC_RETURN `0xFFFF_FFF9`, 8-word frame. The stacked PC was the `vldr` — the constant itself had been moved as an integer, so the fault came only at the first real VFP instruction (1b.4a).
Fix: `CPACR |= 0xF << 20` (CP10/CP11 full access), `dsb`, `isb` before any C (1b.4b).

*Plain words:* The maths co-processor is switched off at power-up; the first time code uses it the chip raises an alarm, so the startup code switches it on first.

</details>

**A5. What does bit 0 of a code address mean on Cortex-M, and what happens if it is 0?**

<details><summary>Answer</summary>

It selects Thumb state, the only state Cortex-M has. Vector words and function pointers carry it (`0x49`, `0x5C5`). A `blx` to an even address clears EPSR.T → INVSTATE UsageFault at the target: CFSR `0x0002_0000`, stacked PC = the even target `0xD4`, escalated to HardFault (1c).

*Plain words:* Every code address on this CPU carries a flag that says "compact instruction set"; drop it and the CPU refuses to run the code.

</details>

**A6. A GPIO register reads 0 but the LED is still lit. How?**

<details><summary>Answer</summary>

Gated-read behaviour is per peripheral: GPIO4 with its clock gated reads 0 silently, keeps its contents and the pin keeps driving; PORT4 gated gives "Cannot access memory" (1d.1, 1d.2g). Also the boot ROM changes registers before `main` (AHBCLKCTRL0 `0x7FF` vs RM reset `0x603`), so read a register at `main` before trusting its documented reset value.

*Plain words:* If a block's clock is off, asking it a question can just return zero — silence is not the truth.

</details>

## B. Exceptions and the NVIC

**B1. How many interrupt priority bits does this chip have, and how did you find out?**

<details><summary>Answer</summary>

Write `0xFF` to IRQ 39's priority byte (`0xE000E427`) and read back `0xE0`: bits 7:5 implemented, 4:0 read-as-zero → 3 bits, 8 levels; level L is written as `L << 5` (level 5 = `0xA0`) (2.2a/b). AIRCR PRIGROUP 0 → all three bits are preemption priority.

*Plain words:* I wrote all ones into the priority setting and read back which bits stuck.

</details>

**B2. ★ Decode EXC_RETURN `0xFFFF_FFE9`, `0xFFFF_FFED`, `0xFFFF_FFF1` and `0xFFFF_FFFD`.**

<details><summary>Answer</summary>

Low byte, bit by bit: b0 ES (1 = exception taken to Secure), b1 RES0, b2 SPSEL (0 MSP / 1 PSP), b3 Mode (1 Thread / 0 Handler), b4 FType (1 standard / 0 extended FP frame), b5 DCRS (1 = default stacking rules), b6 S (1 = frame on the Secure stack), b7 1.

| Value | Low byte | Meaning | Seen in |
|---|---|---|---|
| `0xFFFF_FFE9` | 1110_1001 | Thread, MSP, extended frame | 2.4 SysTick from `main` |
| `0xFFFF_FFED` | 1110_1101 | Thread, PSP, extended frame | 4.1, 4.3 |
| `0xFFFF_FFF1` | 1111_0001 | Handler, MSP, standard frame | 2.5c nesting |
| `0xFFFF_FFFD` | 1111_1101 | Thread, PSP, standard frame | 5.3a, every FreeRTOS task |

*Plain words:* When an interrupt starts, the CPU leaves itself a short note in a register saying where to go back to and how much it saved.

</details>

**B3. Why was the SysTick frame 26 words instead of 8, and what is lazy stacking?**

<details><summary>Answer</summary>

`main` had used the FPU (CONTROL.FPCA 1) → extended frame: 8 State words (R0–R3, R12, LR, ReturnAddress, RETPSR) + 18 FP words (S0–S15, FPSCR, reserved) = 104 B, `0x2000_FFF0 → 0x2000_FF88` (2.4). With FPCCR.LSPEN 1 the FP space is only reserved (LSPACT 1, slot held stale data) and is filled only if the handler executes an FP instruction. Entry clears FPCA, so an interrupt nesting on top stacks only 8 words (2.5c-1b).

*Plain words:* The CPU books space for the maths registers on every interrupt, but only actually saves them if the interrupt code does maths.

</details>

**B4. ★ Read ICSR `0x0000_080F` and `0x00C3_780F`.**

<details><summary>Answer</summary>

Fields: VECTACTIVE [8:0], RETTOBASE 11, VECTPENDING [20:12], ISRPENDING 22, ISRPREEMPT 23.
- `0x0000_080F`: SysTick (15) active, RETTOBASE 1 (only one active), nothing pending (2.4).
- `0x00C3_780F` = `0x80_0000` + `0x40_0000` + (55 << 12 = `0x3_7000`) + `0x800` + `0xF`: SysTick active, RETTOBASE 1, IRQ 39 (exception 55) pending and enabled, ISRPREEMPT 1 = it will preempt (2.5c-1).

*Plain words:* One status register tells you which interrupt is running, which is waiting, and whether the waiting one is going to barge in.

</details>

**B5. Preempt, wait, or tail-chain — what did you measure?**

<details><summary>Answer</summary>

With SysTick (level 4) active: IRQ 39 at level 0 **preempts** — nested 8-word frame on MSP `0xFF88 → 0xFF68`, LR `0xFFFF_FFF1`, RETTOBASE 0 (2.5c). At level 5 it **waits** and is taken after SysTick's body without returning to Thread: 23 cycles from SysTick's epilogue to IRQ 39's first instruction, against a minimum of 21 + 17 − 6 = 32 for return-then-new-entry → **tail-chained** (2.6, 2.6b).

*Plain words:* An urgent interrupt can cut in; a less urgent one waits its turn, and the CPU goes straight from one handler to the next without unpacking in between.

</details>

**B6. ★ What is SPREALIGN, and where did you see it?**

<details><summary>Answer</summary>

The exception frame must start 8-byte aligned. If SP is 4 mod 8 at entry, the core first subtracts 4 and sets RETPSR bit 9 (SPREALIGN); the return adds it back. 5.3a: task A's loop SP `0x2000_8FFC` → `0x8FF8` → frame `0x2000_8FD8`, RETPSR `0x2100_0200` (bit 9 set); task B's SP `0x9FF8` was aligned → frame `0x9FD8`, SPREALIGN 0. In 6.3 the same pad made B's stack use 156 B instead of 152.

*Plain words:* The CPU insists its save area starts on an 8-byte boundary, pads by one word if needed, and notes the pad so it can undo it.

</details>

## C. Faults

**C1. Decode CFSR `0x0200_0000`, `0x0100_0000`, `0x0010_0000`, `0x0000_8200`, `0x0000_0082` and `0x0000_0011`.**

<details><summary>Answer</summary>

Layout: MMFSR [7:0], BFSR [15:8], UFSR [31:16].
- `0x0200_0000` UFSR bit 9 DIVBYZERO (3.1b) · `0x0100_0000` UFSR bit 8 UNALIGNED (3.2b) · `0x0010_0000` UFSR bit 4 STKOF (3.3)
- `0x0000_8200` BFSR BFARVALID + PRECISERR (3.4)
- `0x0000_0082` MMFSR MMFARVALID + DACCVIOL (3.5) · `0x0000_0011` MMFSR MSTKERR + IACCVIOL (4.2)

*Plain words:* One fault register is three panels of warning lights, and each light names the exact mistake.

</details>

**C2. Why did a divide-by-zero become a HardFault, and how did you get a UsageFault instead?**

<details><summary>Answer</summary>

Trap off (CCR.DIV_0_TRP 0): `udiv` by 0 returns 0, no fault (3.1a). Trap on but UsageFault disabled → escalation: IPSR 3, HFSR `0x4000_0000` FORCED, CFSR `0x0200_0000` kept, stacked RA = the `udiv` (3.1b). Set SHCSR.USGFAULTENA (read-modify-write, then `dsb` + `isb`) → IPSR 6, HFSR 0, SHCSR `0x0004_0008` (3.1c–d).

*Plain words:* If the specialist alarm is switched off, the general alarm rings instead — and it says "forwarded".

</details>

**C3. How do you know which instruction caused a fault?**

<details><summary>Answer</summary>

Read the stacked ReturnAddress at frame + `0x18`. For a synchronous (precise) fault it is the faulting instruction itself: the `udiv` (3.1b), the `ldr` (3.2b), the `str` at `0x280` (3.4) and at `0x2F4` (3.5). For an asynchronous interrupt it is the next instruction to execute (2.5d-2: `0x226`, the `isb` after the `dsb`).

*Plain words:* The CPU saves the address it was about to run, so after a crash it points straight at the culprit.

</details>

**C4. Can you trust BFAR and MMFAR?**

<details><summary>Answer</summary>

Only when BFARVALID (CFSR bit 15) or MMFARVALID (bit 7) is set; otherwise the content is UNKNOWN. With VALID 0 they held leftovers of recent data accesses (`0x2000_FFFC` at `main`, the literal address `0x320` before the 3.4 store). MMFAR and BFAR read the same in every state → one shared register on this core (inference; DDI0553 allows it). Both handlers check the bit before saving the address (3.4, 3.5).

*Plain words:* The "where did it happen" register only means something when its "valid" flag is on.

</details>

**C5. What happens on a stack overflow with MSPLIM?**

<details><summary>Answer</summary>

3.3: MSPLIM_S = `0x2000_FFE0`, then a push below it → STKOF UsageFault, CFSR `0x0010_0000`, **SP set to the limit**, no frame word written below it. The handler's own `push` then faulted too → escalated to HardFault (FORCED, LR `0xFFFF_FFF1`), no lockup. SP may reach the limit; only going below it faults.

*Plain words:* A hardware fence stops the stack before it tramples other memory — the CPU stops at the fence line.

</details>

**C6. How did you make an MPU write fault, and how did you prove the write never happened?**

<details><summary>Answer</summary>

Region 0 = `0x2000_8040–805F`, privileged read-only (RBAR `0x2000_8045`, RLAR `0x2000_8041`, MAIR0 `0x44`, MPU_CTRL 5 = PRIVDEFENA + ENABLE), MEMFAULTENA 1 → the `str` gave MemManage: IPSR 4, CFSR `0x82`, MMFAR `0x2000_8040`, RA = the `str` (3.5). Then the debugger wrote `0x1234_5678` there (debug writes are not MPU-checked); the pattern survived the faulting store (3.5a).

*Plain words:* I marked one memory area read-only, tried to write it, and showed the CPU blocked the write before it happened.

</details>

## D. Modes, privilege, SVC

**D1. MSP and PSP: who uses which, and how do you switch Thread mode to PSP?**

<details><summary>Answer</summary>

After reset Thread mode uses MSP; Handler mode always uses MSP. `msr psp, 0x2000_B000`, CONTROL read-modify-write SPSEL 1 (`0xC → 0xE`), `isb` → `$sp` = PSP. A fault from Thread then stacked on PSP (`0x2000_B000 → 0x2000_AF98`), EXC_RETURN `0xFFFF_FFED`, handler on MSP `0x2000_FFF0` (4.1).

*Plain words:* Tasks get their own scratch-pad stacks; the interrupt system keeps a separate one of its own.

</details>

**D2. ★ What do exception entry and return do to CONTROL?**

<details><summary>Answer</summary>

Entry clears SPSEL, FPCA and SFPA and keeps nPRIV. Return sets SPSEL from EXC_RETURN bit 2, FPCA = NOT FType, SFPA from RETPSR bit 20; nPRIV changes only by an `msr`. Measured in 4.3: CONTROL_S `0xF` at the `svc` → `0x1` in the handler → `0x0` after its `msr` + `isb` → `0xE` back in Thread (only the nPRIV change carried across).

*Plain words:* On the way into an interrupt the CPU resets most mode flags but keeps "is this code trusted"; on the way out it restores them from the note it left.

</details>

**D3. Unprivileged code, one MPU region, PRIVDEFENA 1 — what happens?**

<details><summary>Answer</summary>

The background memory map is privileged-only and no region covers flash, so the very next instruction fetch faults: CFSR `0x11` = IACCVIOL + MSTKERR, MMFARVALID 0. Stacking for an unprivileged context is itself checked as unprivileged → no frame word was written, yet PSP still moved to `0x2000_AF98`. CONTROL_S read `0x1` in the handler; HFSR 0, no lockup (4.2).

*Plain words:* Once I made the code untrusted, the memory guard didn't even let it read its own next instruction.

</details>

**D4. How does an SVC handler find the SVC number?**

<details><summary>Answer</summary>

EXC_RETURN bit 2 says which stack holds the frame (`tst lr, #4` → MSP or PSP). ReturnAddress at + `0x18` is the instruction *after* the `svc` (`0x388` for the `svc` at `0x386`); the immediate is the byte at RA − 2 (Thumb T1 encoding, little-endian) → 0. Measured: IPSR 11, SHCSR `0x0007_0080` (SVCALLACT bit 7) (4.3).

*Plain words:* The "system call" instruction carries a small number; the handler looks back at that instruction to read it.

</details>

**D5. Why the `isb` after writing CONTROL, and the `dsb` + `isb` after an NVIC or SCB write?**

<details><summary>Answer</summary>

A CONTROL write applies to later instructions only after a context synchronization event (`isb`). For System Control Space writes, `dsb` makes the write complete and a context synchronization event makes its effects visible (DDI0553 B7.2.16). Measured twice: a pend written from `main` was taken after the `dsb`, before the `isb` (2.5d-2 RA `0x226`; 5.2 RA `0x430`).

*Plain words:* The CPU has instructions already queued up; a barrier flushes the queue so the new setting applies from the very next instruction.

</details>

**D6. Which security state does your firmware run in, and how do you know?**

<details><summary>Answer</summary>

Secure. At `main` `msp_s` is the active stack and `msp_ns` is 0 (1a.5a); with the SAU off every address is attributed Secure; every EXC_RETURN measured has ES 1 and S 1. FreeRTOS was configured to match: `configRUN_FREERTOS_SECURE_ONLY 1`, initial EXC_RETURN `0xFFFF_FFFD`. Splitting into Secure and Non-secure projects was left for later.

*Plain words:* The chip has a trusted and an untrusted world; with no partition set up, everything runs in the trusted one.

</details>

## E. Context switching

**E1. Why do the context switch in PendSV, and why at the lowest priority?**

<details><summary>Answer</summary>

PendSV is pended by software (ICSR.PENDSVSET bit 28) and runs only when nothing more urgent is active, so a switch never interrupts another handler halfway. SysTick just pends it and returns; PendSV then tail-chains in (5.3a: LR `0xFFFF_FFFD`, SHCSR shows PendSV active only). Lowest level = `0xE0`.

*Plain words:* The task switch is a deferred chore that only runs when nothing urgent is going on.

</details>

**E2. In a context switch, what does the hardware save and what does your code save?**

<details><summary>Answer</summary>

Hardware: R0–R3, R12, LR, ReturnAddress, xPSR (8 words; +18 FP words if the task used the FPU). Our PendSV: `mrs r0, psp`, `stmdb r0!, {r4-r11}`, store r0 in the TCB, pick the next TCB, `ldmia r0!, {r4-r11}`, `msr psp, r0`, return with `0xFFFF_FFFD` — 16 instructions, 64 B per switched-out task. FreeRTOS also saves PSPLIM and EXC_RETURN (72 B).

*Plain words:* The CPU automatically packs half the registers; the switcher packs the other half.

</details>

**E3. How do you start the very first task?**

<details><summary>Answer</summary>

Build a fake saved frame and "return" into it: 16 words — R4–R11, then R0–R3, R12, LR = an exit trap, ReturnAddress = the task entry, RETPSR `0x0100_0000` (T bit). Ours: `main` pends PendSV, which skips the save on the first entry (`cbz`); entry LR `0xFFFF_FFE9`, so main's 26-word MSP frame is never popped (5.2). FreeRTOS: `svc` from `vStartFirstTask`, after resetting MSP to `0x2001_0000` (from `0x2000_FFB8`) to reclaim main's stack (6.1).

*Plain words:* You can't return to a task that has never run, so you forge the saved state it would have had.

</details>

**E4. Is it safe if SysTick interrupts PendSV in the middle of a switch?**

<details><summary>Answer</summary>

In our switcher, yes — forced with the debugger in 5.3b: SysTick nested on MSP (`0xFF88 → 0xFF68`, EXC_RETURN `0xFFFF_FFF1`), touched no switch state, and the switch completed identically. If that SysTick pends PendSV again, PendSV is active-and-pending (ICSR `0x1000_E80E`) and tail-chains into a second switch — correct but wasteful. FreeRTOS instead masks with BASEPRI `0x20` around `vTaskSwitchContext`.

*Plain words:* A timer tick during a switch is harmless here — it uses a different stack and never touches the switcher's bookkeeping.

</details>

**E5. Your switch takes 70 cycles and FreeRTOS's 250. Why?**

<details><summary>Answer</summary>

Same method for both: CYCCNT from PendSV entry to the incoming task's first instruction. FreeRTOS adds a C call to `vTaskSwitchContext` (priority-based ready-list lookup, compiled at -O0), a BASEPRI raise/clear with `dsb`/`isb`, and two more saved words (PSPLIM, EXC_RETURN). How the extra ≈180 cycles split was not measured. At 48 MHz: about 1.5 µs vs 5.2 µs (5.3a, 6.3).

*Plain words:* Mine is a hard-wired two-way swap; FreeRTOS does real work choosing the next task by priority, which costs about 3.6 times as long.

</details>

**E6. What would a task using the FPU cost per switch? (derived, not run)**

<details><summary>Answer</summary>

The hardware frame becomes 26 words (FType 0), and the FreeRTOS PendSV (`tst lr, #0x10`) also saves S16–S31 (16 words). Derived for task B with one FP instruction: EXC_RETURN `0xFFFF_FFED`, saved-PC offset + `0x80` instead of + `0x40`. Not run on hardware (6.4 skipped). In our own switcher a hidden trap remains: after the first start FPCCR.LSPACT stays 1 and FPCAR points into main's dead frame (`0x2000_FFA8`, 5.2).

*Plain words:* If a task uses floating point, every switch carries an extra block of maths registers — I worked the numbers out but didn't run it.

</details>

## F. FreeRTOS on the M33

**F1. What is `configMAX_SYSCALL_INTERRUPT_PRIORITY`, and why `0x20`?**

<details><summary>Answer</summary>

The BASEPRI value used in kernel critical sections. Interrupts at numeric priority ≥ `0x20` (levels 1–7) are masked during them and may call `…FromISR` APIs; level 0 is never masked and must not call the kernel (assert at port.c 2132). With 3 bits, `0x20` = level 1; the port asserts it survives the `0xE0` mask (port.c 1768). Our IRQ 39 sits at level 0.

*Plain words:* It is a priority line: interrupts below it may talk to the OS and get briefly held off; interrupts above it are never delayed but must not use the OS.

</details>

**F2. When `xQueueSend` wakes a higher-priority task, where exactly is the sender preempted?**

<details><summary>Answer</summary>

Task A's saved PC was `0x1E4E` = `vClearInterruptMask+8` (LR `vPortExitCritical+57`): the send pends PendSV inside its critical section, where BASEPRI `0x20` masks PendSV (level 7, `0xE0`); PendSV is taken the instant BASEPRI is written back to 0. Task B then printed `dt=0` — same tick (6.2 run 1).

*Plain words:* The OS notes "a switch is needed" inside a protected section, and the switch fires the moment the protection lifts.

</details>

**F3. With equal priorities, why is `dt` 1, and why did the loop period change from 504 to 503 ticks?**

<details><summary>Answer</summary>

An equal-priority wake-up does not yield; with time slicing on, B gets the CPU at the next SysTick — A's saved PC was `task_a+70`, `dt=1` (6.2 run 2). Run 2's period is exactly 503 ticks (3-tick spin + 500-tick delay). In run 1 it was 504: higher-priority B printed its line before A read its start tick, delaying A's spin by one tick.

*Plain words:* Two equally important tasks take turns at each clock tick; a more important one jumps in at once, and its work pushes the other one's timing back.

</details>

**F4. ★ What does PSPLIM do in the FreeRTOS port, and what surprise did you find?**

<details><summary>Answer</summary>

Each task's hardware stack limit, loaded by PendSV. Its bits [2:0] are RES0, so the limit is 8-byte aligned: B's stack base `0x2000_859C` gives limit `0x2000_8598`, the idle task's `0x2000_8A3C` gives `0x2000_8A38` — the last word of the task's own TCB. A's base `0x2000_8158` was already aligned. Because SP may reach the limit without a fault, a one-word overflow would overwrite that TCB word first (inference, not tested).

*Plain words:* Each task has a fence at the bottom of its stack, but the fence snaps to an 8-byte grid and can sit one word too low.

</details>

**F5. How did you measure stack high-water marks, and where is a task's deepest point?**

<details><summary>Answer</summary>

The kernel fills every stack with `0xA5` at creation; a debugger scan up from the base counts untouched words: A 160 / B 156 / idle 100 B used (of 1024 / 1024 / 512). The lowest touched word equalled each task's saved `pxTopOfStack` → the deepest moment is the switch-out (deepest call + 10 software + 8 hardware words) (6.3).

*Plain words:* Paint the stack with a known pattern, run the program, and see how far down the paint got scratched.

</details>

**F6. Is the compiler's `.su` stack estimate a safe bound?**

<details><summary>Answer</summary>

No. Idle measured 100 B against `.su` + 72 B (switch context) = 92 B: the 4 B alignment pad at exception entry and a never-written top word are not in `.su`, and neither are interrupt frames or FP context. Use a measured value plus margin (6.3).

*Plain words:* The compiler's estimate missed a few bytes that the hardware adds, so I trust measurement plus a margin.

</details>
