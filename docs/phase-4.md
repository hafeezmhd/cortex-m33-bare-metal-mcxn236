# Phase 4 — Modes, privilege, the process stack and SVC

**Tag:** [`phase-4`](https://github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236/tree/phase-4) (6375ed1) ·
**Log:** `learning_log.md`, blocks "4.0" … "Phase 4 closed" (sessions 22–23, 10–16 Nov 2026)

An RTOS runs tasks in Thread mode on the process stack (PSP), often unprivileged, and lets them
ask for privileged work through `svc`. This phase built that path one bit of CONTROL at a time:
first move Thread mode to PSP, then drop privilege and watch the MPU refuse it, then use `svc`
to get privilege back.

## 1. Banked state at `main` (4.0)

| Register | Value | Meaning |
|---|---|---|
| CONTROL_S | `0` | privileged (nPRIV 0), MSP (SPSEL 0), FPCA 0 (the first FP instruction runs after the halt) |
| MSP_S | `0x2000_FFF0` | = `$sp`: `0x2001_0000` − main's 16-byte prologue |
| PSP_S, MSPLIM_S, PSPLIM_S | `0` | process stack and stack limits unused |
| xPSR | `0x6900_0000` | IPSR 0 = Thread mode |
| SHPR2 | `0` | SVCall at level 0 |

Every core register that differs between Secure and Non-secure has its own GDB name
(`control_s`, `psp_ns`, `msplim_s` …); all were readable.

## 2. Thread mode on the process stack (4.1)

`msr psp, 0x2000_B000`, then CONTROL read-modify-write `|= SPSEL` (`0xC → 0xE`), then `isb`.
After that `$sp` = PSP `0x2000_B000` while MSP stays `0x2000_FFF0`. A fault from Thread mode then showed:

| Item | Value | Meaning |
|---|---|---|
| Frame | on **PSP**: `0x2000_B000 → 0x2000_AF98` (26 words, FPCA 1) | hardware stacks onto the stack Thread was using |
| Handler stack | MSP `0x2000_FFF0` | Handler mode always uses MSP |
| EXC_RETURN | `0xFFFF_FFED` | bit 2 SPSEL = 1: return to PSP (vs `0xFFFF_FFE9` from MSP) |
| CONTROL_S in the handler | `0` | entry cleared SPSEL, FPCA and SFPA |

## 3. Unprivileged Thread mode under the MPU (4.2)

With the Phase 3 MPU still on (one region, PRIVDEFENA 1), setting CONTROL.nPRIV made the very
next instruction fetch fault: the background memory map is for privileged code only, and no
region covers flash.

| Item | Value | Meaning |
|---|---|---|
| CFSR | `0x0000_0011` | IACCVIOL (instruction fetch) + MSTKERR (the fault's own stacking failed) |
| MMFARVALID | 0 | no address recorded for a fetch fault |
| Frame | **no word written**, yet PSP moved to `0x2000_AF98` | exception stacking for an unprivileged context is itself checked as unprivileged |
| CONTROL_S in the handler | `0x1` | nPRIV is kept on entry; the handler is privileged because it is in Handler mode |
| HFSR | 0 | MemManage handled it; no lockup |

## 4. SVC back to privileged (4.3)

With the MPU turned off, unprivileged Thread code runs normally and calls `svc 0`. Our
`SVC_Handler` (vector slot 11, [source at the tag](https://github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236/blob/phase-4/source/frdmmcxn236_m33_scratch.c#L180)):

```c
void SVC_Handler(void)
{
    uint32_t *stacked_sp;
    __asm__ volatile (                 /* EXC_RETURN bit 2 = SPSEL: which stack holds the frame */
        "tst lr, #4\n\t"
        "ite eq\n\t"
        "mrseq %0, msp\n\t"
        "mrsne %0, psp\n\t"
        : "=r" (stacked_sp)
    );
    uint32_t return_address = stacked_sp[6];             /* ReturnAddress at +0x18 */
    uint8_t svc_num = ((uint8_t *)return_address)[-2];   /* svc imm8, little-endian */
    g_svc_num = svc_num;

    uint32_t ctrl_val;
    __asm__ volatile ("mrs %0, control" : "=r" (ctrl_val));
    ctrl_val &= ~(1u << 0);            /* clear nPRIV */
    __asm__ volatile ("msr control, %0" : : "r" (ctrl_val) : "memory");
    __asm__ volatile ("isb" : : : "memory");

    g_svc_count++;
}
```

CONTROL_S through the call, measured at each breakpoint:

| Point | CONTROL_S | Why |
|---|---|---|
| at the `svc` (unprivileged Thread on PSP, FP used) | `0xF` | nPRIV 1, SPSEL 1, FPCA 1, SFPA 1 |
| SVC_Handler entry | `0x1` | entry clears SPSEL, FPCA, SFPA — keeps nPRIV |
| after the handler's `msr` + `isb` | `0x0` | nPRIV cleared |
| back in Thread | `0xE` | return restores SPSEL from EXC_RETURN, FPCA from FType, SFPA from the stacked RETPSR — **only nPRIV carries across** |

Also measured: IPSR 11 (SVCall), SHCSR `0x0007_0080` (SVCALLACT), LR `0xFFFF_FFED`, frame
26 words on PSP, stacked return address `0x388` = the instruction **after** the `svc` (so the
`svc` itself sits at RA − 2), immediate 0. After the return the store that faulted in Phase 3
now completes, and the terminal prints `Hello` / `IRQ 39 taken`.

Self-test 1.8/3 on paper; every board item had already been measured in 4.3.

Next: [Phase 5 — a context switcher of our own](phase-5.md).
