# Phase 3 — Faults on purpose

**Log:** `learning_log.md`, blocks "3.0" … "Phase 3 closed" (sessions 16–22, 3–10 Nov 2026) ·
**Code:** `UsageFault_Handler`, `BusFault_Handler`, `MemManage_Handler` and their weak aliases are still in
today's sources (no separate tag)

Five faults were caused deliberately, one per step, each first with the fault disabled (to see
it escalate) and then with its own handler. For every one, the evidence is the same set of
registers read at the handler's first instruction: CFSR (what went wrong), HFSR (did it
escalate), SHCSR (which handler is active), the fault-address registers, and the stacked return
address (which instruction faulted).

## 1. The alarm panel (3.0)

| Register | Address | At `main` | Role |
|---|---|---|---|
| CCR | `0xE000ED14` | `0x0000_0201` | traps off (DIV_0_TRP bit 4, UNALIGN_TRP bit 3) |
| SHCSR | `0xE000ED24` | 0 | fault enables (MEMFAULTENA 16, BUSFAULTENA 17, USGFAULTENA 18) + active bits |
| CFSR | `0xE000ED28` | 0 | MMFSR [7:0] · BFSR [15:8] · UFSR [31:16], write-1-to-clear |
| HFSR | `0xE000ED2C` | 0 | FORCED bit 30 = a disabled fault escalated to HardFault |
| MMFAR / BFAR | `0xE000ED34` / `…38` | both `0x2000_FFFC` | valid **only** when MMFARVALID / BFARVALID is set |

The debugger sets DEMCR's vector-catch bits at connect (`0x011007F0`), so every fault halts at
its handler's first instruction ("Program received signal SIGSTOP") with no breakpoint needed —
confirmed in 3.1e by removing the breakpoint.

## 2. The five faults

| Step | Trigger | Handler setup | Measured at entry |
|---|---|---|---|
| 3.1a | `udiv` by a volatile 0, trap **off** | — | result **0**, no fault (CFSR 0) |
| 3.1b | same, CCR `0x211` (DIV_0_TRP), UsageFault **disabled** | Default_Handler | **HardFault**: IPSR 3, HFSR `0x4000_0000` FORCED, CFSR `0x0200_0000` (DIVBYZERO), stacked RA = the `udiv` |
| 3.1c–d | same, USGFAULTENA 1 (set in C: CCR, SHCSR read-modify-write, `dsb`, `isb`) | own UsageFault_Handler, slot 6 | **UsageFault**: IPSR 6, HFSR 0, SHCSR `0x0004_0008`, handler saved CFSR `0x0200_0000` |
| 3.2a | word `ldr` from `0x2000_8009`, UNALIGN_TRP **off** | — | no fault; read `0x0012_3456` (little-endian bytes across two variables) |
| 3.2b | same, CCR `0x209` (UNALIGN_TRP) | UsageFault_Handler | UsageFault, CFSR `0x0100_0000` (UNALIGNED), RA = the `ldr`, destination never written |
| 3.3 | MSPLIM_S = `0x2000_FFE0`, then a call chain pushes below it | UsageFault_Handler | STKOF UsageFault, CFSR `0x0010_0000`; **SP = the limit**, no frame written below it; the handler's own `push` then faulted again → HardFault (FORCED, LR `0xFFFF_FFF1`), **no lockup** |
| 3.4 | store to LPUART4 DATARO `0x500B_4030` (a read-only register) | own BusFault_Handler, slot 5 | **precise** BusFault: CFSR `0x0000_8200` (PRECISERR + BFARVALID), BFAR `0x500B_4030`, RA = the `str`, HFSR 0 |
| 3.5 | Secure MPU region 0 `0x2000_8040–805F` privileged read-only, then a store into it | own MemManage_Handler, slot 4 | **precise** MemManage: CFSR `0x82` (DACCVIOL + MMFARVALID), MMFAR `0x2000_8040`, RA = the `str`, SHCSR `0x0007_0001` |

MPU setup for 3.5 (all from DDI0553): RBAR `0x2000_8045` (base, privileged RO, XN), RLAR
`0x2000_8041` (limit, attribute index 0, enable), MAIR0 `0x44` (Normal non-cacheable), MPU_CTRL
`5` (PRIVDEFENA + ENABLE). MPU_TYPE read `0x0000_0800` = 8 regions.

## 3. What the measurements settled

- **Escalation:** a disabled fault becomes a HardFault with HFSR.FORCED, keeping the original
  CFSR bit and return address; enabling it gives the fault its own handler and HFSR stays 0.
- **Precise faults point at the instruction:** in every synchronous fault (3.1b, 3.2b, 3.4, 3.5)
  the stacked return address is the faulting instruction itself — unlike an asynchronous IRQ,
  whose return address is the next instruction.
- **The faulting store does not happen (3.5a):** a pattern written to the MPU-protected word by
  the debugger (debug-port writes are not MPU-checked) survived the faulting `str`.
- **Fault addresses only with their VALID bit:** both handlers check BFARVALID (CFSR bit 15) /
  MMFARVALID (bit 7) before trusting the register. MMFAR and BFAR read the same value in every
  state (`0x2000_FFFC`, `0x320`, `0x500B_4030`, `0x3B4`, `0x2000_8040`) → one shared register on
  this core (inference; DDI0553 allows sharing). With VALID 0 it held a recent data address — UNKNOWN by the spec.
- **Stack limit:** SP may go down *to* the limit; going below it faults with SP set to the limit
  and nothing stacked below (DDI0553 B3.21).

## 4. Document discrepancies found

- TRM Table 3-3 says CCR.UNALIGN_TRP is "fixed at 1"; on this chip it is writable (3.2a 0 → no
  fault, 3.2b 1 → fault).
- RM Table 4 says MPU violations invoke the HardFault handler; the TRM and the measurement say
  MemManage (with MEMFAULTENA 1).

Self-test 2.55/5 on paper, then a GDB check that confirmed every correction (MMFSR byte `0x82`,
BFSR byte `0`, stacked RA `0x2F4` = the `str`).

Next: [Phase 4 — modes, privilege, PSP and SVC](phase-4.md).
