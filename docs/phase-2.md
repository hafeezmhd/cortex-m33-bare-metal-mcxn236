# Phase 2 — Exceptions and the NVIC, measured

**Log:** `learning_log.md`, blocks "2.0" … "Phase 2 self-test GDB check" (sessions 8–15, 26 Oct–3 Nov 2026) ·
**Code:** the IRQ 39 handler, vector slots 15/55 and the weak aliases are still in today's sources (no separate tag)

The question for this phase: what exactly does the core do when an interrupt arrives, nests,
or follows another one? Each behaviour was set up with our own register macros (named after the
DDI0553 registers, not CMSIS) or a single GDB write, then read back at breakpoints — xPSR,
EXC_RETURN, the stacked frame, ICSR, SHCSR, the NVIC bits — and, where the question was about
time, counted with the DWT cycle counter.

## 1. Starting state and priority bits (2.1, 2.2)

| Register at `main` | Value | Meaning |
|---|---|---|
| VTOR | `0x0000_0000` | our vector table in flash |
| AIRCR | `0xFA05_0000` | PRIGROUP 0 → with 3 priority bits every bit is preemption priority |
| SHPR1–3, ICSR | 0 | all system handlers at level 0; Thread mode, nothing pending |
| SYST_CSR | 0 | SysTick off; CLKSOURCE 0 = the chip's reference clock |

Writing `0xFF` to the priority byte of IRQ 39 (IPR9 byte `0xE000E427`) read back **`0xE0`**: 3
implemented bits [7:5], [4:0] RAZ/WI. So level *L* is written as `L << 5` (level 5 = `0xA0`,
confirmed). 3 bits = 8 levels, as RM 3.3.1 states.

## 2. SysTick: polled, then interrupt-driven (2.3, 2.4)

- **Polled (2.3b):** RVR = 47,999 (period = RELOAD + 1, DDI0553 B11.1), CLKSOURCE 1 (48 MHz core
  clock). Two COUNTFLAG waits measured **48,005** cycles apart (48,000 + up to one poll iteration
  of detection jitter).
- **Interrupt (2.4):** own C `SysTick_Handler` overrides a `.weak` alias in our vector table
  (slot 15 = handler address | 1). At the handler's first instruction:

| Item | Value | Meaning |
|---|---|---|
| xPSR | `0x0900_000F` | IPSR 15 = SysTick (Q and T set) |
| ICSR | `0x0000_080F` | VECTACTIVE 15, RETTOBASE 1 (only one active) |
| LR | `0xFFFF_FFE9` | EXC_RETURN: Secure, Thread, MSP, **extended frame** (FType 0) |
| sp | `0x2000_FFF0 → 0x2000_FF88` | **26 words** = 8 State + 18 reserved FP words (main had used the FPU) |
| FPCCR | `0xC000_001D` | LSPACT 1: FP space reserved but **not written** (lazy stacking) — stale stack data in the slot |
| Stacked RETPSR | `0x0910_0000` | T, Q, and bit 20 = SFPA |

The LED blinks 10 times per 10 s (toggle every 500 ticks).

## 3. An external interrupt: IRQ 39 (2.5a, 2.5b)

The vector table was extended from 16 to 56 words so slot 55 (`0xDC`) holds our
`LP_FLEXCOMM4_IRQHandler` (IRQ 39 = exception 16 + 39; RM Table 15). The UART's own interrupt
enables are all 0, so only software can pend it.

| GDB action | Result | Rule shown |
|---|---|---|
| set ISPR1 bit 7 with ISER1 bit 7 = 0 | stays pending, never taken; ICSR `0x0040_0000` (ISRPENDING 1, VECTPENDING 0) | taken only if enabled **and** priority suffices; VECTPENDING counts only enabled ones |
| then set ISER1 bit 7 | taken: xPSR `0x0900_0037` (IPSR 55), LR `0xFFFF_FFE9`, 26-word frame, ICSR `0x0000_0837`, ISPR1 → 0, IABR1 → `0x80` | entry clears pending, sets active |
| after return | IABR1 0, ICSR 0, counter 1, no re-pend | — |

## 4. Preemption vs waiting (2.5c)

IRQ 39 pended while the core sat at SysTick's first instruction (SysTick at level 4):

| | IRQ 39 at level 0 | IRQ 39 at level 5 |
|---|---|---|
| ICSR while halted | `0x00C3_780F` — ISRPREEMPT **1** | `0x0043_780F` — ISRPREEMPT **0** |
| What happened | **preempts** SysTick: nested frame on MSP (`0xFF88 → 0xFF68`, 8 words — the handler has no FP context), EXC_RETURN `0xFFFF_FFF1` (return to Handler mode), RETTOBASE 0, SysTick + IRQ 39 both active | **waits**: taken after SysTick's body, on SysTick's own Thread frame, with SysTick's EXC_RETURN `0xFFFF_FFE9` still in LR |

A debugger lesson came out of 2.5c-1: continuing from a breakpoint at the current pc makes the
debugger **execute that instruction first** (stacked return address `0x116` instead of `0x114`,
plus an alignment pad). Deleting the breakpoint before `continue` gave `0x114` and no pad —
measured both ways.

## 5. Interrupt pended from C, and where it lands (2.5d)

`main` enables and pends IRQ 39 itself (ISER1/ISPR1 writes, `dsb`, `isb`) and prints which
branch ran: after a reset with no debugger the terminal shows `Hello` / `IRQ 39 taken`. With a
breakpoint in the handler, the stacked return address was `0x226` = the `isb`: the pend took
effect after the `dsb` completed, before the `isb` executed (one sample).

## 6. Tail-chaining proven by cycle count (2.6)

Is a pending interrupt at the end of a handler **tail-chained** (no unstack + restack), or does
the core return to Thread and take it again? Both leave the same frame on the stack, so the
answer had to come from time:

| Measurement (CYCCNT, halts excluded) | Cycles |
|---|---|
| SysTick epilogue → IRQ 39's first instruction, IRQ 39 pending (2.6) | **23** |
| SysTick epilogue → plain return landing in Thread, nothing pending (2.6b) | **21** |
| Minimum for return-then-new-entry: 21 + 17 (measured 8-word entry) − 6 (debug restart) | **32** |

23 < 32 → **tail-chained**: the switch to the next handler costs about two cycles more than a
plain return. The same measurement showed SysTick's first interrupt landing inside
`uart4_putc` while "IRQ 39 taken" was still being sent (GDB: `c=110 'n'`).

## 7. Closing checks

- Our own register macros were mapped against the SDK's `core_cm33.h`: all 11 addresses =
  block base + offset (read-only comparison; CMSIS stayed out of the build).
- Self-test 3.3/5 on paper, then a GDB check: level 3 (`0x60`) preempts SysTick —
  LR `0xFFFF_FFF1`, ICSR `0x37`, SHCSR `0x800`.

Next: [Phase 3 — faults on purpose](phase-3.md).
