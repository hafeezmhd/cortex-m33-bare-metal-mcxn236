# Phases 0–1 — From an empty project to "Hello" on the UART

**Log:** `learning_log.md`, sections "Phase 0" … "Phase 1 self-test" (sessions 1–8, 13–26 Oct 2026) ·
**Code:** still in today's `source/startup_m33.S`, `m33_scratch.ld` and the LED/UART lines of
`main` (no separate tag: these phases predate the repository's first commit)

The project started as an empty MCUXpresso C project with every SDK component unticked. The
SDK startup file, linker script, `SystemInit`, drivers and CMSIS headers were never added.
Everything the chip needs to boot and talk was written by hand and checked on the board:
first a read-only audit, then a linker script and vector table, then RAM init and the FPU, then
an LED and a UART driven straight from register addresses.

## 1. Phase 0 — what the chip is, before writing code

| Fact | Value | Source |
|---|---|---|
| Core | Cortex-M33 r1p0, CPUID `0x411F_D210`, FPv5 single precision (MVFR0 `0x1011_0021`) | read with GDB in 1a.5 |
| Flash | 1 MB at `0x0000_0000` (Secure alias `0x1000_0000`) | RM 3.4, Table 16 |
| RAM | RAMA–RAME at `0x2000_0000`; we use only **RAMB** `0x2000_8000`, 32 KB (an access may not cross a RAM block) | RM 4.4 |
| Peripherals | `0x4000_0000` Non-secure / `0x5000_0000` Secure alias (address bit 28) | RM Table 16 |
| Priority bits | 3 (8 levels) | RM 3.3.1, measured in Phase 2 |
| Boot ROM | checks our vector table: word 0 = valid RAM SP, word 1 = valid flash PC, words 8/9/10/13 = 0 for a plain image | RM Table 158 |

Two first findings: the wizard's project never built (board stubs not on the include path),
and "hard-float ABI with the FPU off at reset" means our `Reset_Handler` must enable the FPU
before any C runs.

## 2. Boot from nothing (1a)

| Step | Evidence |
|---|---|
| Own `.ld` with no roots | link succeeds, image **0 bytes** — `--gc-sections` keeps only what is reachable from `ENTRY` and `KEEP()`; `main` was discarded (map: `.text.main 0x8`) |
| Vector table + `Reset_Handler` (`bl main`) | image 80 B; `main` kept because `Reset_Handler` calls it — the linker has no special "main" |
| Table check (objdump) | word 0 `0x2001_0000` (`__stack_top`), word 1 `0x49` = `Reset_Handler` 0x48 + Thumb bit 1, slots 8/9/10/13 = 0 |
| First debug run | halted at `main`, sp `0x2000_FFFC` → the ROM loaded word 0 and ran our code |
| Breakpoint at `Reset_Handler` after a real reset | LR `0x1302_D94D` = inside the Secure boot ROM → the ROM **called** us (BLX); MSP = word 0 |
| Security state | `msp_s` active, `msp_ns` 0, CONTROL_S 0 → we run **Secure, privileged, on MSP** (SAU off: everything is Secure) |

## 3. RAM and the FPU (1b, 1c)

- **Without `.data`/`.bss` rules** the globals became orphan sections placed in **flash**: the
  store in `main` would have targeted flash. With `> RAMB AT > FLASH`, `.data` runs at
  `0x2000_8000` and loads from flash.
- **RAM is not zero at reset:** before the `.bss` loop `g_zero` read `0x0000_000E`. The copy and
  zero loops in `Reset_Handler` were then confirmed word by word (`0x12345678` / `0`).
- **FPU off on purpose (1b.4a):** the first `vldr` faulted — CFSR `0x0008_0000` = UFSR.NOCP,
  escalated to HardFault (HFSR `0x4000_0000` FORCED) because UsageFault was disabled; stacked PC =
  the faulting instruction; EXC_RETURN `0xFFFF_FFF9`, 8-word frame.
- **Fix (1b.4b):** `CPACR |= 0xF << 20`, `dsb`, `isb` as the first lines of `Reset_Handler` →
  `f = 1.5f * 2` read back `0x4040_0000` (3.0f), CONTROL_S `0xC` (FPCA and SFPA set by the first FP instruction).
- **Thumb bit broken on purpose (1c):** a `blx` to an even address → CFSR `0x0002_0000` =
  UFSR.INVSTATE, stacked PC = the even target. Because FPCA was now 1 the frame was **26 words**
  and EXC_RETURN `0xFFFF_FFE9` (FType 0) — the only difference from 1b.4a.

## 4. Register to pin: the red LED (1d)

The red LED D13 sits between VDD_BOARD and pin P4_18 (680 Ω): **active-low**. Driving it from GDB,
one register at a time, gave the rules used for every peripheral since:

| Finding | Evidence |
|---|---|
| The boot ROM changes registers before `main` | AHBCLKCTRL0 `0x7FF` at main, RM reset value `0x603` |
| PORT4 clock gated → access error; GPIO4 gated → reads 0 silently, keeps its state, pin keeps driving | "Cannot access memory" vs `0` with the LED still on |
| P4_18 is GPIO out of reset (MUX 0) — the input buffer is a separate bit | PDIR stayed 0 until PCR18.IBE (bit 12) = 1; RM 57.3.1 "MUX 0 = analog" does not match this silicon |
| SET/CLR/TOGGLE registers are action registers | PSOR/PCOR/PTOR read 0; target = old \| bits |
| Active-low proven | pin low → ON, pin high → OFF |
| Busy-wait cost | one `volatile` delay iteration = 7 instructions, **15 cycles** (CYCCNT: 15,000,042 for 1,000,000) |
| Core clock | 48 MHz FIRC (SCG CSR `0x0300_0000`, FIRCCFG 0) — agrees with 16 LED flashes per 10 s |

In C (1d.3/1d.4): clock gate by the SET register, then level (PCOR), then direction (PDDR), then
toggle with PTOR.

## 5. Register to pin: LPUART4 to the VCOM (1e)

Path from the documents: P1_9 (TX) / P1_8 (RX) = LP_FLEXCOMM4 lines FC4_P1 / FC4_P0 on ALT2
(RM Table 64) → 0 Ω R53/R54 → MCU-Link → USB VCOM (COM3). Six gates had to open, each found in
its own RM chapter and checked from GDB before being written in C:

| Gate | Write | Notes |
|---|---|---|
| Bus clocks | AHBCLKCTRLSET0 bit 14 (PORT1), SET1 bit 15 (FC4) | via SET registers: AHBCLKCTRL1 had reserved bit 29 set by the ROM |
| Resets | PRESETCTRLCLR0/1 | already released at main |
| Functional clock | FROHFDIV 0 → FCCLKSEL4 3 (fro_hf_div) → FLEXCOMM4CLKDIV 0 | the ROM left the divider **HALTED**; released last |
| Pin mux | PCR8 `0x1200` (ALT2 + IBE), PCR9 `0x200` | — |
| Mode | PSELID PERSEL = 1 (UART) | read-only fields ignore the write |
| LPUART | BAUD `0x0F00_001A` (×16, SBR 26 → 115,384.6 baud, +0.16 %), then CTRL `0x0008_0000` (8N1, TE) | SBR may change only while TE = RE = 0 |

First byte: a GDB write of `0x61` to DATA printed `a` on the terminal. Then in C:
`uart4_putc` polls STAT.TDRE (buffer has room — back-to-back characters) rather than TC (line
idle), and `uart4_puts("Hello\r\n")` printed `Hello` after a cold boot with no debugger.

One bug was caught on paper before the first build: `while (STAT & TDRE == 0u)` — `==` binds
tighter than `&`, so the loop never waited. Written as `((STAT & TDRE) == 0u)`.

## 6. Lessons carried into every later phase

- A value at `main` is what the boot ROM left, not the RM reset value (AHBCLKCTRL0 `0x7FF`,
  FCCLKSEL4 3, FROHFDIV running, AHBCLKCTRL1 bit 29) — read it before predicting it.
- Change one variable at a time (the IBE-vs-MUX question needed exactly that).
- Count code size per instruction from objdump (`ldr` literal 2 B, modified immediate
  `ldr + mov.w + str` 8 B, each new literal 4 B); measure cycles with CYCCNT, never guess them.
- GDB `x/wx` for register reads (the IDE Memory view lost upper bytes on APB registers).

Self-tests: Phase 0 2/5, Phase 1 3.75/5 (both on paper, then checked on the board).

Next: [Phase 2 — exceptions and the NVIC](phase-2.md).
