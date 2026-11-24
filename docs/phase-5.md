# Phase 5 — A two-task context switcher of our own

**Tag:** [`phase-5`](https://github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236/tree/phase-5) (2a202a9) ·
**Log:** `learning_log.md`, blocks "5.0" … "Phase 5 closed" (sessions 24–25, 16–19 Nov 2026)

Before using an RTOS, the same job was done by hand: two tasks, each on its own process stack
(PSP), switched by a PendSV handler written in assembly, first on demand and then every 100
SysTick ticks. Everything an RTOS port does at a context switch is visible here in 16
instructions — and every frame, register and cycle count below was read on the board.

## 1. Design (5.0)

| Item | Value |
|---|---|
| Stacks | A `0x2000_8C00–0x2000_8FFF` (top `0x2000_9000`), B `0x2000_9C00–0x2000_9FFF` (top `0x2000_A000`) |
| TCB | `{ uint32_t * volatile sp; const char *name; }` — 8 B, saved PSP at offset 0 |
| Initial frame | 16 words below the top: R4–R11 (+0…+28), then the hardware frame R0, R1, R2, R3, R12, LR, ReturnAddress, RETPSR (+32…+60) |
| Frame contents | registers tagged `0xA000_00nn` / `0xB000_00nn` (visible in a dump), LR = `task_exit_trap`, ReturnAddress = task entry, RETPSR `0x0100_0000` (T bit only) |
| Return value used | EXC_RETURN `0xFFFF_FFFD` = Secure, Thread mode, PSP, standard frame (no FP in tasks) |
| PendSV priority | lowest (level 7, `0xE0`), so it never preempts another handler |

## 2. The switch ([`startup_m33.S` line 127 at the tag](https://github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236/blob/phase-5/source/startup_m33.S#L127))

```asm
PendSV_Handler:
    ldr     r1, =g_cur_tcb       /* r1 = &g_cur_tcb */
    ldr     r2, [r1]             /* r2 = running TCB; 0 on the first entry (.bss) */
    cbz     r2, 1f               /* first entry: no task context to save */
    mrs     r0, psp              /* outgoing task's PSP */
    stmdb   r0!, {r4-r11}        /* save the callee-saved registers below the hardware frame */
    str     r0, [r2]             /* cur->sp */
1:  ldr     r3, =g_tcb_a
    cmp     r2, r3
    bne     2f                   /* cur = 0 or B -> next = A */
    ldr     r3, =g_tcb_b         /* cur = A -> next = B */
2:  str     r3, [r1]             /* g_cur_tcb = next */
    ldr     r0, [r3]             /* next->sp */
    ldmia   r0!, {r4-r11}        /* R4..R11 back; r0 -> hardware frame */
    msr     psp, r0
    ldr     lr, =0xFFFFFFFD      /* Thread, PSP, standard frame */
    bx      lr                   /* exception return pops the 8-word frame from PSP */
```

The hardware stacks R0–R3, R12, LR, ReturnAddress and xPSR on exception entry; the handler only
has to save R4–R11. `r0` walks the stack; PSP itself changes only at `msr`.

## 3. First start (5.2)

`main` pends PendSV once (`ICSR.PENDSVSET`). Measured at the PendSV entry:

- LR `0xFFFF_FFE9` (entry from Thread on **MSP** with FPCA 1 — main had used the FPU) and a
  26-word frame `0x2000_FFF0 → 0x2000_FF88`. The handler overwrites LR with `0xFFFF_FFFD`, so this
  frame is **never popped**: main's MSP stack is abandoned (FreeRTOS reclaims it explicitly — see Phase 6).
- Stacked ReturnAddress = the `isb` after the pend: PendSV was taken after the `dsb`, before the `isb`.
- After the switch, task A runs with CONTROL_S `0x2` (PSP, privileged), PSP `0x2000_9000`.
- Lazy FP state was left behind: FPCCR.LSPACT stays 1 and FPCAR still points into main's dead
  frame (`0x2000_FFA8`) — a trap for any task that would use the FPU (not fixed in Phase 5).

## 4. Time slicing (5.3a)

SysTick counts 100 ticks, then sets PENDSVSET. Measured:

| Item | Value |
|---|---|
| How PendSV is entered | **tail-chained** from SysTick: no unstack/restack, LR `0xFFFF_FFFD` = the task's own EXC_RETURN |
| A's frame | `0x2000_8FD8`, RETPSR.SPREALIGN 1 — A's loop sp `0x2000_8FFC` was 4 mod 8, the core first aligned it to `0x8FF8` |
| B's frame | `0x2000_9FD8`, SPREALIGN 0 (sp already 8-aligned) |
| Saved PSPs | A `0x2000_8FB8`, B `0x2000_9FB8` (= frame − 0x20 for R4–R11) |
| Switch cost | **70 cycles** (CYCCNT, PendSV entry → incoming task's first instruction) |
| Slice | 4,800,954 cycles = 100 × 48,000 + halt jitter |
| Board | LED (task A) 5 blinks / 10 s, VCOM `Task B active` (task B) |

## 5. Breaking into a switch on purpose (5.3b)

With the debugger, SysTick was forced to fire **inside** PendSV, after A's registers were saved
but before the current-TCB pointer changed:

- SysTick nested on MSP (`0x2000_FF88 → 0x2000_FF68`, EXC_RETURN `0xFFFF_FFF1`, SHCSR
  `0x0007_0C00` = SysTick and PendSV both active). It touched no switch state, and the switch
  completed exactly as in 5.3a — proof that this switcher needs no critical section.
- With the slice counter at 99, the nested SysTick pended PendSV **while PendSV was active**
  (ICSR `0x1000_E80E`: PENDSVSET + VECTPENDING 14). PendSV's `bx lr` then tail-chained into a
  second PendSV, which re-saved A unchanged and switched straight back to B: correct, but A
  lost its slice — the cost of a double switch.

## 6. Still open

- FP in tasks (5.4) was deferred: the lazy-FP state left by the first start (LSPACT 1, FPCAR into
  main's frame) is untested with a task that uses the FPU.
- No architectural rule was found that states the LR value on a tail-chain (measured: the
  background EXC_RETURN).

Phase 6 replaces this switcher with the FreeRTOS port and compares the two:
[phase-6.md](phase-6.md).
