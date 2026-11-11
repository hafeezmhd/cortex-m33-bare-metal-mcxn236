# frdmmcxn236_m33_scratch — Learning Log

Series: ARM Cortex-M33 + FreeRTOS from scratch on FRDM-MCXN236.
Theory: "ARM Cortex-M and FreeRTOS: Daily Study Guide" (sections cited as G1.8 etc.)
Priority: Study Checklist → Priority Ranking tab.
Capstone projects (frdmmcxn236_hello_world, _ns, _s) are never opened for editing, built or flashed.

## Rules (step 0.6)
- Allowed: CMSIS device header + core_cm33.h for register names/addresses only; debugger, .map, objdump.
- Not allowed until stated: SDK startup, SDK linker script, system_MCXN236.c, fsl_* drivers, FreeRTOS templates/demo configs.
- SDK versions may be read AFTER ours, for comparison only.
- Every step: explain → cite guide → PREDICT → run → verify on hardware → log. Never silently revise; log original + correction.
- No PASS/FAIL without hardware evidence (registers, memory view, disassembly, serial).

## Phase 0 — Setup and read-only audit (2026-10-13)

### 0.1 Toolchain
| Item | Value |
|---|---|
| IDE | MCUXpresso IDE v25.6.136 |
| SDK | SDK_2.x_FRDM-MCXN236 26.06.00, manifest 3.15.0 (newer than IDE line — observation) |
| GCC | Arm GNU Toolchain 14.2.Rel1, gcc 14.2.1 20241119 (IDE bundled) |
| LinkServer | 26.6.137 (IDE default path); 25.6.131 installed, unused |
| Probe | MCU-LINK FRDM-MCXN236 (r0E7), CMSIS-DAP V3.146, serial P3WFDLNLBQFFN, caps DEBUG/VCOM/SIO |
Decision: probe firmware stays at V3.146 for the whole series (3.160/3.172 installers present, not flashed).
Note: IDE option "Pull ISP on reset (MCU-Link)" is ON — check its effect at step 1a.

### 0.2 Documents
- MCX N23x Reference Manual Rev. 4, 2025-03-05
- Mask Set Errata 0P21K Rev. 2.0, 14 Apr 2025
- UM12041 FRDM-MCXN236 Board User Manual Rev. 3.0, 16 Aug 2024
- FRDM-MCXN236 schematic (latest history entry 24 Jan 2024)
- ARMv8-M Architecture Reference Manual DDI0553 B.z
- Cortex-M33 TRM 100230 Issue 08 (core r1p0)

### 0.3 Memory map (RM 3.4, Table 16, Table 17)
Prediction: flash @0x4000_0000 4 MB; SRAM @0x0 4 MB, several blocks; ROM @0x4000_0000;
alias = EXC_RETURN bit 0; peripherals/odd regions: no clue. Result: 1/6 (several blocks).
| Region | Non-secure | Secure | Size |
|---|---|---|---|
| Flash (2×512 KB) | 0x0000_0000 | 0x1000_0000 | 1 MB |
| Boot ROM | 0x0300_0000 | 0x1300_0000 | 256 KB |
| RAMX | 0x0400_0000 | 0x1400_0000 | 96 KB |
| RAMA–RAME | 0x2000_0000 | 0x3000_0000 | 256 KB (224 usable with default ECC) |
| Peripherals | 0x4000_0000 | 0x5000_0000 | — |
| PPB | 0xE000_0000 | not aliased | — |
Alias rule: address bit 28 = 1 → Secure.
Correction learned: EXC_RETURN bits describe the exception return (G4.5); memory security comes from the address via IDAU+SAU (G4.2).
Findings:
- RM contradiction: 3.4 says 0x0–0x1FFF_FFFF always Non-secure; Table 16 says 0x1000_0000+ Secure. Resolve with TT instruction (Phase 12).
- Each RAM block is a separate AHB port; no access may cross a block boundary (RM 4.4). Stacks/buffers stay inside one block.
- RAMB/RAMX ECC on by default hides RAME2/RAME3 (32 KB) → wizard shows SRAM 0x38000 (224 KB).

### 0.4 Core ID registers — deferred to step 1a (no attach to the capstone image).

### 0.5 Chip-specific unknowns
| Item | Answer given | Manual | Source |
|---|---|---|---|
| __NVIC_PRIO_BITS | 3 | 3 bits, 8 levels ✅ | RM 3.3.1 |
| Clock at reset | 12 MHz | fro_hf 48 MHz default; ROM runs CPU at 48 MHz unless CMPA BOOT_SPEED ❌ | RM 24.1, 12.2.4 |
| Watchdog at reset | running | WWDT MOD resets to 0 → stopped ❌ | RM 37, MOD reg |
| ROM checks | vector table + header | ✅ — see below | RM Table 158 |
Boot ROM reads our vector table: 0x00 SP must be valid RAM, 0x04 PC valid flash, 0x20 image length, 0x24 imageType (0 = plain), 0x28 CRC/ext header, 0x34 exec address (0 = XIP). Our reserved slots = 0.
SWD is disabled while the boot ROM runs.

### 0.7 Project creation
Wizard: FRDM-MCXN236, MCXN236VDF, Empty board files, C project, Copy sources ON,
all components unticked (TFM_NS/osa had to be found and unticked — a "–" box means a child is selected),
Redlib nohost-nf, semihost HardFault handler OFF, FPv5-SP-D16 Hard ABI, TrustZone None, flash link (not RAM).
Memory table: FLASH0 0x0/0x80000, FLASH1 0x80000/0x80000, SRAM 0x2000_0000/0x38000, SRAMX 0x0400_0000/0x18000.
Prediction: hello_world.c/.h. Result: source/frdmmcxn236_m33_scratch.c + 8 stub board files; no startup/device/CMSIS. 0/2.
Disk check (dir /s /b /a): 13 files, matches IDE tree.
Finding: Hard ABI + FPU off at reset → our Reset_Handler must set CPACR CP10/CP11 before any C code (else UsageFault NOCP).
Workspace is a git repo (workspace, main, ↑1 unpushed capstone commit). New files untracked; decision pending (A same repo / B .git/info/exclude).
Correction: log lives at project root (frdmmcxn236_m33_scratch/learning_log.md), not source/ as first planned. Kept — root is not compiled.

## Hardware checks queued for step 1a
CPUID / ID_PFR1 / MVFR0 · SYSCON DEVICE_ID0 (0xFF8) FLASH_SIZE/RAM_SIZE · security state (expect Secure) ·
main clock (expect 48 MHz) · WWDT0/1 MOD (expect WDEN=0) · IPR write 0xFF → read 0xE0 · "Pull ISP on reset" effect.

### Phase 0 self-test (2026-10-13): 2/5
Q1 ½ (rule right, addresses 0x0000_0100 NS / 0x1000_0100 S missing) · Q2 1 · Q3 ½ (CRC vs 0x28 → invalid → no jump, likely ISP; len=0 edge case unknown)
Q4 0 (CPACR CP10/CP11 off → UsageFault NOCP → escalates to HardFault FORCED; fix: set CPACR[23:20], DSB, ISB) · Q5 0 (5<<5 = 0xA0; low 5 bits unimplemented)
Re-test: Q4 in step 1b (on hardware), Q5 in Phase 2.

### 1a.1 Managed linker script OFF
Script the IDE expects: frdmmcxn236_m33_scratch_Debug.ld (project + config name), Script path empty → looked for in Debug/.
IDE default stack: end of SRAM → initial SP 0x2003_8000 (top of RAME1, single block). Heap: post-data.
Prediction: build fails at link (missing linker script). Result: build fails at COMPILE — board.h not found.
Cause: wizard created board/ but did not add it to the include path (-I has only source/). Generated project never builds.
Compile flags noted: -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -O0 -g3 -ffunction-sections -fdata-sections -fstack-usage.

### 1a.2 Board stubs removed, main() = for(;;)
Prediction: compile OK; no linking; fails on missing startup; warning for no return.
Result: compile OK; linker DID run; failed: "cannot open linker script file frdmmcxn236_m33_scratch_Debug.ld"; 0 warnings. Score 1/3.
Learned: linking always runs; ld needs the script before anything else; main() falling off = return 0 (C99), no -Wall warning.
Link flags: -nostdlib (no toolchain startup/libs), --gc-sections (unreferenced sections removed → vector table needs KEEP), -Map, -print-memory-usage.

### 1a.3 Hand-written m33_scratch.ld (MEMORY FLASH 1M @0x0, RAMB 32K @0x2000_8000; __stack_top = 0x2001_0000; .isr_vector KEEP + .text)
IDE settings: Linker script = m33_scratch.ld, Script path = .. → ld called with "-T m33_scratch.ld -L ..".
Prediction: succeeds; no warnings (-Werror); FLASH 1 MB; main discarded. Score 2/4.
Result: success, 1 warning "cannot find entry symbol Reset_Handler; not setting start address"; FLASH used 0 B; text/data/bss = 0.
Corrections: no -Werror in the command lines (only -Wall); 1 MB is Region Size, not Used Size.
Learned: --gc-sections keeps only what is reachable from ENTRY + KEEP(). No roots → empty image.
Post-build: objcopy/checksum commented out (LPC-era vector checksum not used on MCXN).

### 1a.3b Map file evidence
Prediction: heading "text"; section ".main"; ~1 KB. Score 0/3.
Result: "Discarded input sections": .text.main 0x8 (8 bytes). Cross-ref: main defined, never referenced → no gc root → discarded. Claim proven.
__stack_top = 0x20010000 ✓. Memory Configuration matches script ✓.
Note: --sort-section=alignment wraps our input patterns in SORT_BY_ALIGNMENT — the effective script differs from our file.

### 1a.4 startup_m33.S: 16-entry vector table (.isr_vector) + Reset_Handler (bl main; b .) + Default_Handler (b .)
Prediction: warning gone (Reset_Handler defined); main discarded (linker treats main as entry); FLASH ?; word1 = 1 (odd). Score 1.5/4.
Result: 0 warnings; FLASH 80 B = 64 (table) + 6 (Reset_Handler) + 2 (Default_Handler) + 8 (main) → main KEPT.
Learned: ld has no special "main". Roots = ENTRY + KEEP(); Reset_Handler's "bl main" makes main reachable. On bare metal main is just the function our reset code calls.
Assembler line: -x assembler-with-cpp (capital .S → C preprocessor runs).

### 1a.4b Static image check (objdump -s / -d)
Prediction: main@0x00, Reset_Handler@0x04, Default_Handler@0x08–0x3C; word1 "8-byte, dump it"; word0 = initial MSP from script; main = bl + b. Score 1/4.
Result: table 0x00–0x3F; main@0x40, Reset_Handler@0x48, Default_Handler@0x4E.
Word0 0x2001_0000 ✓ (__stack_top). Word1 0x0000_0049 = 0x48|1 ✓. Handlers 0x4F = 0x4E|1 ✓. Slots 0x20/24/28/34 = 0 ✓.
main @ -O0: push {r7}; add r7,sp,#0; nop; b.n 44 → 8 B, no epilogue (never returns). First RAM write = push → 0x2000_FFFC (RAMB).
Learned: table slots hold POINTERS; code sits after the table. Entries are 32-bit words. Word value = function address | 1.

### 1a.5a First flash + debug (default "stop at main")
Dialogs: Target mismatch (project MCXN236 vs probe MCXN236VDFT) → Yes (same part, suffix only). Probe FW update offered → declined (keep V3.146). IDE Debug Mode: Non-Stop.
Stopped: pc 0x44 <main+4> (bp after prologue). msp=sp=0x2000_FFFC (0x2001_0000 − 4 from push {r7}) → word0 loaded, first RAMB write OK.
lr 0x4D <Reset_Handler+4> → bl main at 0x48 executed. psp 0, ipsr 0 (Thread), epsr T, xpsr 0x6900_0000 (ROM leftover flags).
Inference (not fact): r0 = 0x49 = our reset vector → ROM loaded word1 and branched. r4 0x3000_F544 / r5 0x5000_0480 = Secure aliases → ROM runs Secure. r8–r11 magic patterns, meaning unknown.
RESULT: boot ROM accepted plain image (slots 0x20/24/28/34 = 0) and executed our Reset_Handler. PASS (hardware evidence above).

Security state (open since 0.3): msp_s = 0x2000_FFFC = active sp; msp_ns = 0 → running SECURE. Prediction from 0.3 CONFIRMED.
control_s = 0 → privileged, MSP, FPCA=0. msplim_s = 0 (no stack limit). primask/basepri/faultmask_s = 0 (nothing masked at handover).
Reason: core resets Secure; SAU off + ALLNS=0 → all Secure; most-secure wins. Does NOT resolve RM 3.4 vs Table 16 (SAU masks IDAU) → still TT in Phase 12.

FPU (queued check): MVFR0 = 0x1011_0021 → SIMDReg 1, FPSP 2 (SP ✓), FPDP 0 (no DP ✓), Div/Sqrt/Round 1. Matches -mfpu=fpv5-sp-d16. (DDI0553 D1.2.178)
FPCCR = 0xC000_0004 → ASPEN=1, LSPEN=1 (lazy stacking on, Phase 5); bit 2 decode deferred. s0–s31 = 0, FPCA = 0.

### 1a.5a (cont.) Resume/Suspend ×3
Prediction: pc 0x46; "PC will not move". Result: pc 0x44 ×3, sp 0x2000_FFFC ×3; DWT CYCCNT +~425M, +~420M cycles → core WAS running.
Learned: unchanged pc ≠ stopped core; CYCCNT is the proof. Always-0x44 = likely halt during branch refill (inference, 3 samples).
CYCCNT cannot give clock freq (wait time unknown) — clock check still queued.
CoreSight ROM table @0xE00F_E000: ITM 0xE000_0000, DWT 0xE000_1000, FPB 0xE000_2000, SCS 0xE000_E000, +0xE004_0000.
PID0 0x21 + PID1[3:0] 0xD → PARTNO 0xD21 = Cortex-M33 (TRM Table 3-5) ✓.

### 1a.5c CPUID (closes deferred 0.4)
Prediction: none given. CPUID @0xE000_ED00 = 0x411F_D210 → Arm, r1p0, PartNo 0xD21 (M33). Matches TRM Issue 08 reset value ✓.
Same row: ICSR 0; VTOR 0x0 (our table) ✓; AIRCR 0xFA05_0000 (PRIGROUP 0, BFHFNMINS 0, PRIS 0, little-endian);
SCR 0; CCR 0x201 (bits 9,0 RES1; DIV_0_TRP=0, UNALIGN_TRP=0 → Phase 3 must set them). Source: DDI0553 D1.2.

### 1a.5b Halt at first instruction (bp on Reset_Handler, Restart = real reset via boot ROM)
Prediction: MSP 0x40; PC 0x48 with bit0 clear; LR not given. Score 1/2.
Result: pc 0x48 <Reset_Handler>, epsr T; msp = msp_s = sp = 0x2001_0000 (word0, nothing pushed yet); r7 0 (reset happened).
lr 0x1302_D94D → inside boot ROM Secure alias (0x1300_0000–0x1303_FFFF), bit0 set → ROM CALLED us (BLX). r0 0x49 again (2nd boot) → ROM likely does BLX r0 with our vector (strong inference).
Correction learned: MSP ← word0 (__stack_top), not a code address. Thumb bit → EPSR.T, PC always even.
Open: CYCCNT advanced only 0xA8 across Restart → does not count through reset/ROM (cause unknown). Never compare CYCCNT across resets.
Flash log: only semihosting/CoreSight console found → replaced by direct flash read-back (1a.6).

## Session 2 (2026-10-14)

### 1a.6 Read-only memory audit (Memory view, halted at Reset_Handler) — first record, superseded by "1a.6 / 1a.6b" below
Prediction: flash = objdump (0x04 = 0x49); 0x50 → error; RAM 1000b 256 KB, FLASH 0110b 1 MB; NS alias returns 0; CUSTOMER_REVISION_ID 1010b.
Result: 0x00–0xBF "????" (wide monitor); 0x50 own monitor 0xFFFFFFFF; 0xC0–0x14F 0xFFFFFFFF (erased).
DEVICE_ID0 S 0x5000_0FF8 = 0x6A, NS 0x4000_0FF8 = 0x6A (no error). DIEID 0xFFC = 0xA0. 0x5000_1000 "??????5B" (reserved slot).
Decode (RM 14.4.1.90): FLASH_SIZE 0110b = 1 MB ✅; RAM_SIZE 1010b = 352 KB ❌ (= 256 RAMA–E + 96 RAMX; inference).
NS alias ❌: Secure transaction to NS alias is legal (G4.2); debugger access, CPU test in Phase 12.
Anomalies: same address 0x50 read two ways (???? vs FF); DIEID [23:8] reads 0 vs RM fixed 0x5209 → Memory view upper bytes untrusted (inference: byte-wise fallback). Flash and [27:24] ungraded → 1a.6b with x/wx.
Reasoning correction: RM says 1010b = NON-secure version; any other value = secure.

### 1a.6 / 1a.6b Read-only memory audit (halted at Reset_Handler)
Prediction (1a.6, locked before run): flash = objdump (0x04 = 0x49); 0x50 → error; RAM 1000b 256 KB, FLASH 0110b 1 MB;
Secure read of NS alias → 0; CUSTOMER_REVISION_ID 1010b. Score 1.5/5.
Memory view result: 0x00–0xBF "????"; 0x50 FF in a 2nd monitor; ID regs showed only low byte (0x6A, 0xA0).
GDB x/wx result (trusted):
  flash 0x00–0x4F = objdump word for word (table, Thumb bits, bl main = F7FF FFFA); 0x50+ = 0xFFFFFFFF (erased) ✅
  → probe programmed exactly 80 B; capstone image erased at least 0x50–0x14F. Closes "flash log not found".
  DEVICE_ID0 S 0x5000_0FF8 = NS 0x4000_0FF8 = 0x0540_006A:
    CUSTOMER_REVISION_ID 0101b → secure version (TrustZone present); ROM_REV_MINOR 4; FLASH 0110b = 1 MB; RAM 1010b = 352 KB
    352 = 256 RAMA–E + 96 RAMX (inference). Wizard's 224 KB = usable SRAM at 0x2000_0000 with ECC on.
  DIEID 0x5000_0FFC = 0x0059_D9A0: chip number 0x059D9, major 0xA, minor 0.
Lessons:
  - Register reads: use GDB x/wx. Memory view lost upper bytes on APB regs and showed stale/failed "????" for flash.
  - An alias is an address; security belongs to the transaction. A Secure access to the NS alias is legal (G4.2). Debugger access only; CPU test in Phase 12.
  - RM: 1010b = NON-secure version; any other value = secure.
RM discrepancy: DIEID reset diagram (RM 14.4.1.91) shows bits 23:8 = 0x5209; silicon reads 0x59D9. Unexplained.
Tutor correction: my "…5209 = 32-bit read OK" check assumed the RM value was right; it isn't for this die.

### 1a.7 Watchdog + clock gate (read-only, GDB x/wx, halted at Reset_Handler)
Prediction: AHBCLKCTRL0 = 0x603 with bit28 = 1; WWDT0 MOD = 0 with WDEN = 1; TV unchanged because core halted. Score 1/3.
Result: AHBCLKCTRL0 0x5000_0200 = 0x7FF (reset 0x603, RM 14.4.1.12) → ROM set bits 2–5 (RAMB–RAME ctrl clocks) + 6–8 (reserved).
  Bit28 WWDT0 = 0, bit29 WWDT1 = 0 → watchdog bus clocks gated.
  WWDT0 MOD 0x5001_6000 = 0 (WDEN 0); TV 0x5001_600C = 0xFF twice (reset value, not counting).
Confirms Phase 0 claim "WWDT stopped at reset" on hardware, after the ROM. No feed needed in 1b.
Caveat: WWDT0 clock gated → RM silent on gated reads; MOD=0 could be bus default. Queued: set bit28, re-read.
Lessons: ROM changes clock gates before handing over → never assume reset values at main(). TV frozen because WDEN=0,
  not because the core is halted. Decode your own predicted value before sending it.
  
### 1b.1 Globals with no .data/.bss rules (build only, NOT flashed)
Code: unsigned int g_init = 0x12345678; unsigned int g_zero; main: g_zero = g_init;
Prediction: builds, 0 warnings; .data/.bss in RAM (copied from flash); FLASH 80→88 B; VMA=LMA=0x60; .bss counts in FLASH
  because zeroes are stored. Score 1.5/5.
Result (clean build): 0 errors, 1 warning "LOAD segment with RWX permissions". FLASH Used 104 B, RAMB 0 B. size: text 96 data 4 bss 4.
Map/objdump -h: orphans → output sections named after input: .data.g_init VMA=LMA 0x60, CONTENTS LOAD DATA;
  .bss.g_zero 0x64, ALLOC only (file off 0x1064 = next section's → 0 bytes in file).
objdump -d: main 0x40–0x57: ldr r3,[pc] / ldr r3,[r3] / ldr r2,[pc] / str r3,[r2] + literals 0x60, 0x64 (+16 B).
  Reset_Handler moved 0x48→0x58, Default_Handler 0x4E→0x5E; bl re-encoded f7ff fff2 (linker fixes all refs).
Learned:
  - Unmatched input sections become orphans: ld places them after similar sections, in the same region (FLASH), silently
    (only --orphan-handling=warn reports them).
  - The store in main targets 0x64 = flash → would fault or be lost. Do not flash. (Optional Phase 3 experiment.)
  - RWX warning: .text (RX) + orphan .data/.bss (RW) contiguous → one ELF LOAD segment needing RWX (binutils ≥2.39 warns).
  - FLASH "Used Size" = address range used, including .bss; bytes actually programmed = 100.
  - .data goes to RAM only if the SCRIPT gives it a RAM VMA (> RAM AT > FLASH) and STARTUP copies it. Neither exists yet.
  
### 1b.2 .data/.bss output-section rules (build only, NOT flashed)
Script: .data : ALIGN(4) { __data_start__ … *(.data*) … __data_end__ } > RAMB AT > FLASH; __data_load__ = LOADADDR(.data);
        .bss : ALIGN(4) { __bss_start__ … *(.bss*) *(COMMON) … __bss_end__ } > RAMB
(Mistake caught before build: block first pasted into startup_m33.S. .S = assembler input, .ld = linker input.)
Prediction: .data VMA=LMA=0x60, .bss 0x64; RWX warning gone; FLASH 104 / RAMB 0. Score 0.5/3.
Result: 0 errors, 0 warnings. FLASH 100 B, RAMB 8 B.
  .data VMA 0x2000_8000 LMA 0x60 (CONTENTS LOAD DATA); .bss VMA 0x2000_8004 (ALLOC only; LMA 0x64 = same VMA−LMA offset, meaningless).
  .isr_vector: 0x2001_0000, 0x59, 0x5F…, reserved 0 → linker re-resolved table after code moved.
Learned: "> REGION" = VMA, "AT > REGION" = LMA. FLASH holds code + .data image; .bss costs RAM only.
  RWX warning gone: RW (RAMB) and RX (FLASH) no longer contiguous → separate segments (inference; objdump -p not run).
  Image still NOT runnable: nothing copies .data or zeroes .bss.
  
### 1b.3 .data copy + .bss zero in Reset_Handler (FLASHED)
Code: ldr =__data_load__/__data_start__/__data_end__ copy loop (ldr/str post-index #4); movs r3,#0 + str loop over
  __bss_start__..__bss_end__; bl main; b .; .ltorg.
Mistake caught before build: nested /* */ comments left "b ." live as first instruction and a stray */ on
  .type Default_Handler → rule: delete old code, don't comment it out; // for single lines.
Prediction: Stop A g_init = 0x12345678; B, C "no clue"; FLASH 116 B. Score so far 0/3, #4 pending.
Result (GDB x/2wx 0x20008000):
  A (Reset_Handler, before loops): 0xC5DB0FF2 0x0000000E (leftover RAM; origin unknown)
  B (main entry):                  0x12345678 0x00000000 → copy ✅ zero ✅
  C (after g_zero = g_init):       0x12345678 0x12345678 → store hits RAM ✅
PASS (hardware evidence above).
Learned: RAM is not zero at reset (g_zero was 0xE) → .bss zeroing is required, not cosmetic. Trace the loops by hand
  before predicting; "no clue" is not a prediction.
  
### 1b.4a FPU used with CPACR off → fault on purpose (Phase 0 Q4 re-test)
Code: main: volatile float f = 1.5f; f = f * 2.0f; (before for(;;)). Breakpoint on Default_Handler.
Prediction: IPSR 3; HFSR 0x40000000 FORCED; CFSR 0x00080000 "BusFault"; stacked PC at +0x18. Score 2/3. Q4: 0 → 2/3.
Result: stop at Default_Handler (pc 0xAC). xPSR 0x69000003 → IPSR 3 HardFault.
  CFSR 0x00080000 = UFSR.NOCP (bit 19; UFSR = CFSR[31:16], not BFSR). HFSR 0x40000000 FORCED.
  SHCSR 0x4 = HARDFAULTACT; USGFAULTENA = 0 → UsageFault disabled → escalated.
  Frame @MSP 0x2000FFD0: R0 B4, R1 20008008, R2 20008004, R3 3FC00000, R12 4, LR 95, PC 54, xPSR 69000000 (8 words, FPCA 0).
  EXC_RETURN 0xFFFFFFF9 (IDE Faults view): ES 1 Secure, SPSEL 0 MSP, Mode 1 Thread, FType 1 std frame, DCRS 1, S 1.
  objdump: 0x4E mov.w r3,#0x3FC00000; 0x52 str (integer, OK); 0x54 vldr s15 = first VFP instr = faulting PC ✅.
Learned: CFSR = MMFSR[7:0] | BFSR[15:8] | UFSR[31:16]. Stacked PC = faulting instruction (not next).
  Float constants can be moved/stored as integers → fault only at first real VFP instruction.
Open: GDB/LinkServer flash reads (x/3i 0x50) failed in HardFault state: "Debug port inaccessible after access at 0x40/0x50/0x4C";
  worked at Reset_Handler (1a.6b). Cause unknown. Test: x/3i 0x50 while halted at main.
Note: EXC_RETURN prediction spoiled by IDE Faults view.

### 1b.4b FPU enable in Reset_Handler (CPACR) — fix for 1b.4a
Code: first in Reset_Handler: ldr r0,=0xE000ED88; ldr r1,[r0]; orr r1,r1,#(0xF<<20); str r1,[r0]; dsb; isb.
  Same main as 1b.4a (volatile float f = 1.5f; f = f*2.0f). Breakpoints: main (stop-at-main), for(;;) line, Default_Handler.
Prediction: CPACR 0x00F00000; Default_Handler hits; FPCA 0 ("not doing anything float"); f not predicted. Score 1/4.
Result:
  main: CPACR 0xE000_ED88 = 0x00F00000 (CP10 = CP11 = 11b full access) ✅
  for(;;) (pc 0x60): xPSR 0x69000000 (IPSR 0), CFSR/HFSR/SHCSR = 0 → no fault; Default_Handler NOT hit.
  f @0x2000_FFF4 = 0x40400000 = 3.0f (1.5f 0x3FC00000, exponent +1).
  CONTROL_S = 0xC → nPRIV 0, SPSEL 0, FPCA 1, SFPA 1.
PASS (hardware evidence above). Closes Phase 0 finding "Hard ABI + FPU off → CPACR before any C".
Learned: Default_Handler runs only on an exception. FPCA set by HW on first FP instruction → future exceptions
  stack an extended (FP) frame, lazily filled (FPCCR.LSPEN=1) → Phase 5. SFPA = FP context is Secure.
  GDB names banked regs: control_s / control_ns (plain "control" invalid on this TrustZone core).
1b.3 #4 (FLASH bytes) never collected → ungraded.

### 1b.4c Debugger flash read retest
x/3i 0x50 at for(;;) (Thread, no fault): works. Failed in 1b.4a (HardFault state) → correlation with fault state
  (1 sample each), cause still unknown. 0x50 = middle of 32-bit mov.w @0x4E → "strh" garbage; disassemble from boundaries.
  No prediction given.

### 1c Thumb bit broken on purpose (BLX to even address)
Code: static void target(void){}; bad = (void(*)(void))((uintptr_t)target & ~1u); bad(); objdump: literal 0xD5 (target|1) →
  bic.w r3,#1 → 0xD4 → blx r3 @0x6A. main now push {r7,lr} (calls a function). Prediction: IPSR 3; CFSR UFSR bit 17 INVSTATE
  (BFSR 0); stacked PC 0xD4, T 0. Score 3/3. CFSR-layout re-test PASSED. Result: xPSR 0x69000003 (HardFault); CFSR 0x00020000
  = UFSR.INVSTATE; HFSR 0x40000000 FORCED. Frame @MSP 0x2000FF88: R0 E8, R1 20008008, R2 20008004, R3 D4, R12 4, LR 6D, PC
  D4, xPSR 68100000 (T=0, bit20=1). Frame size 0x2000FFF0 − 0x2000FF88 = 0x68 = 26 words = EXTENDED FP frame (FPCA=1 since
  1b.4b) vs 8 words in 1b.4a. EXC_RETURN 0xFFFFFFE9 → FType (bit 4) = 0 = extended frame; 1b.4a was 0xFFFFFFF9 (FType 1).
  Only difference. No prediction given. Learned: BLX with bit0=0 clears EPSR.T; fault raised at the target (stacked PC = even
  target), not at the blx. LR in frame = blx return (0x6D) → shows the caller. FPCA=1 → every exception stacks the 26-word
  frame (lazy fill, Phase 5). To verify (Phase 5): stacked xPSR bit 20 = SFPA (DDI0553 RETPSR) — inference, not checked in
  manual. Cleanup: target/bad lines removed from main after the experiment.

### 1d (started) LED path — facts verified, 1d.1 not yet run
RGB LED D13 (UM12041 Table 5 / Fig 7, schematic sheet 10): Red P4_18, Green P4_19, Blue P4_17; common anode → R166 0Ω →
  VDD_BOARD; cathodes via 680/470/470 Ω → MCU → active-low (from schematic; confirm in 1d.2). Using RED P4_18 (shared with
  mikroBUS PWM). Registers (Secure alias): AHBCLKCTRL0 0x5000_0200 bits 17 PORT4 / 23 GPIO4; PRESETCTRL0 0x5000_0100 bits
  17/23 (reset diagram 0; modify via SET/CLR); PORT4 PCR18 0x5011_A0C8; GPIO4 0x5009_E000: PDOR +40, PSOR +44, PCOR +48, PTOR
  +4C, PDIR +50, PDDR +54.

## Session 3 (2026-10-14 / 2026-10-15)

### 1d.0 Pre-checks (session 3, 2026-10-14)
mikroBUS socket: nothing plugged in (user-confirmed) → P4_18 drives only the red LED (D13, 680 Ω cathode resistor). Assumption CLOSED.
.c: 1c lines (target/bad/<stdint.h>) deleted, not commented. Clean build 0 errors / 0 warnings.
  FLASH 200 B (= text 196 + .data image 4), RAMB 8 B. size: text 196 data 4 bss 4.
  
### 1d.1 LED path read-only audit (halted at main, pc 0x46, GDB x/1wx)
Predictions arrived in the SAME message as results (3 h after request) → UNGRADED (rule: predictions locked before run).
  Sent: AHBCLKCTRL0 0x00208000 "written by our code"; PRESETCTRL0 0x00208000 "Port 4 enabled"; PCR18 0; PDDR 0.
Result:
  AHBCLKCTRL0 0x5000_0200 = 0x000007FF (same as 1a.7 at Reset_Handler) → bit17 PORT4 = 0, bit23 GPIO4 = 0 → clocks GATED.
  PRESETCTRL0 0x5000_0100 = 0x00000000 → PORT4/GPIO4 NOT held in reset.
  PORT4 PCR18 0x5011_A0C8 → "Cannot access memory" (gated → access error).
  GPIO4 PDDR 0x5009_E054 = 0x00000000 (gated, NO error → value not trustworthy).
Bit positions re-checked: RM 14.4.1.12 (bit 23 "Enables the clock for GPIO4", bit 17 "…PORT4"). SET/CLR regs: AHBCLKCTRLSET0 0x220, PRESETCTRLSET0 0x120 (RM Table SYSCON map).
Learned:
  - Our code never writes SYSCON; the ROM's 0x7FF reaches main unchanged. Of the 3 gates, only the clock is closed.
  - Gated-read behaviour is per peripheral: PORT4 errors, GPIO4 and WWDT0 read without error → a value from a gated block is not evidence.
  - 0x00208000 = bits 21+15; bits 17+23 = 0x00820000. Decode your own predicted value (repeat of 1a.7 lesson).
  - Clock gate (AHBCLKCTRL, 1 = on) ≠ reset control (PRESETCTRL, 1 = held in reset).
Open (updated): gated-read semantics → per-peripheral; RM chapter searched, no statement found.

### 1d.2a Open PORT4/GPIO4 clock gate from GDB (halted at main, pc 0x46)
Write: set {unsigned int}0x50000220 = 0x00820000  (AHBCLKCTRLSET0, write-1-to-set, RM 14.4.1.16)
Predictions arrived in the SAME message as results → UNGRADED (2nd time). New rule: predictions → tutor replies "locked" → run.
  Sent: 0x00820000|0x7FF = 0x008207FF; PCR18 0, MUX 0000b ALT0 GPIO; PDDR "ROM set no direction bit" (no value).
Result:
  AHBCLKCTRL0 = 0x008207FF (bits 17 PORT4 + 23 GPIO4 now 1; other bits unchanged → SET reg works)
  PORT4 PCR18 0x5011_A0C8 = 0x00000000 (was "Cannot access memory" in 1d.1)
  GPIO4 PDDR  0x5009_E054 = 0x00000000
RM 57.6.1.23: PORT4 PCR18 reset 0000_0000h; MUX[11:8] 0000b = Alternative 0 (GPIO); IBE bit 12 (input buffer) = 0.
Learned:
  - PORT4 gated → access error; clocked → reads. Cause = clock gate (single-variable change, 1 sample).
  - P4_18 is GPIO out of reset (MUX = ALT0) → no pin-mux write needed for the LED.
  - LED off because PDDR bit 18 = 0 (input).
  - GPIO4 gated-read semantics still UNRESOLVED: real PDDR was 0 too, so 1d.1's 0 proves nothing either way.
    Queued test: PDDR bit 18 = 1, clear clock (AHBCLKCTRLCLR0 0x5000_0240), re-read.
Tutor corrections:
  - Proposed 1d.2b "set PCR18 MUX to GPIO" assumed MUX needed changing; reset value is already GPIO. Step dropped.
  - Claimed 1d.2a would test GPIO gated reads; it could not (true value also 0).
  
### 1d.2b P4_18 → output from GDB (halted at main; clocks still on from 1d.2a)
Write: set {unsigned int}0x5009E054 = 0x00040000  (GPIO4 PDDR bit 18)
Predictions arrived in the SAME message as results → UNGRADED (3rd time).
  Sent: PDDR 0x00040000; PDOR 0 (reset, never written); LED ON (PDOR bit18 = 0 → pin low → current flows).
Result: PDDR = 0x00040000; PDOR 0x5009_E040 = 0x00000000; red LED D13 ON, bright red (visual).
PASS (register read-back + visual).
Learned:
  - Setting PDDR makes the pin drive PDOR's current bit at once → direction change alone lit the LED (PDOR reset 0).
  - Path confirmed: VDD_BOARD → R166 0 Ω → D13 red → R167 680 Ω → P4_18 (schematic "RGB LED"; no DNP parts).
    Pin low → LED on (consistent with active-low; 1d.2c drives high to complete the proof).
  - Inference: I ≈ (3.3 − ~1.9) / 680 ≈ 2 mA (typical values, not datasheet-checked).
  - GDB writes go through the debug port; no CPU instruction (str) executes while halted.
  - No external wiring needed: LED is on-board; the mikroBUS question only ruled out a second driver on P4_18.
Order for the 3 LED gates: clock (AHBCLKCTRL0) → reset (already released) → mux (already GPIO) → direction (PDDR).

### 1d.2c Drive P4_18 high with PSOR (GDB, halted at main) — first graded step of session 3
Predictions LOCKED before run (10:54). Score 2/3.
  #1 PSOR reads 0 "write-only" → value ✅, reason ½: RM map lists PSOR RW; diagram R = 0 → "writable, reads 0".
  #2 PDOR 0x00040000 via set-alias, no RMW → ✅
  #3 LED off because pin high → ½: missing ~0 V across D13 + R167 < Vf → no current.
Write: set {unsigned int}0x5009E044 = 0x00040000
Result: PSOR 0x5009_E044 = 0x00000000; PDOR 0x5009_E040 = 0x00040000; LED OFF.
PASS. Active-low PROVEN: 1d.2b pin low → ON; 1d.2c pin high → OFF.
Learned:
  - SET registers: write 1 → bit set in PDOR; writing 0 changes nothing; reads return 0 (RM 55.7.1.10).
  - Register map "RW" + diagram R row all-zero = writable, reads 0. Not "write-only". Quote the source you cite.
  - LED fully dark → VDD_P4 ≈ VDD_BOARD (inference).
RM conflict found (tutor, while checking): 57.3.1 "PCRn[MUX] = 0 → disabled/analog, input buffer off" vs
  57.6.1.23 "0000b = Alternative 0 (GPIO)" + separate IBE bit 12. Output works at MUX 0 (1d.2b/c). Input side → 1d.2d.
Process: predictions → "locked" → run worked. Keep it.

### 1d.2d PDIR with pin driven high, PCR18 = 0 (MUX 0, IBE 0) — read only
Predictions LOCKED (11:08). Score 1/1 (+#2 graded in 1d.2e).
  #1 PDIR 0x00000000: PDIR text "logic zero or not configured for use by a digital function" + IBE = 0 → ✅
  #2 bit 18 = 1 if IBE = 1 → pending (1d.2e)
Result: PDIR 0x5009_E050 = 0x00000000 while PDOR bit18 = 1, PDDR bit18 = 1, LED OFF.
PASS.
Learned:
  - Output and input are separate paths: pin drives high, yet PDIR reads 0 → input buffer off.
  - All GPIO4 PDIR bits 0: every PORT4 PCR resets to 0 → all input buffers off (inference).
  - Can't yet blame MUX 0 (RM 57.3.1) vs IBE 0 (RM 57.6.1.23): both were 0. Change one variable → 1d.2e.
  
  
### 1d.2e Input buffer on: PCR18 = 0x00001000 (IBE = 1, MUX = 0) — GDB, halted at main
Predictions LOCKED (11:18). Score 3/3. Also closes 1d.2d #2 (bit18 = 1 with IBE) ✅ → 1d.2d final 2/2.
  #1 PCR18 0x00001000 ✅  #2 PDIR 0x00040000 (others 0, IBE off) ✅  #3 LED off, IBE input-only ✅
     (wording: pin high = VDD_P4, not VDD_BOARD; equal only by inference from 1d.2c)
Write: set {unsigned int}0x5011A0C8 = 0x00001000
Result: PCR18 = 0x00001000; PDIR = 0x00040000; LED OFF.
PASS. RM CONFLICT RESOLVED (one-variable change: only IBE flipped, MUX stayed 0):
  - IBE (PCR bit 12) gates the digital input buffer; MUX 0000b = Alternative 0 GPIO is a digital mode (RM 57.6.1.23 correct).
  - RM 57.3.1 "PCRn[MUX] = 0 → disabled/analog, input buffer off" does NOT match this silicon.
    Inference: legacy Kinetis text. Not confirmed.
Rule for this chip: to read a GPIO pin, set PCRn.IBE = 1; otherwise PDIR reads 0.
Learned: change one variable at a time; 1d.2d alone could not separate MUX from IBE.

### 1d.2f Toggle with PTOR (GDB, halted at main; PCR18 IBE = 1)
Predictions LOCKED (12:10). Score 3/4.
  #1 PTOR reads 0x00040000 "RW → reads back" → ❌ reads 0 (RM 55.7.1.12 diagram R row all 0; cited 55.7.1.1 = wrong section)
  #2 PDOR 0 (bit18 1 → toggled) ✅  #3 PDIR 0 (IBE on, pin low) ✅  #4 LED ON ✅
Write: set {unsigned int}0x5009E04C = 0x00040000
Result: PTOR 0x5009_E04C = 0x00000000; PDOR = 0x00000000; PDIR = 0x00000000; LED ON (red).
PASS. PTOR flips PDOR bits with one write; PDIR tracks the pin (IBE = 1) → output and input agree.
Learned:
  - Read behaviour comes from the diagram's R row, not the map's RW column. PSOR/PCOR/PTOR = action registers, read 0.
  - Repeat error: same lesson as 1d.2c #1 → re-test in Phase 1 self-test.
State now: AHBCLKCTRL0 0x008207FF, PCR18 0x1000, PDDR 0x40000, PDOR 0, LED ON.

### 1d.2g Gate GPIO4 clock with PDDR = 0x00040000 (GDB, halted at main)
Predictions LOCKED (13:17). Score 2/3.
  #1 AHBCLKCTRL0 0x000207FF → value ✅, reason ½: "bits 16 + 17 set" wrong; 0x00020000 = bit 17 only (3rd decode slip)
  #2 PDDR 0 (gated → bus answers 0) → ✅ value; claim proven only by 1d.2g-b
  #3 LED stays ON → value ✅, reason ½: toggle was 1d.2f not 1d.2e; credited the PORT4 clock (unverified).
     Simpler: GPIO PDDR/PDOR flip-flops hold their state unclocked and drive the pad (inference).
Write: set {unsigned int}0x50000240 = 0x00800000  (AHBCLKCTRLCLR0, W1C, RM 14.4.1.17)
Result: AHBCLKCTRL0 = 0x000207FF; PDDR = 0x00000000 (no error); LED ON.

### 1d.2g-b Re-enable GPIO4 clock
Predictions LOCKED (13:27). Score 1.5/2.
  #1 bits 0–10, 17, 23 (= 0x008207FF; typed value missing a digit) → bits ✅, reason ½: reset is 0x603; ROM set bits 2–8 (1a.7)
  #2 PDDR 0x00040000, register never cleared; LED-on as evidence → ✅
Write: set {unsigned int}0x50000220 = 0x00800000
Result: AHBCLKCTRL0 = 0x008207FF; PDDR = 0x00040000.
PASS. OPEN QUESTION CLOSED (for GPIO4/PORT4, 1 sample each):
  - Gated GPIO4 read → 0, NO error; register contents retained; pin keeps driving.
  - Gated PORT4 access → error (1d.1/1d.2a).
  - → Gated-read behaviour is per peripheral. A missing clock enable on GPIO gives silent zeros, not a fault.
Still unknown: WWDT gated reads (1a.7).

## Session 4 (2026-10-15 / 2026-10-16)

### Log housekeeping (found at session-4 start, not yet done)
- Session 2 blocks appear twice: `###` copy (1a.6–1b.4c) and a flattened copy under "Session 2 (2026-10-14)".
- Session 3 blocks appear twice: `###` copy (1d.0–1d.2g-b) and a flattened copy under "Session 3".
- 1c exists only in the flattened copy → move it into a `###` block first, then delete the flattened copies.

### 1d.3 Light the LED from C code (session 4, 2026-10-15)
Code (main, after float lines): REG32(SYSCON_AHBCLKCTRLSET0 0x50000220) = (1u<<17)|(1u<<23);
  REG32(GPIO4_PDDR 0x5009E054) = 1u<<18; REG32(GPIO4_PCOR 0x5009E048) = 1u<<18;
  REG32(a) = (*(volatile uint32_t *)(a)).
Code check before build (tutor):
  - 0x5000_0220u: C has no '_' digit separator → compile error. Fixed.
  - Bits 10/16 copied from LPC55S16 → wrong chip. MCXN236: PORT4 = 17, GPIO4 = 23 (RM 14.4.1.12, log 1d.2a). Fixed.
  - Generic PDDR/PCOR macros would clobber CMSIS GPIO_Type members in 1e → prefixed GPIO4_/SYSCON_.
  - Comment swapped names (bit 17 = PORT4, 23 = GPIO4) → fixed in 1d.4.
Predictions LOCKED (19:41). Score 2/3 (P4 ungraded).
  P1 FLASH 2000 B, 0 warnings → ½ (0 warnings ✅; 236 B actual; "three registers" isn't a size reason)
  P2 LED off at main → value ✅, reason ½ (cause = PDDR bit18 = 0 after reset; flash reset erased GDB state)
  P3 LED on after PDDR line, pin drives PDOR → ✅ (PDOR value not given → that part ungraded)
  P4 "would not change", no values → UNGRADED (AHBCLKCTRL0 did change 0x7FF → 0x008207FF)
Result: build 0 errors / 0 warnings; FLASH 200 → 236 B; size text 196 → 232, data 4, bss 4.
  main pc 0x46: LED off. Clock line at pc 0x60. LED ON at the PDDR line. PDOR 0x00000000.
  for(;;) pc 0x78: AHBCLKCTRL0 0x008207FF; PDDR 0x00040000; PCOR 0x00000000; AHBCLKCTRLSET0 reads 0.
PASS (register reads + visual).
Learned:
  - Own code now does what GDB did in 1d.2: ROM bits 0–10 + bit 17 PORT4 + bit 23 GPIO4.
  - LED came on at PDDR (PDOR reset 0); PCOR was a no-op on the pin. Good practice: set the level
    (PCOR/PSOR) BEFORE the direction (PDDR) so the pin starts at a known value.
  - +36 B = 3 × (8 B instr: ldr literal + mov.w imm + str) + 3 × 4 B address literals (confirmed by objdump in 1d.4).
  - Take bit positions from THIS chip's RM / our log, never from a sister chip (LPC55 ≠ MCXN SYSCON map).
  - Predict sizes by counting instructions + literals; predictions need values to be graded.
  - Read the address asked for: SET register 0x...220 ≠ AHBCLKCTRL0 0x...200.

### 1d.4 Blink with PTOR + volatile busy-wait (session 4, 2026-10-16)
Code: setup order clock SET → PCOR → PDDR (1d.3 lesson). Loop: PTOR bit18; for (volatile uint32_t i=0; i<1000000u; i++){}.
  g_init/g_zero/float first removed by user, restored on tutor advice (one-variable change).
  Build: FLASH 236 → 272 B; size text 232 → 268, data 4, bss 4. 0 errors / 0 warnings.
objdump main (& $od --disassemble=main works): setup writes 0x60/0x68/0x70 = ldr literal + mov.w imm + str (8 B)
  + 4 B literal → 1d.3 inference CLOSED. PTOR 0x78–0x7E; i=0 0x80–0x84; loop body 0x86–0x92 (7 instr:
  ldr/adds/str i in RAMB, ldr i, ldr limit from flash, cmp, bls); 0x94 b.n 0x78.
  Limit literal 0x000F423F = 999999 (GCC: i < 1000000 → i <= 999999, bls).
Predictions LOCKED (11:40). Score 2.5/5.
  P1 0x86–0x92, 7 instr → ✅
  P2 13 cycles/iter (costs guessed, uncited) → ❌ measured 15
  P3 13,000,020 → ❌ measured 15,000,042 (structure right; 1M iterations right)
  P4a LED OFF during first delay (PTOR 0→1, pin high) → ✅
  P4b 18 ON flashes/10 s at 48 MHz → ½ (method + clock source right; value off from P2)
Result: stop at main pc 0x46, LED OFF. DEMCR 0x011007F0 (TRCENA 1); DWT_CTRL 0x40000001 (NUMCOMP 4, CYCCNTENA 1)
  → debugger had already enabled CYCCNT; set-writes were no-ops.
  Hit 1 @0x78: CYCCNT 0x00000117, LED ON. Hit 2 @0x78: CYCCNT 0x00E4E301, LED OFF.
  Δ = 15,000,042 = 15 × 1,000,000 + 42. Free run: 17 and 16 ON flashes / 10 s.
PASS (CYCCNT + visual).
Learned:
  - Delay iteration = 15 cycles (CPI ≈ 2.1 for 7 instructions). Never guess instruction costs; measure them with CYCCNT.
  - Clock ≈ 48 MHz ± 6% (16 flashes predicted at 48 MHz, 16–17 counted). Rules out 12 MHz (Phase 0) and 96 MHz.
    Not proof → SCG registers in 1e.
  - LED sequence: off at main (PDDR 0) → on after PCOR+PDDR → off after first PTOR. Active-low again.
  - CYCCNT stops while halted → breakpoint-to-breakpoint Δ counts only executed cycles.
Open:
  - Extra 2 cycles/iter: flash wait states on the literal load and/or branch-target fetch? (inference) Test: loop or limit in RAM.
  - DEMCR bits 4–10 = vector catch (DDI0553 layout, tutor from memory, verify) → debugger halts on faults by itself.

## Session 5 (2026-10-16 / 2026-10-17)

### Session-5 intake notes
- UM12041 Rev. 3.0 attached mid-session (missing from the first attachment set).
- learning_log.md: 1d.3 and 1d.4 blocks present, but 3 lines are older than session4_log_blocks.md:
  1d.3 "Comment still swaps names → fix" (fixed in 1d.4); 1d.3 "+36 B … objdump -d not yet run" (confirmed in 1d.4);
  1d.4 "g_init/g_zero/float kept" (session 4: first removed by user, restored on tutor advice).
  → Replace both blocks with the session4_log_blocks.md versions.
- Log housekeeping still pending: session 2/3 blocks appear twice; move 1c into a ### block, delete flattened copies.
- No source files changed in session 5 (.ld, .S, .c identical to end of session 4). All 1e work so far is GDB only.

### 1e.1 VCOM path from the documents (session 5, 2026-10-16) — docs only, no hardware
Predictions LOCKED (17:27 + addendum 17:58). Score 3/5 (P4 ungraded).
  P1 TX = P1_9 → ½ (value ✅; no reason)
  P2 RX = P1_8 → ½ (value ✅; no reason)
  P3 FC4, "pins 11/12" → ½ (FC4 ✅; 11/12 = camera header J9 pin numbers, UM Table 26; answer P1_8 = FC4_P0, P1_9 = FC4_P1)
  P4 "ALT0 for P4_18" + full ALT lists → UNGRADED (no value, wrong pin). Answer ALT2 = MUX 0010b.
  P5 R53→P1_9, R54→P1_8, R55/R56 DNP→P4_2/P4_3 → ✅ (values 0 Ω not predicted → ungraded part)
  P6 JP6 open → ½ (value ✅; no reason)
  Process: user answered "No" to "P4–P6 written after opening the docs?". P4's ALT lists match RM Table 64 text.
Result:
  Schematic sheet 3 (MCU_PORT_0_1_2_3): P1_8 → P1_8/FC4_RXD-MCULINK_VCOM_RX [4,12] + P1_8/SmartDMA_PIO4-CAMERA_D4;
    P1_9 → P1_9/FC4_TXD-MCULINK_VCOM_TX [4,12] + P1_9/SmartDMA_PIO5-CAMERA_D5.
  Sheet 4 (MCU_PORT_4_5_A): MCU_UART_TX [8] — R53 0 — P1_9/FC4_TXD-MCULINK_VCOM_TX
                            MCU_UART_RX [9] — R54 0 — P1_8/FC4_RXD-MCULINK_VCOM_RX
                            R55 0 DNP — P4_2/FC2_UART_TXD-MCULINK_VCOM_TX; R56 0 DNP — P4_3/FC2_UART_RXD-MCULINK_VCOM_RX
  Sheet 9 (MCU_LINK_DEBUG): MCU_UART_TX ↔ MCU_LINK_RX, MCU_UART_RX ↔ MCU_LINK_TX (tutor-read).
  RM 9.2.1 Table 64 (p.105): P1_8 ALT2 FC4_P0, ISP UART_RXD, MED+I2C+I3C; P1_9 ALT2 FC4_P1, ISP UART_TXD, MED+I2C. Default DIS.
  RM 57.6.1.x PCR MUX: 0010b = Alternative 2. → PCR8/PCR9 of PORT1 need MUX = 0010b (+ IBE=1 on RX, from 1d.2e).
  RM 50.2.3 (p.1719): LPUART mode FC_P0 = RXD, FC_P1 = TXD.
  RM 12.2.3 Table 157: boot ROM UART ISP = FC4 on P1_9 (TXD) / P1_8 (RXD).
  UM12041 Table 3: JP6 open (default) = VCOM enabled; shorted pulls LPC_HW_VER_6 low → VCOM disabled. §3.8: MCU-Link LPC55S69
    → P1_8/P1_9 via R54/R53; P1_8/P1_9 are the default UART ISP pins.
  UM12041 Table 26: P1_9 = camera header J9 pin 11, P1_8 = pin 12.
PASS (three documents agree).
Learned:
  - Off-page refs [n] = sheet numbers where the net continues; follow them until you reach the other chip. Ctrl+F the net name.
  - Board-level routing choice = fitted vs DNP 0 Ω resistors (FC4 route fitted, FC2 route DNP).
  - FC pin roles depend on the mode (LPUART: P0 = RXD, P1 = TXD); the ALT number only selects "FC4_Pm".
  - Every prediction needs a reason; missing reasons cost 2 points here.
Closed: camera header J9 empty (user-confirmed) → MCU-Link is the only other device on P1_8/P1_9.
Open: lead for "Pull ISP on reset": ROM UART ISP uses these same pins via MCU-Link VCOM (inference).

### 1e.2a Core clock from SCG / SYSCON (GDB read-only, halted at main pc 0x46) — session 5, 2026-10-17
Predictions LOCKED (10:11). Score 5/5.
  P1 CSR 0x03000000, SCS FIRC ✅   P2 RCCR 0x03000000 = CSR ✅   P3 FIRCCFG 0 → 48 MHz ✅
  P4 AHBCLKDIV 0 → ÷1, UNSTAB 0 ✅   P5 48 MHz, fits 1d.4 ✅
  Reason note: "ROM programs both" unproven — all values = RM reset values (cannot distinguish ROM write vs untouched; cf. 1a.7).
Result: SCG CSR 0x5004_4010 = 0x03000000 (SCS 0011b FIRC, RM 25.7.1.5, p.802); RCCR 0x5004_4014 = 0x03000000;
  FIRCCFG 0x5004_4308 = 0x00000000 (RANGE 0 = 48 MHz, RM 25.7.1.14); SYSCON AHBCLKDIV 0x5000_0380 = 0x00000000 (÷1, stable, RM 14.4.1.27).
  Core clock = 48 MHz. Check vs 1d.4: 15,000,042 / 48e6 = 0.3125 s per toggle → 16.0 ON flashes / 10 s (counted 16–17).
PASS. CLOSES open question "core clock ≈ 48 MHz ± 6% (visual)".
Learned:
  - FIRC = fro_hf (RM Table 204, p.758: "fro_hf: Clock output from FRO_144M. SCG_FIRCCFG[RANGE] controls the frequency").
  - CSR = active source (read-only, write → transfer error); RCCR = requested source. Equal → no switch pending.
  - Register values = configuration, not measured frequency; the blink count is the independent check.
  - Reset value = value at main here, but that alone doesn't show whether the ROM touched the register.

### 1e.2b FC4 functional clock registers (GDB read-only, halted at main pc 0x46) — session 5, 2026-10-17
Predictions LOCKED (11:14). Score 2/4.
  P1 FCCLKSEL4 0x00000007 (no clock) → ❌ read 0x3. (Alternative "0x3 if ROM configured FC4" was right; first value graded.)
  P2 FLEXCOMM4CLKDIV 0x40000000, HALT 1 → ✅
  P3 FROHFDIV 0x40000000, halted → ❌ read 0x0 (running, ÷1). Reason = RM reset value only (weak point: reset vs ROM-handed).
  P4 SEL 3, FROHFDIV.DIV 0, FLEXCOMM4CLKDIV.DIV 0, clear both HALTs → FC4 48 MHz → ✅ (design; verify in 1e.4)
Result: FCCLKSEL4 0x5000_02C0 = 0x00000003 (SEL 011b fro_hf_div; RM 14.4.1.23 p.282, reset SEL 111b = no clock, bits 31:3 'u')
  FLEXCOMM4CLKDIV 0x5000_0860 = 0x40000000 (HALT 1 → divider stopped, DIV 0; = RM 14.4.1.69 p.330 reset)
  FROHFDIV 0x5000_0388 = 0x00000000 (HALT 0 running, DIV 0 ÷1, UNSTAB 0; RM 14.4.1.29 p.287 reset 0x40000000)
  Clock chain (RM 24.3.17 Fig 83, p.768): fro_hf 48 MHz → FROHFDIV ÷1 (running) → FCCLKSEL4 = fro_hf_div → FLEXCOMM4CLKDIV (HALTED) → FC4.
PASS (register reads).
Learned:
  - FCCLKSEL4 and FROHFDIV were changed before main; FLEXCOMM4CLKDIV was not. Inference: boot ROM prepared FC4 (its UART ISP port, RM Table 157)
    — or the debugger connect script; not distinguished.
  - FC4 still has no functional clock: its divider is halted. One closed stage blocks the chain (cf. 1d.1).
  - Our init writes all three registers explicitly; never rely on ROM-left state.
  - Commit to one predicted value; hedged alternatives aren't graded.
Open: who set FCCLKSEL4/FROHFDIV: ROM or debugger? Test: read at Reset_Handler breakpoint after Restart (1a.5b method).

### 1e.3a UART gate audit (GDB read-only, halted at main pc 0x46) — session 5, 2026-10-17
Predictions LOCKED (11:35). Score 4.5/5.
  P1 AHBCLKCTRL0 0x7FF, bit14 0 ✅   P3 PRESETCTRL0 0, bit14 0 ✅   P4 PRESETCTRL1 0, bit15 0 ✅
  P2 AHBCLKCTRL1 0x0, bit15 0 → ½ (bit15 ✅; value 0x20000000; reason = RM reset value → weak point reset vs ROM-handed, 3rd time)
  P5 PCR8/PCR9 "Cannot access memory" (PORT1 gated) ✅
Result: AHBCLKCTRL0 0x5000_0200 = 0x000007FF (bit14 PORT1 = 0, RM 14.4.1.12)
  AHBCLKCTRL1 0x5000_0204 = 0x20000000 (bit15 FC4 = 0; bit29 = 1 but RESERVED, reset 0 — RM 14.4.1.13 p.266)
  PRESETCTRL0 0x5000_0100 = 0 (bit14 PORT1_RST 0); PRESETCTRL1 0x5000_0104 = 0 (bit15 FC4_RST 0, RM 14.4.1.7 p.252)
  PORT1 PCR8 0x5011_70A0 / PCR9 0x5011_70A4 → "Cannot access memory" (PORT1 base S 0x5011_7000; RM map PCR8 A0h, PCR9 A4h, reset 0)
PASS.
Learned:
  - UART path gates at main: PORT1 clock OFF, FC4 clock OFF, both resets released, FC4 functional divider HALTED (1e.2b).
  - Gated PORT access → error: 2 samples (PORT4 1d.1, PORT1 here).
  - Reserved bits can be 1 at main (AHBCLKCTRL1 bit29; cf. AHBCLKCTRL0 bits 6–8). Use SET/CLR registers, never '=' on AHBCLKCTRLn.
Open: AHBCLKCTRL1 bit29 (reserved) set before main — by ROM or debugger? Purpose unknown.

### 1e.3b Open PORT1 + FC4 bus clock gates (GDB writes, halted at main) — session 5, 2026-10-17
Writes: set {unsigned int}0x50000220 = 0x00004000 (AHBCLKCTRLSET0 bit14 PORT1)
        set {unsigned int}0x50000224 = 0x00008000 (AHBCLKCTRLSET1 bit15 FC4)  — RM 14.4.1.16 (SET0 220h, SET1 224h)
Predictions LOCKED (12:58 + addendum 13:04). Score 2/4.
  P1 AHBCLKCTRL0 0x00004000 → ❌ 0x000047FF (= 0x7FF | 0x4000)
  P2 AHBCLKCTRL1 0x00008000 → ❌ 0x20008000 (= 0x20000000 | 0x8000; bit29 reasoning was attached to AHBCLKCTRL0)
  P3 PCR8 0 ✅   P4 PCR9 0 ✅
  Process: first send had two values (P1) and descriptions only (P3/P4); addendum made P3/P4 gradable.
  print/x $pc not pasted; pre-setup state inferred from AHBCLKCTRL0 lacking bits 17/23.
Result: AHBCLKCTRL0 0x000047FF; AHBCLKCTRL1 0x20008000; PORT1 PCR8 0x5011_70A0 = 0; PCR9 0x5011_70A4 = 0 (now readable).
PASS.
Learned:
  - SET register write → target = old | bits. Predict the TARGET register, not the value written (cf. 1d.2a). NEW weak point.
  - Keep bits with their register: bit29 belongs to AHBCLKCTRL1 (1e.3a).
  - PORT1 gated → error; clocked → reads (2nd sample after PORT4).
  - PCR8/PCR9 = 0 at main → P1_8/P1_9 GPIO, IBE off; ROM did not leave them in UART mode (normal boot, inference).
  - Remaining UART gates: pin mux (PCR8/9 → ALT2, IBE on RX), FC4 functional divider HALT.

### Theory interlude (2026-10-17)
User asked for a full overview before 1e.3c. Tutor published "MCXN236 Register to Pin" (https://claude.ai/artifact/72Fy79CYu9xwfbzNWokNfA):
six layers (bus clock gate, reset, functional clock, pin mux, block setup, board), port vs pin vs GPIO/PORT, clock tree with
1e.2 values, register kinds (plain / action SET-CLR / read-only / reserved), UART 8N1 frame, LED vs UART chain table,
boot sequence, troubleshooting order. Linked from the Bench Map references.
Also verified for 1e.4: LPUART baud = baud clock ÷ ((OSR+1) × SBR) (RM 50.5.5.6, p.1871);
LP_FLEXCOMM init order (RM 50.2.4, p.1719): bus clock → functional clock → PSELID[PERSEL] → module config;
PSELID at FC base + 0xFF8, reset 0x0010_3070 (RM 50.2.6.1.1, p.1720). LP_FLEXCOMM4 base NS 0x400B_4000 (S 0x500B_4000).
Candidate: 48 MHz ÷ (16 × 26) = 115,385 baud (+0.16 % vs 115,200) — to be checked against the RM in 1e.4.

Session 5 tutor corrections: none of substance.

## Session 6 (2026-10-21 / 2026-10-22)

### Session-6 intake notes
- learning_log.md: all session 5 blocks (1e.1–1e.3b) present, but TWICE: an older ### copy at the end of the session-4 section
  (no session header; its 1e.1 still lists J9 as open) and the appended "## Session 5" section. Keep the appended section, delete the older copy.
- Housekeeping (1) session 2/3 duplicates + move 1c into a ### block, and (2) 1d.3/1d.4 stale lines: still pending.
- Source files at session start identical to end of session 4 (.ld, .S, .c 1d.4 blink).

### 1e.3c Pin mux P1_8/P1_9 → LP_FLEXCOMM4 ALT2 (GDB writes, halted at main pc 0x46) — session 6, 2026-10-22
Composition (user, tutor-checked): PCR8 = IBE 1 + MUX 0010b → 0x00001200; PCR9 = MUX 0010b → 0x00000200; LK (bit15) = 0
  (LK = lock until next reset). First attempts: bit 12 written as 1000 0000 0000b (= bit 11, inside MUX);
  0x1028/0x1029 = ALT2 in bits 7:4 + pin number in 3:0 → decodes to GPIO + ODE + SRE (+PS). Fixed via 16-bit grid.
Setup: redo 1e.3b SET writes → AHBCLKCTRL0 0x000047FF, AHBCLKCTRL1 0x20008000 ✓; PCR8/PCR9 before = 0 / 0.
Writes: set {unsigned int}0x501170A0 = 0x1200 ; set {unsigned int}0x501170A4 = 0x200
Predictions LOCKED (11:45). Score 2/2.
  P1 PCR8 0x00001200, fields read back (RM 57.6.1.18) ✅   P2 PCR9 0x00000200 (RM 57.6.1.19) ✅
Result: PCR8 0x5011_70A0 = 0x00001200; PCR9 0x5011_70A4 = 0x00000200.
PASS (register read-back).
Learned:
  - Bit n = a 1 followed by n zeros = 2^n (bit 12 = 0x1000). Fill a 16-bit grid; MUX [11:8] = 3rd hex digit from the right.
  - The pin number is chosen by the PCR ADDRESS (A0h = PCR8, A4h = PCR9), never by bits in the value.
  - PCR fields are RW (diagram fields span R and W rows) → read back; reserved bits (31:16, 14, 7) read 0.
  - Chain: Table 64 (NXP silicon: FC4_P0 → P1_8 ALT2, FC4_P1 → P1_9 ALT2) + board (R54/R53 to MCU-Link) +
    our MUX write + FC4 mode (RM 50.2.3: LPUART P0 = RXD, P1 = TXD). FC5 (ALT3) lands as P4/P5 = DSR_b/DTR_b → unusable.
  - Pin P1_8 is a general-purpose pad; MUX picks which internal signal reaches it. FC_P0..P6 are generic lines whose meaning
    depends on the FlexComm mode. Table 64 binds instance+line to package pins; chapter 50 gives the meaning.
  - UART lines cross over: MCU-Link TX → P1_8 (MCU RX); P1_9 (MCU TX) → MCU-Link RX. Net names are from the MCU's view.
  - Hex is notation: one hex digit = 4 bits, so fields line up with digits; GDB x/1wx prints hex.
Open (inference): P1_9 likely undriven until FC4 has a mode (PSELID) and a functional clock.

### 1e.3d FC4 functional clock from GDB (halted at main pc 0x46) — session 6, 2026-10-22
Composition (user, tutor-checked): FROHFDIV 0x5000_0388 = 0 (HALT 0, DIV 0); FCCLKSEL4 0x5000_02C0 = 3 (SEL 011b fro_hf_div);
  FLEXCOMM4CLKDIV 0x5000_0860 = 0 (HALT 0, RESET 0, DIV 0). First attempt FLEXCOMM4CLKDIV = 0x40000000 (HALT 1 = stopped) → fixed.
  Chain first written as FROHFDIV → CLKDIV → CLKSEL; RM 24.3.17 Fig 83 (p.768): fro_hf_div → FCCLKSEL4 mux → FLEXCOMM4CLKDIV → FC4.
  Write order = signal order; divider HALT released LAST so FC4 never sees a half-configured clock.
Setup: 1e.3b SET writes + 1e.3c PCR writes redone (new session); AHBCLKCTRL0 0x47FF, AHBCLKCTRL1 0x20008000, PCR8 0x1200, PCR9 0x200 ✓.
Predictions LOCKED (14:53). Score 3/3.
  P1 FROHFDIV 0 ✅   P2 FCCLKSEL4 3 ✅   P3 FLEXCOMM4CLKDIV 0 ✅ (reasons: RW fields read back, RM 14.4.1.29/.23/.69)
Result: before 0x00000000 / 0x00000003 / 0x40000000 (= 1e.2b); after 0x00000000 / 0x00000003 / 0x00000000.
PASS (register read-back). FC4 functional clock configured = 48 MHz (not measured; baud test in 1e.4).
Learned:
  - A read-back equal to the old value doesn't prove the write happened (FROHFDIV, FCCLKSEL4); only a changed value does (CLKDIV).
  - FROHFDIV has no RESET bit; FLEXCOMMnCLKDIV has HALT 30 / RESET 29 / DIV 7:0; UNSTAB 31 = status.
  - GDB-only UART gates complete: bus clocks, resets, functional clock, pin mux. Remaining: PSELID + LPUART config (1e.4).

### 1e.3e UART gates in C (bus clocks, resets, FC4 clock, pin mux) — session 6, 2026-10-22
Decision: option A (own prefixed macros) for 1e.3e; CMSIS device header revisited at 1e.4.
Code (after LED setup, before for(;;)): SET0 = 1u<<14 (PORT1); SET1 = 1u<<15 (FC4); PRESETCTRLCLR0 = 1u<<14; PRESETCTRLCLR1 = 1u<<15;
  FROHFDIV = 0; FCCLKSEL4 = 3u; FLEXCOMM4CLKDIV = 0; PCR8 = (2u<<8)|(1u<<12); PCR9 = 2u<<8.
  New macros: SYSCON_AHBCLKCTRLSET1 0x50000224, PRESETCTRLCLR0/1 0x50000140/144 (RM 14.4.1.11), FROHFDIV 0x50000388,
  FCCLKSEL4 0x500002C0, FLEXCOMM4CLKDIV 0x50000860, PORT1_PCR8/9 0x501170A0/A4.
Code check before build (tutor): core_cm33.h include (not in project) → removed; header history replaced → restored;
  unprefixed names → compile errors; SET0 bit 15 / SET1 bit 16 → wrong bits (correct 14 / 15, log 1e.3b 0x4000/0x8000);
  CLR registers written with 0 → no-op (write-1-to-clear). All fixed.
Predictions LOCKED (18:08), no reasons, no addendum. Score 3/9.
  P1 0/0, FLASH 300 B → ❌ (368 B)   P2 AHBCLKCTRL0 0x4000 → ❌ (0x008247FF)   P3 AHBCLKCTRL1 0x8000 → ❌ (0x20008000)
  P4 PRESETCTRL0 0 ½   P5 PRESETCTRL1 0 ½   P6 CLKDIV 0 ½   P7 PCR8 0x1200 ½   P8 PCR9 0x200 ½   P9 blinking ½ (count ungraded)
Process: first run halted at pc 0x78 (= first UART write in the new image; stale IDE breakpoint, inference) → reads discarded, rerun.
  First rerun paste read PCR8 twice instead of PCR9 → missing items requested before grading.
Result: build 0 errors / 0 warnings; FLASH 272 → 368 B; size text 364 data 4 bss 4. x/4i 0x78 = new image.
  main pc 0x46. Bp *0xba (PTOR, loop top): LED ON. AHBCLKCTRL0 0x008247FF; AHBCLKCTRL1 0x20008000; PRESETCTRL0 0; PRESETCTRL1 0;
  FLEXCOMM4CLKDIV 0x00000000; PCR8 0x00001200; PCR9 0x00000200. Free run: 16 + 16 ON flashes / 10 s (unchanged from 1d.4).
PASS (register reads + visual). Layers 1–4 of the UART path are now set by firmware and survive reset.
objdump main: 0x40 push; 0x46 first C line; LED writes 0x60/0x68/0x70; UART block 0x78–0xb9 (66 B); PTOR 0xba (loop top);
  i=0 0xc2; loop body 0xc8–0xd4; 0xd6 b.n 0xba; literals 0xd8–0x110 (SET0 literal 0xe0 reused by the UART block).
Learned:
  - SET write → TARGET = old | bits — 2nd miss (1e.3b). ROM-handed value at main — 5th miss. Top of Phase 1 self-test.
    AHBCLKCTRL0 = 0x7FF (ROM) | 0x820000 (LED) | 0x4000 (PORT1) = 0x008247FF; AHBCLKCTRL1 = 0x20000000 | 0x8000.
  - Size: large immediate = ldr+mov.w+str (8 B); value 0–255 = ldr+movs+str (6 B); identical literals are shared; pool alignment pad.
    +96 B = 6×8 + 3×6 + 8×4 − 2 (pad present in 1d.4, gone now).
  - Every prediction needs a reason: 6 correct values scored ½ (−3).
  - PRESETCTRL CLR writes are no-ops today (already 0): their effect can't be seen (cf. 1e.3d).
  - Clear stale IDE breakpoints before a new session (Breakpoints view → Remove All).

### 1e.4a PSELID → UART (paused before composition) — session 6, 2026-10-22
Task issued: user composes the PERSEL value for UART (binary), the full line set {unsigned int}0x500B4FF8 = 0x________,
  and one sentence on read-only fields + LOCK. No prediction or run yet.
Facts (RM 50.2.6.1.3 PSELID, offset FF8h; FC4 S base 0x500B_4000): ID 31:12 RO; I2CPRESENT 6 / SPIPRESENT 5 / UARTPRESENT 4 RO;
  LOCK 3 (locks PERSEL until reset); PERSEL 2:0. RM diagram reset = 0x0010_3070. PERSEL encodings on the next RM page (user to look up).

Session 6 tutor corrections: none of substance. (User's "pad P1_8 wired to MCU-Link TX should be RX" was checked: TX is correct —
  UART crossover, net MCU_UART_RX named from the MCU's view; log 1e.1 sheet 9 "MCU_UART_RX ↔ MCU_LINK_TX".)

## Session 7 (2026-10-23 / 2026-10-25)

### Session-7 intake notes
- learning_log.md: all session 6 blocks present but TWICE: ### copies (1e.3c–1e.3e, no "## Session 6" header) and a flattened
  copy (1e.3c–1e.4a + corrections). 1e.4a + session-6 corrections exist only flattened; flattened copy has extra lines.
  → Housekeeping (5): delete both copies, paste session6_log_blocks.md in their place. (1)–(4) still pending.
- Source files identical to end of session 6 (1e.3e image in flash). No source file changed in session 7 (all 1e.4 work is GDB only).

### 1e.4a PSELID → UART mode (GDB write, halted at bp *0xba) — session 7, 2026-10-23
Composition (user, tutor-checked): PERSEL 001b = UART (RM 50.2.6.1.3 p.1723); LOCK 0 (1 = PERSEL frozen until reset, p.1722,
  cf. PCR LK); ID/I2CPRESENT/SPIPRESENT/UARTPRESENT R-only (empty W row p.1722) → write 0.
  First form 0x00103071 copied the reset diagram into RO fields (works, bad habit) → chose 0x00000001.
  GDB address first written with RM 'h' suffix (0x500B4FF8h → syntax error) → fixed.
Write: set {unsigned int}0x500B4FF8 = 0x00000001
Predictions LOCKED (16:24). Score 1.5/2.
  P1 before 0x00103070 → ½ (value ✅; reason = RM reset value only — weak point, 6th time;
     correct reason: no line of our code writes PSELID, ROM did not leave FC4 in a mode)
  P2 after 0x00103071 → ✅ (PERSEL RW reads back)
Process: first print/x $pc (at main) not pasted; reads accepted (pc 0xba printed before and after).
Result: pc 0xba; AHBCLKCTRL1 0x20008000 (FC4 bus clock on); PSELID 0x500B_4FF8 = 0x00103070 → 0x00103071.
PASS (register read-back, value changed).
Learned:
  - RO fields ignore writes: wrote 0x1, ID 0x00103 and PRESENT 111b unchanged → write only the fields you mean.
  - FC4 supports I2C, SPI and UART (bits 6:4 = 111b). PERSEL 000b at loop top: no mode before our write (ROM changed
    FCCLKSEL4/FROHFDIV but not PSELID; normal boot, inference).
  - RM 'h' suffix is notation; GDB/C need 0x prefix only.
  - Reset value ≠ reason: name what did or didn't touch the register before the halt point.
  - GDB-only for now: redo the PSELID write after every reset until 1e.4e puts it in C.

### 1e.4b LPUART header decision + register map (docs only) — session 7, 2026-10-23
Decision: option A (own prefixed macros, values from the RM) for all LPUART registers in Phase 1; CMSIS revisited in Phase 2.
Register map (RM 50.5.5.1 p.1862–1863; LPUART4 base 400B_4000h = LP_FLEXCOMM4 base, same 4 KB block, PSELID at FF8h):
  LPUART4_BAUD 0x500B4010 reset 0x0F000004 RW | LPUART4_STAT 0x500B4014 reset 0x00C00000 RW
  LPUART4_CTRL 0x500B4018 reset 0x00000000 RW | LPUART4_DATA 0x500B401C reset 0x00001000 RW
  (also: VERID 0h R 0x04040007, PARAM 4h R 0x00000303, GLOBAL 8h, PINCFG Ch, FIFO 28h 0x00C00022, DATARO 30h R)
Check (tutor): STAT address first written as 0x500B4010 (= BAUD) → fixed 0x500B4014.
  Q1: "IDAU sets bit 28" corrected: we pick the alias by address (bit 28 = Secure alias, log 0.3); IDAU attributes the region.
  Q2: RM NOTE p.1862: write to an RO REGISTER / read of a WO register can cause a bus error (not measured).
      ≠ 1e.4a: RO FIELDS inside the RW PSELID ignored writes, no error (measured).
Learned:
  - One peripheral block per 4 KB: LPUART registers from 0h, LP_FLEXCOMM PSELID at FF8h of the same block.
  - Check every address against its own offset (copy slip STAT = BAUD address).
  - Never GDB-write VERID/PARAM/DATARO (RO registers).
Not graded (lookup).

### 1e.4c-1 LPUART4 BAUD = 115200 from 48 MHz (GDB write, halted at bp *0xba) — session 7, 2026-10-23
Composition (user, tutor-checked, 3 rounds): OSR code 01111b (ratio 16, RM 50.5.5.6 p.1868–1869), SBR 26 = 0x1A
  (48e6 / (16 × 115200) = 26.04); baud = 48e6 / (16 × 26) = 115,384.6 → +0.16 % (SBR row p.1871).
  All other fields 0 (MAEN1/2, M10, TDMAE/RDMAE/RIDMAE, MATCFG, BOTHEDGE, RESYNCDIS, LBKDIE, RXEDGIE, SBNS = 1 stop bit).
  Value 0x0F00001A = reset 0x0F000004 with SBR 4 → 26.
  Slips: 28-bit binary for the reset value; "12M" typed for 48M; table filled with the reset SBR (4);
  "OSR code is 16" (code is 01111b = 15, ratio = code + 1); hex first written as 0x0F000004, then 0x004 (SBR 4 again).
  Fixed by assembling the hex from the decode table with two 16-bit grids and back-decoding.
Setup: PSELID redone (lost on reset) → 0x00103071.
Predictions LOCKED (22:52), no reasons, no addendum. Score 0.5/2.
  P1 BAUD before 0x00103070 → ❌ (= PSELID value, wrong register; read 0x0F000004)
  P2 BAUD after 0x0F00001A → ½ (value ✅, no reason)
Process: "continue" typed twice; halted at 0xba as required → reads valid.
Result: pc 0x46 (main) → 0xba; PSELID 0x00103071; BAUD 0x500B_4010 = 0x0F000004 → 0x0F00001A.
PASS (register read-back, SBR changed).
Learned:
  - Assemble the hex from your own decode table, never from the reset value; back-decode before sending.
  - OSR field = ratio − 1 (code 01111b → ×16).
  - Keep each predicted value with its own register/address (P1 used PSELID's value).
  - Reset value is a valid prediction only with "nothing wrote it before the halt" (no code line; ROM left FC4 without a mode).
Open: are LPUART registers accessible while PSELID PERSEL = 000b? (untested)

### 1e.4c-2 LPUART4 CTRL = 8N1 + TE (GDB write, halted at bp *0xba) — session 7, 2026-10-24
Composition (user, tutor-checked, 2 rounds): RM 50.5.5.8 p.1879–1885. M (bit 4) = 0 8-bit, M7 (bit 11) = 0, BAUD M10 = 0 → 8 data bits;
  PE (bit 1) = 0 no parity (PT ignored); TE (bit 19) = 1; RE (bit 18) = 0 (TX only in 1e.4); all others 0 → 0x00080000.
  Slips: M7 given as bit 7 (= LOOPS); TE/RE bit numbers missing; RE reason quoted the "turning RE off" behaviour;
  IDLECFG first 011b with no reason → 000b (receiver off). TXDIR acts only in single-wire mode.
  Write order PSELID → BAUD → CTRL (SBR only while TE = RE = 0, p.1871).
Setup: PSELID 0x00103071, BAUD 0x0F00001A redone.
Predictions LOCKED (12:42), no reasons; reasons arrived WITH results → ungraded. Score 1/2.
  P1 CTRL before 0x00000000 → ½ (late reason "reset value" alone)
  P2 CTRL after 0x00080000 → ½ (late reason "IDLECFG bits 8–10" wrong: value sets TE bit 19)
Process: "continue" typed twice (2nd time); halted at 0xba → reads valid.
Result: pc 0x46 → 0xba; CTRL 0x500B_4018 = 0x00000000 → 0x00080000. PASS (read-back, value changed).
Learned:
  - Reasons go in the prediction message or an addendum BEFORE the run; sent with results they don't count.
  - Decode your own written value before giving its reason (0x00080000 = bit 19 TE).
  - Reset value + "nothing wrote it before the halt" = a reason; reset value alone = ½ (7th time).
State left: halted at 0xba, FC4 = UART 115,384.6 baud 8N1, TE on (GDB-only).

### 1e.4d One byte 'a' out through VCOM (GDB, halted at bp *0xba) — session 7, 2026-10-24/25
Paused 2026-10-24 15:12 during composition (board disconnected); resumed 2026-10-25.
Part A: terminal on MCU-Link VCOM COM port, 115200 8N1, no flow control. (COM3)
Composition (user, tutor-checked): DATA 0x500B401C = 0x00000061 ('a' = 0110 0001b in T7:T0); FRETSC 0 (normal character;
  1 = break if T9 = 0 / idle if T9 = 1 instead of T9:T0, p.1887); T9/T8 0 (8-bit mode, 1e.4c); receive-status fields
  (NOISY, PARITYE, RXEMPT, IDLINE, LINBRK) 0, meaningless on writes. First reasons gave read meanings → corrected.
  p.1886 NOTE: DATA reads = receive FIFO, writes = transmit FIFO (two registers, one address).
  p.1886 Function: "A 16-bit or 32-bit write pushes the data written into the FIFO and does not update the value of
  CTRL[R8T9] or CTRL[R9T8]." (user first quoted the NOTE instead)
Clarified: terminal only displays (RE = 0); one GDB 32-bit write sends the whole byte.
Setup: PSELID 0x00103071, BAUD 0x0F00001A, CTRL 0x00080000 (redone after reconnect).
Predictions LOCKED (00:01) + addendum (00:06, before run; first P2 gave DATA's value, not STAT's). Score 2/3.
  P1 STAT before 0x00C00000 (TDRE 1, TC 1) ✅
  P2 STAT after 0x00800000 (TC 0, transmitter active) ❌ read 0x00C00000
  P3 terminal 'a' ✅
Process: first print/x $pc (main) not pasted; reads at pc 0xba (printed before and after) accepted.
Result: STAT 0x500B_4014 = 0x00C00000 before and after; terminal shows "a".
PASS (terminal + register reads). FIRST UART OUTPUT. End-to-end baud check passed (48 MHz FC4 clock + SBR 26 confirmed).
Learned:
  - One 8N1 character = 10 bits ≈ 87 µs at 115,384 baud; a GDB read takes ms (inference) → TC = 0 window invisible to GDB.
  - STAT before = after does not mean "no transmission" — the terminal is the evidence (cf. 1e.3d).
  - To observe TDRE/TC changing, the CPU must poll them (1e.4e).
  - Predictions need the asked register's value.

### 1e.4e-1 UART init + polled putc in C (issued, not started) — session 7, 2026-10-25
Task issued: user composes (1) macros PSELID/BAUD/CTRL/STAT/DATA (option A, Secure addresses), (2) three init lines in order
  PSELID 0x00000001 → BAUD 0x0F00001A → CTRL 0x00080000 after the pin mux, with a comment on why BAUD precedes CTRL,
  (3) static void uart4_putc(char c): wait while STAT bit 23 TDRE = 0, then write DATA, (4) uart4_putc('a') before for(;;),
  (5) one sentence: why wait on TDRE, not TC (RM p.1874–1875). Tutor checks before build.

Session 7 tutor corrections: none of substance.
Session 7 scores: 1e.4a 1.5/2 · 1e.4b docs · 1e.4c-1 0.5/2 · 1e.4c-2 1/2 · 1e.4d 2/3.
Weak points this session: reset value alone as reason (6th, 7th time); no reasons (1e.4c-1, 1e.4c-2);
  reasons sent with results; reset-value bits copied into composed hex (1e.4a, 1e.4c-1); predicted another register's value
  (1e.4c-1 P1, 1e.4d first P2); "continue" typed twice; first print/x $pc (at main) not pasted (1e.4a, 1e.4d).

### Log housekeeping done (session 7, 2026-10-25, tutor)
- (1) Session 2/3 flattened copies deleted; 1c and "1d (started)" moved into ### blocks (text unchanged, only line breaks added);
  "## Session 2" / "## Session 3" headers added. 1b.4c given a ### heading. Both 1a.6 records kept (first one marked as superseded).
- (2) 1d.3/1d.4 blocks replaced with the session4_log_blocks.md versions (with its "## Session 4" header).
- (3) Older session-5 copy (1e.1–1e.3b at the end of the session-4 section) deleted; "## Session 5" section kept.
- (5) Session 6 ### copies + flattened copy deleted; session6_log_blocks.md pasted in their place.
- (6) Session 7 duplicate (### + flattened) replaced with session7_log_blocks.md; COM3 filled in; "State left" and weak-point lines kept.
- Intake notes from sessions 4–7 that list these items as pending are kept as history.

## Session 8 (2026-10-25 / 2026-10-26)

### Session-8 intake notes
- learning_log.md attached after the block files: sessions 4–7 identical line-for-line to session4–7_log_blocks.md
  (session 7 + documented additions: 1e.4d "State left", COM3, weak-points line). Duplicates only intended
  (1a.6 superseded record; 1e.4a session-6 paused + session-7 run). Cosmetic: no "## Session 1" header; 1a.5c before 1a.5b.
- Source files at session start = end of session 6 (1e.3e image).

### 1e.4e-1 UART init + polled putc in C — session 8, 2026-10-25/26
Composition (user, tutor-checked, 6 rounds):
  Macros: LP_FLEXCOMM4_PSELID 0x500B4FF8 (renamed from LPUART4_PSELID: PSELID belongs to LP_FLEXCOMM, RM 50.2.6.1.3),
    LPUART4_BAUD 0x500B4010, _STAT 0x500B4014, _CTRL 0x500B4018, _DATA 0x500B401C; LPUART4_STAT_TDRE (1u << 23).
  Init after PCR9: PSELID = 1u<<0; BAUD = bits 1,3,4,24–27 (= 0x0F00001A); CTRL = 1u<<19; then uart4_putc('a') before for(;;).
  Comment: "RM 50.5.5.6 p.1871: BAUD[SBR12:SBR0] may only be changed while both CTRL[RE] and CTRL[TE] are 0 —
    so program BAUD before enabling CTRL". First drafts added unverified claims (TE starts baud clock, preamble at reset
    baud, M10/SBNS same rule) and a hedged consequence → removed.
  uart4_putc: while ((REG32(STAT) & TDRE) == 0u) {} ; REG32(DATA) = (uint32_t)(unsigned char)c;
  BUG caught before build: while (STAT & TDRE == 0u) → == binds tighter than & → STAT & ((1u<<23)==0) = STAT & 0 = 0
    → loop never runs, DATA written without waiting. First "fix" parenthesised the comparison (same meaning). Q1–Q3 answered
    correctly. Tutor note (inference): the volatile STAT read is still performed once, it just doesn't gate.
    Saved file still had the old loop at the pre-prediction check → fixed.
  TDRE vs TC (RM p.1874–1875): TDRE = 1 as soon as the buffer can take the next character while the current one shifts out
    ("updated at the start of each transmitted character") → back-to-back; TC = 1 only when buffer empty AND nothing is being
    transmitted (line idle). Tutor correction of user claim: waiting on TC adds an idle gap of the CPU's reaction time
    (< 1 µs at 48 MHz), not "a full character time".
Predictions LOCKED (2026-10-26 09:19) + P2 addendum (09:29, before run step 5). Score 1.5/3.
  P1a 0 errors / 0 warnings → ½ (no reason)
  P1b FLASH 474 B (+106) → ❌ 464 B (+96)
  P2 "a" once (0x61), then silence; only call is in main, loop has no putc → ✅
Result: build 0/0; FLASH 368 → 464 B, RAMB 8 B; size text 460 data 4 bss 4.
  objdump: uart4_putc 0x40–0x73 (push {r7}; sub sp,#12; mov r3,r0; strb r3,[r7,#7]; poll 0x4c–0x56: ldr STAT addr,
    ldr [r3], and.w #0x800000, cmp #0, beq; ldr DATA addr; ldrb; str; adds r7,#12; mov sp,r7; ldr.w r7,[sp],#4; bx lr;
    3 nop; literals 0x500B4014, 0x500B401C). main 0x74; first C line 0x7a; PSELID 0xee (ldr+movs+str); BAUD 0xf4
    (ldr addr + ldr value literal 0x0F00001A + str); CTRL 0xfa (ldr + mov.w #0x80000 + str); movs r0,#97 + bl 0x40 @0x102;
    PTOR loop top 0x108; b.n 0x108 @0x124; pad nop 0x126; literals 0x128–0x170.
  Size: +96 = init 36 (6+4, 6+8, 8+4) + call 6 + uart4_putc 52 (44 code incl. nops + 8 lit) + 2 pad.
  Run: print/x $pc = 0x7a (stop at main). delete; continue → terminal "a", LED 16 ON flashes / 10 s.
    Terminate, RESET button, no debugger → terminal "a", LED 16 / 10 s.
PASS (terminal + objdump + build output). First UART output from our own firmware; survives reset without GDB.
Learned:
  - Size rules extended: ldr-literal = 2 B (narrow); constant not encodable as a modified immediate → 2nd literal
    (ldr 2 B + 4 B), not movw/movt at -O0; 0x0008_0000 and 0x0080_0000 ARE modified immediates (8-bit value shifted);
    leaf function: push {r7} / bx lr (no lr save); char arg spilled with strb; -O0 inserts nops.
  - Never guess encodings: use the logged rules or objdump (cf. 1d.4 cycle guesses).
  - C precedence: == and != bind tighter than & → always write ((x & MASK) == 0).
  - A rule comment = RM statement + source; no unverified consequences.
  - Prefix a register macro with the block that owns it (PSELID → LP_FLEXCOMM4, not LPUART4).
  - New addresses: uart4_putc 0x40, main 0x74, stop-at-main 0x7a, loop top 0x108 (old 0x46/0xba invalid).
  - No GDB-only state left: PSELID/BAUD/CTRL now in C.
Process: "continue" typed twice (3rd time). print/x $pc at main pasted ✅.
Closed: firmware alone brings up the UART after reset (no debugger).

### 1e.4e-2 uart4_puts("Hello\r\n") — session 8, 2026-10-26
Composition (user, tutor-checked, 2 rounds): static void uart4_puts(const char *s){ while (*s != '\0'){ uart4_putc(*s); s++; } }
  defined after uart4_putc (C99+: no implicit declaration; GCC 14 = error); main: uart4_puts("Hello\r\n") replaces
  uart4_putc('a') (commented-out line deleted). CR 0x0D = column 0, LF 0x0A = next line; LF alone → staircase,
  CR alone → overwrite (tutor note, inference: some terminals offer implicit CR on LF). "Hello\r\n" = 8 B stored
  (7 + '\0'), 7 B / 70 bit times on the wire.
Predictions LOCKED (10:20). Score 2/3.
  P1a 0 errors / 0 warnings (reasons per warning class) → ✅
  P1b FLASH 516 B → ❌ 520 B
  P2 "Hello", cursor column 0 of next line, once → ✅
Result: build 0/0; FLASH 464 → 520 B, RAMB 8 B; size text 460 → 516, data 4, bss 4 (FLASH = text + data).
  objdump: uart4_puts 0x1d0–0x1fb (44 B: push {r7,lr}; sub sp,#8; add r7,sp,#0; str r0,[r7,#4]; b.n cond;
    body ldr/ldrb/mov r0/bl 0x40/ldr/adds/str; cond ldr/ldrb/cmp #0/bne; nop; nop; adds r7,#8; mov sp,r7; pop {r7,pc}).
  main: 0x102 ldr r0,[pc] (literal 0x16c = 0x000001FC) + 0x104 bl 0x1d0 (was movs r0,#97 + bl 0x40 → +0 code, +4 literal).
  String at 0x1fc: 48 65 6c 6c 6f 0d 0a 00 (.rodata after .text, per .ld). Stop-at-main 0x7a, loop top 0x108 unchanged.
  Reset_Handler literals (-s dump) 0x1b4–0x1c8: 0xE000ED88 (CPACR), 0x204 (data_load), 0x20008000, 0x20008004,
    0x20008004, 0x20008008. Default_Handler 0x1cc (b .) + nop pad 0x1ce.
  Size: +56 = uart4_puts 44 + literal 4 + string 8 + 0 pad. (Predicted 38 + 6 + 8.)
  Run: print/x $pc = 0x7a; delete; continue (typed twice, 4th time) → terminal "aHello", LED 16/10 s.
    Suspend → "Suspension Timeout"; Terminate no effect → board unplugged/replugged (cold boot, no debugger)
    → terminal "Hello", cursor under 'H' on the next line, LED 16/10 s.
PASS (terminal + objdump + build).
Learned:
  - -O0 epilogue template: [nop…] adds r7,#frame; mov sp,r7; pop/ldr.w … — reuse patterns already seen in objdump.
  - Call-site change ldr-literal vs movs: both 2 B → only the literal is new.
  - .rodata strings land after .text (our .ld order); code reaches them through a literal-pool address.
  - FLASH Used = text + data (the .data initial values are stored in flash).
Open (inference): 'a' before "Hello" in the debug run = old 1e.4e-1 image ran during probe connect/reset before
  reflash; step-5 terminal clear unconfirmed.
Process: Suspension Timeout after double "continue" (cause unknown, inference). Recovery: GDB "interrupt" →
  Terminate → unplug as last resort; then Debug view → Remove All Terminated before next launch.
1e (polled UART TX) COMPLETE.

### Phase 1 self-test (session 8, 2026-10-26) — paper, open-book, no hardware. Score 3.75/5
Q1 (stop-at-main 0x7a): (a) CTRL 0x00000000 ½ — value ✅; reason covered Reset_Handler/main but not the boot ROM
     (needed: ROM left FC4 without a mode, PERSEL 000b 1e.4a; CTRL read 0 at loop top 1e.4c-2).
   (b) FCCLKSEL4 0x00000007 ❌ — log 1e.2b: 0x00000003 at main (changed before main; RM reset 111b). 8th ROM-handed miss.
Q2 (first hit 0x108): (a) AHBCLKCTRL1 0x00008000 ❌ — 0x20008000 (1e.3a: 0x20000000 at main, reserved bit 29;
     | SET1 bit 15, 1e.3e). Method old | bits ✅, old = RM reset instead of value at main. 9th ROM-handed miss.
   (b) AHBCLKCTRLSET1 0x00000000 ✅ (SET/CLR registers read 0, 1d.2f/1e.3b).
Q3 (a) Secure; bit 28 = alias, state from reset/attribution (SAU off → all Secure) ✅
   (b) vector LSB → EPSR.T; BL never changes state (only BX/BLX use the LSB); even entry → UFSR.INVSTATE,
       escalated to HardFault FORCED with UsageFault disabled (1c) ✅
Q4 LMA 0x0000_0204, VMA 0x2000_8000, g_zero 0x2000_8004 / 0 flash bytes ✅ — confirmed by the -s dump:
   Reset_Handler literals 0x1b8–0x1c8 = 0x204, 0x20008000, 0x20008004, 0x20008004, 0x20008008; FLASH 520 = text 516 + data 4.
Q5 (a) UFSR.INVSTATE, CFSR bit 17 ✅  (b) no FP instruction → FPCA 0 → EXC_RETURN 0xFFFFFFF9, 8-word frame, bit 4 FType 0→1 ✅
   (rerun not done; matches 1b.4a, inference).
Learned: before predicting any value at a breakpoint, check the log's "at main" value — the boot ROM runs before
   Reset_Handler. A reset-value reason must cover ROM + our code. Weak point stays TOP for Phase 2.

HW check of Q1/Q2 (2026-10-26): pc 0x7a: CTRL 0x500B4018 = 0x00000000; FCCLKSEL4 0x500002C0 = 0x00000003.
  bp 0x108 (pc 0x108): AHBCLKCTRL1 0x50000204 = 0x20008000; AHBCLKCTRLSET1 0x50000224 = 0x00000000.
  All four = log values (1e.2b, 1e.3e, 1e.4c-2, 1d.2f). Self-test 3.75/5 confirmed on hardware.
Open: GDB console shows "continue" twice (first with a leading space) with one "Continuing." — echo by the IDE
  console or typed twice? (user to confirm; if echo → tutor correction to the 1e.4c–1e.4e process notes)
PHASE 1 COMPLETE (2026-10-26).

### Phase 2 started — 2.0 decision issued (session 8, 2026-10-26)
Options for SCB/NVIC/SysTick/DWT register names: A own macros; B CMSIS core_cm33.h names only (needs config defines,
  e.g. __NVIC_PRIO_BITS 3; still no SDK drivers/startup); C hybrid — own macros 2.0–2.3, switch to CMSIS from 2.4
  (tutor recommendation: addresses teach most in 2.1–2.3; Phase 6 FreeRTOS port uses CMSIS names). User decision
  pending. Proposed Phase 2 steps: 2.0 decision · 2.1 GDB read-only audit at main (VTOR, AIRCR.PRIGROUP, SHPR1–3,
  ICSR, SysTick CTRL; ROM-handed re-test) · 2.2 IPR 0xFF → 0xE0 + Phase 0 Q5 re-test (0xA0) · 2.3 SysTick polled
  (COUNTFLAG) vs CYCCNT · 2.4 SysTick interrupt handler blinks LED · 2.5 NVIC IRQ pended by software (ISPR),
  priorities/preemption · 2.6 tail-chaining in cycles.

### Session 8 tutor corrections and summary
Session 8 tutor corrections: none of substance so far. PENDING: in every paste "continue" appears twice (first with a
  leading space) followed by ONE "Continuing.", while all other commands appear once → possibly an IDE console echo.
  If the user confirms he typed it once, log a tutor correction: the "continue typed twice" process notes in 1e.4c-1,
  1e.4c-2, 1e.4e-1, 1e.4e-2 were wrong; drop that weak point. Session 8 scores: 1e.4e-1 1.5/3 · 1e.4e-2 2/3 · Phase 1
  self-test 3.75/5. Weak points this session: ROM-handed value (8th, 9th: FCCLKSEL4, AHBCLKCTRL1 bit 29); guessed
  instruction encodings (ldr-literal as 4 B, movw/movt, mask literal, non-leaf prologue, -O0 epilogue); C precedence
  (& vs ==); reason missing on a build-result row (1e.4e-1 P1a); comment claims beyond the RM. Strengths: Q3–Q5 of the
  self-test full marks; 1e.4e-2 uart4_puts body counted instruction-exact; precedence bug found and explained on
  paper; P2s fully right twice.

## Session 9 (2026-10-26)

### Session-9 intake notes
- learning_log.md: session 8 appears TWICE — lines 826–932 (formatted) and 934–960 (flattened duplicate).
  Second copy only adds: 1e.4e-2 Reset_Handler literals 0x1b4–0x1c8 (CPACR, 0x204, 0x20008000, 0x20008004,
  0x20008004, 0x20008008) + Default_Handler 0x1cc / pad 0x1ce; blocks "Phase 2 started" and "Session 8 tutor
  corrections". Fix: merge those into the formatted copy, delete 934–960. Nothing missing.
  DONE (tutor, 13:30): merged, flattened copy deleted, stale "HW check of Q1/Q2: pending." line removed.
- Open question closed: "continue" shown twice = typed twice (user confirmed). No tutor correction; process
  notes 1e.4c-1…1e.4e-2 stand. Weak point stays: type continue ONCE. Inference (untested): a 2nd continue while
  running may be linked to the 1e.4e-2 Suspension Timeout.

### 2.0 register-name decision — A vs C explained (session 9)
A = own macros through Phase 2 (addresses/bits composed from DDI0553; NVIC index math + byte-wide IPR by hand).
C = own macros 2.0–2.3, CMSIS core_cm33.h names from 2.4 (needs IRQn_Type + config defines or device header;
  sub-decisions: names only vs header functions; peripherals stay own macros).
Tutor correction (session 8 rationale): "Phase 6 FreeRTOS port uses CMSIS names" — wrong as stated; port.c uses
  its own raw-address macros (e.g. portNVIC_SYSTICK_CTRL_REG 0xE000E010, tutor knowledge, verify in Phase 6);
  CMSIS ties are only __NVIC_PRIO_BITS → configPRIO_BITS and handler-name mapping. Recommendation C kept, reason
  restated: CMSIS names = the common reading vocabulary after 2.3.
User decision: pending.

### 2.0 decision — tutor recommendation revised (session 9)
Tutor recommendation changed C → A after user restated the objective (learn what happens behind the scenes).
Reason: C hides addresses, NVIC index math (n/32, n%32) and IPR byte access from 2.4 on.
Plan under A: own macros named after DDI0553 register names with block prefix (SCB_VTOR, SCB_AIRCR, SCB_SHPR3,
  SCB_ICSR, SYST_CSR/RVR/CVR, NVIC_ISER0, NVIC_IPR0); REG8 macro for byte-wide IPR (2.2);
  end of Phase 2: read-only mapping exercise core_cm33.h names ↔ own macros (header not included in the build).
DDI0553 B.z = Armv8-M Architecture Reference Manual (architecture: exception model, SCB/NVIC/SysTick/SAU/MPU/DWT
  registers). TRM 100230 = Cortex-M33 implementation. NXP RM = chip outside the PPB. To be attached for 2.1.
User decision: pending confirmation.
User decision: A (2026-10-26 12:56). 2.0 CLOSED. Bench Map v32.

### 2.1 Core-register audit at main (GDB read-only, stop-at-main pc 0x7a) — session 9, 2026-10-26
Registers (DDI0553 B.z D1.2): VTOR 0xE000ED08 (D1.2.272 p.1902), AIRCR 0xE000ED0C (D1.2.3 p.1458), SHPR1 0xE000ED18
  (D1.2.234 p.1844), SHPR2 0xE000ED1C (D1.2.235 p.1846), SHPR3 0xE000ED20 (D1.2.236 p.1847), ICSR 0xE000ED04
  (D1.2.126 p.1675), SYST_CSR 0xE000E010 (D1.2.241 p.1854). PDF page = printed page in DDI0553.
Paper prep: field-reset table from DDI0553 checked — misreads: PRIS, PRIGROUP, DIT read as IMPLEMENTATION DEFINED
  (they reset to zero; only writability / WI behaviour is IMP DEF); VECTKEYSTAT "reads as 0xFA05" (not a reset value);
  SYSRESETREQ is RW (not WO). Lesson: read the field to its "resets to …" line.
Predictions LOCKED (18:57). Score 6/7.
  R1 VTOR = 0x00000000 ✅ (log 1a.5c + no writes; deeper: TRM VTOR row — VTOR_S[31:7] from INITSVTOR)
  R2 AIRCR = 0xFA050000 ✅ (PRIGROUP 0 → group [7:1], sub [0]; 3 prio bits → all preemption)
  R3 SHPR1 = 0 ✅   R4 SHPR2 = 0 ✅   R5 SHPR3 = 0 ✅
  R6 ICSR = 0x00000000 ✅ (VECTACTIVE 0 = Thread; a debug halt is not an exception)
  R7 SYST_CSR = 0x00000004 ❌ → 0x00000000: CLKSOURCE 0 = external reference clock. Chip has one: SYSTICKCLKSEL0
     0x5000_0260 (RM 14.4.1.18 p.277), Fig 72 RM 24.3.7 p.765; CPU0STCKCAL NOREF reset 0 = reference clock provided
     (RM 14.4.1.3 p.247). Quoted "External clock is not available in this device" NOT in RM Rev.4 (full-text search).
Run: print/x $pc = 0x7a before and after reads; no continue; terminated.
Weak point re-test (ROM-handed value): PASSED — log "at main" values (1a.5c) used; reasons covered ROM/RH/main/debugger.

### 2.2a NVIC_IPR9 PRI_N3 ← 0xFF (GDB, stop-at-main pc 0x7a) — session 9, 2026-10-26
Composition (user, tutor-checked, 2 rounds): LP_FLEXCOMM4 vector 55, IRQ 39 (RM 3.3.3 Table 15 p.33; IPR reg 9 ✓);
  39 = 4×9 + 3 → n 9, m 3; IPR9 0xE000E424 (DDI0553 D1.2.185 p.1769: 0xE000E400 + 4n); PRI_N3 byte 0xE000E427,
  bits 31:24. Fixes: duplicated "x/1wx"; "set {unsigned int}0xE000E427" = 4-byte unaligned store spilling into IPR10
  → {unsigned char} (type in braces = access size).
Predictions LOCKED (23:25). Score 2.5/3.
  P1 IPR9 before = 0x00000000 ½ — value ✅; reason said main's LED/UART writes run before 0x7a (wrong: 0x74–0x79 =
     prologue, 0x7a = first C line; those writes come after the halt).
  P2 byte after 0xFF = 0xE0 ✅ (3 implemented bits [7:5], [4:0] RAZ/WI)
  P3 IPR9 after = 0xE0000000 ✅
Result: pc 0x7a before/after; 0x00000000 → byte 0xe0 → word 0xe0000000. Terminated (GDB-only write gone).
Learned: __NVIC_PRIO_BITS = 3 measured on hardware (RM 3.3.1 confirmed). GDB {type} sets the access width.
  At stop-at-main 0x7a nothing in main has executed except the prologue.

### 2.2b Priority level 5 → IPR9 PRI_N3 (GDB, stop-at-main pc 0x7a) — session 9, 2026-10-27 — Phase 0 Q5 re-test
Composition (user, tutor-checked, 1 round): level 5 = 0b101 → bits [7:5], [4:0] = 0 (RAZ/WI, 2.2a) → 0b1010_0000 =
  0xA0; back-decode (0xA0 >> 5) & 7 = 5. Commands: set {unsigned char}0xE000E427 = 0xA0; x/1bx; x/1wx 0xE000E424.
Predictions LOCKED (11:44). Score 2/2.
  P1 byte = 0xA0 ✅   P2 IPR9 = 0xA0000000 ✅
Result: pc 0x7a before/after; IPR9 before 0x00000000 (extra read); byte 0xa0; word 0xa0000000. Terminated.
Note: reasons hedged ("not normally", "not expected") — cite measured evidence (2.2a: IPR9 0 at 0x7a, same image).
PHASE 0 Q5 RE-TEST PASSED: priority level L is written as L << (8 − __NVIC_PRIO_BITS) = L << 5; level 5 = 0xA0.

### 2.3a SysTick clock path + counter registers at main (GDB read-only, pc 0x7a) — session 9, 2026-10-27
Predictions LOCKED (12:14). Score 1/4 (S4/S5 "no prediction" → ungraded).
  S1 SYSTICKCLKSEL0 0x50000260 = 0x00000007 ❌ → 0x00000000 (SEL 000 = SYSTICKCLKDIV0 output; RM 14.4.1.18 p.277,
     reset 111b none) — changed before main. Cited "RM Table 64" = pinmux table (wrong source).
  S2 SYSTICKCLKDIV0 0x50000300 = 0x40000000 ❌ → 0x00000000 (HALT 0, DIV 0 → running ÷1; RM reset HALT 1)
     — changed before main.
  S3 CPU0STCKCAL 0x50000038 = 0x00000000 ½ — value ✅; cited "RM Table 41" = flash IFR command table; right source
     RM 14.4.1.3 p.247.
  S4 SYST_RVR 0xE000E014 = 0x00FFFFFF (ungraded; DDI0553 D1.2.243 UNKNOWN reset)
  S5 SYST_CVR 0xE000E018 = 0x00000000 (ungraded; D1.2.242 UNKNOWN reset)
  S6 SYST_CALIB 0xE000E01C = 0x00000000 ½ — value + mapping (S3[25]→NOREF[31], [24]→SKEW[30], [23:0]→TENMS) ✅;
     cited "RM Table 572" not found in RM Rev.4. NOREF 0 = reference clock provided; TENMS 0 = calibration not
     known (D1.2.240 p.1853).
Weak points: ROM-handed value — 10th/11th miss (SYSCON clock select/divider changed before main, same pattern as
  FCCLKSEL4/FROHFDIV 1e.2b). NEW TOP: cited sources must exist in the attached docs (2.1 CLKSOURCE quote; 2.3a
  Tables 41/64/572) → unopened source = ½ max.
Findings: SysTick reference clock live at main = main_clk ÷1 via SYSTICKCLKSEL0 000 (Fig 72, RM 24.3.7 p.765).
  RM Table 205 p.760 "Maximum clock speed of modules in run modes": System tick clock OD 25 / SD 16 / MD 8 MHz →
  48 MHz ÷1 at main exceeds every mode (voltage mode unknown).
Open (inference): ROM used SysTick (RVR max, CVR 0, disabled)? ROM vs debugger connect: test by UART dump after cold
  power-on without debugger.

### 2.3b SysTick polled (COUNTFLAG), processor clock, 1 ms — session 9, 2026-10-27
Composition (user, tutor-checked, 5 rounds): clock P (CLKSOURCE 1; 48 MHz log 1e.2a; avoids R-path unknowns:
  voltage mode, Table 205 limit). Macros SYST_CSR 0xE000E010u / RVR 0xE000E014u / CVR 0xE000E018u /
  SYST_CSR_COUNTFLAG (1u<<16) with DDI0553 refs. RELOAD = f/1000 − 1 = 47,999 (0xBB7F) from DDI0553 B11.1 IVHDT
  (p.290: period = RELOAD+1). CSR = (1u<<2)|(1u<<0) = 0x5. Order per IPPGV: RVR, CVR, CSR; then two waits
  ((CSR & COUNTFLAG) == 0u). Placed in main after LPUART CTRL, before uart4_puts.
  Fixes: first draft f = 16 MHz (a limit, not our clock); block first placed inside uart4_putc (would run per
  character) → moved; COUNTFLAG "read-only" unsourced (p.1855: cleared by a software read as 1 or any SYST_CVR
  write); read vs write meaning mixed in back-decode.
  Side facts (B11.1): two SysTicks with Main+Security ext (RXPCW); SysTick stops in Debug state (RQSKV).
Predictions LOCKED (18:44). Score 1/4.
  P1 0 err / 0 warn ½ (no reason)
  P2 FLASH 596 B ❌ → 580 B (+60 = RVR 8 [ldr + movw + str] + CVR 6 + CSR 6 + 2 waits × 14 [nop, ldr, ldr,
     and.w #0x10000, cmp, beq] + 3 literals 12). Mask is a modified immediate (no literal); no pad needed.
  P3 CYCCNT Δ 47,998 ❌ → 48,005 (0x0000BD4D @0x124 → 0x000178D2 @0x132). Δ = 48,000 period + difference of
     flag-detection phase within the poll loop (both exits are the same not-taken beq). Exact value not derivable
     without the measured poll-iteration cost (unmeasured; inference ~6–10 cycles).
  P4 "Hello" ✅ + LED 15/10 s ❌ (log 1d.4: 15 cycles/iteration → 16/10 s; "~16 cycles" was guessed) → ½
Result: build 0/0; FLASH 580 B, RAMB 8 B; text 576 data 4 bss 4. objdump main: SysTick writes 0x102–0x115,
  wait 1 0x116–0x123, wait 2 0x124–0x131, ldr r0 string + bl uart4_puts 0x132/0x134, LED loop top (PTOR) 0x138,
  b.n 0x138 @0x154, pad 0x156, literals 0x158–0x1b0. uart4_puts 0x20c, string 0x238. Stop-at-main 0x7a.
  Run: pc 0x7a; bp 0x124 / 0x132 (HW); terminal "Hello", LED 16/10 s; RESET button (warm, no debugger) → same.
PASS. SysTick period = 48,000 core cycles = 1 ms at 48 MHz (confirms 1e.2a independently).
Tutor correction (size rule, session 8): a constant ≤ 0xFFFF that is not a modified immediate → movw (4 B) at -O0;
  only wider constants (e.g. 0x0F00_001A) come from a literal.
Process: step 8 asked for unplug (cold); RESET used (warm) — accepted. "continue" still shown twice in every paste
  after the instruction to type it once → question re-opened (if typed once → IDE echo → tutor correction to the
  1e.4c–1e.4e and session-9 "typed twice" notes, drop the weak point).

### Session 9 summary (2026-10-26 / 2026-10-27)
Scores: 2.1 6/7 · 2.2a 2.5/3 · 2.2b 2/2 (Phase 0 Q5 passed) · 2.3a 1/4 · 2.3b 1/4.
Tutor corrections: (1) session-8 2.0 rationale "FreeRTOS port uses CMSIS names" wrong (port.c uses raw-address
  macros; verify Phase 6); recommendation changed C → A. (2) Size rule: 16-bit constants → movw at -O0.
  (3) Tutor said "PDF pages" for DDI0553 — PDF page = printed page there, so no harm.
Weak points this session: fabricated/unopened citations (NEW TOP: 2.1 quote, 2.3a Tables 41/64/572); ROM-handed
  value (10th, 11th: SYSTICKCLKSEL0, SYSTICKCLKDIV0 — but PASSED in 2.1 where log values existed); "IMPLEMENTATION
  DEFINED" read as the reset value when it only covers writability; hedged reasons instead of measured evidence;
  guessed numbers (LED 16 cycles/iteration vs logged 15; 596 B size); polling exit modelled as a fixed branch cost;
  reason missing on build row (P1 again); read vs write meaning; what has executed at stop-at-main (prologue only).
Strengths: 2.2 composition (IRQ → n, m, byte address) right first time; Q5 re-test passed; RELOAD derivation from
  IVHDT; predicted movw for 47999; placement bug fixed when shown; saved file sent before build.

## Session 10 (2026-10-27 / 2026-10-28)

### Session-10 intake notes
- learning_log.md: session 9 present once (955–1091); matches the session-10 prompt. session9_log_blocks.md
  not attached → no cross-check needed.
- Prompt correction: study guide Part 2 = FreeRTOS. Exceptions are G1.6–G1.11 ("Part 1 continued:
  Exceptions": G1.6 exceptions/NVIC, G1.7 priorities/masking, G1.8 entry/exit).
- Guide discrepancy: G1.7 "Reset (−3)" vs DDI0553 B3.9 RCMTC p.80: Reset −4 (−3 = Secure HardFault when
  AIRCR.BFHFNMINS = 1). Guide fix: pending user decision.
- Open question CLOSED: "continue" shown twice in GDB pastes. User typed it ONCE in 2.3b; the paste still showed
  two → IDE echo, not a double keystroke.
  Tutor correction: session-9 intake note "typed twice (user confirmed)" and the 1e.4c-1…1e.4e-2 process notes
  that counted a double continue are withdrawn; weak point "type continue ONCE" dropped. The inference that a 2nd
  continue caused the 1e.4e-2 Suspension Timeout loses its basis → Suspension Timeout cause: unknown.
- Bench Map v38.

### 2.4 SysTick interrupt — composition issued (session 10, 2026-10-27)
Sources checked this session: B3.9 RCMTC p.80 (SysTick = exception 15); B3.30 RWPRT p.141 (word offset =
  exception number); D1.2.241 p.1855 (TICKINT bit 1: count to 0 → SysTick pending; resets to 0 on Warm reset);
  B11.1 IVDJQ p.291; D1.2.236 p.1847 (SHPR3; word/halfword/byte accessible if Main Extension — TRM §1.2: M33
  implements Armv8-M with the Main Extension); D1.2.126 p.1675 (ICSR; VECTACTIVE [8:0] text on p.1679, not
  banked, = IPSR exception number); D1.2.142 p.1707 (IPSR [8:0]); B3.5 RZFHH p.74 (IPSR inside XPSR); B3.19
  p.106–113 (frame; figure p.107: State context 8 words R0 R1 R2 R3 R12 LR(R14) ReturnAddress RETPSR,
  Additional state context 10 words, FP context 18, Additional FP context 16; higher addresses at top);
  B3.22 p.118 / D1.2.96 p.1628–1629 (EXC_RETURN: PREFIX 31:24 = 0xFF, 23:7 RES1, S 6, DCRS 5, FType 4
  [0 = extended, 1 = standard], Mode 3 [1 = Thread], SPSEL 2 [0 = MSP], bit 1 RES0, ES 0); D1.2.9 CCR p.1471–1472
  (bit 9 RES1 — "STKALIGN" appears in neither TRM nor DDI0553); D1.2.100 FPCCR p.1633 (0xE000EF34; ASPEN 31,
  LSPEN 30, TS 26, LSPACT 0); Glossary p.2137 (PE = Processing Element).
B3.19 rule analysis (user, tutor-checked): keep RSHNX (baseline) + RBLQS (Secure→Secure Additional state =
  IMP DEF, governs our case; TRM has no DCRS text) · RSGKY (covers FPCA 1 and 0; decides FP section) · RZWQX
  (ASPEN 1 → FPCA set by FP instr; LSPEN 1 → lazy) · RPBVF (realign; CCR bit 9 RES1, log CCR 0x201) · RBKVD
  (RETPSR content) · RQDKQ context only. Skip ITQYR (late arrival), RNXDF (FPCCR.TS bit 26 = 0 in 0xC000_0004).
  Fixes during review: first table skipped FP rules on "code uses no FP" (wrong: .c lines 84–85, log 1b.4b);
  "lines 69–72 of the PDF" is not a citation (rule ID + page); lazy preservation RESERVES space (RSGKY bullet 2);
  SPSEL reason = reset value → use measured MSP (1a.5a).
Tutor correction (session 10, 2.4 composition): tutor said RBLQS/DCRS cannot be settled by any attached
  document for our core → likely "no prediction". Wrong: log 1b.4a (EXC_RETURN decoded, 8-word frame, FPCA 0)
  and 1c (EXC_RETURN + 26-word frame, FPCA 1) are measured Secure→Secure entries on this core. Predictions
  must use them (arguing which differences to 2.4 matter).
C6 review: section word counts miscounted (Additional state, FP — use p.107 figure labels; +1 RPBVF word);
  frame top formula missed main's push → measure $sp at stop-at-main instead; ReturnAddress landmark
  depends on where SysTick can fire (C5: Hello before/after enable). FPCCR 0xE000EF34, LSPACT bit 0 verified.
Composition review notes: drafts contained generic-system values contradicting our log (150 MHz PLL0, RVR
  149,999, GPIOx->PTOR, "often 0x00", "classic armex", a non-attached "Exception Model User Guide" citation).

### 2.4 SysTick interrupt: own C handler in vector slot 15 — session 10, 2026-10-28
Composition (user, tutor-checked, many rounds): C handler (strong SysTick_Handler) overrides .weak +
  .thumb_set SysTick_Handler, Default_Handler in startup_m33.S; slot 15 .word SysTick_Handler. Reason: B3.19
  p.107 State context = AAPCS caller-saved set; G1.8. Thumb bit of the C symbol from -mthumb.
  SYST_CSR = (1u<<2)|(1u<<1)|(1u<<0) = 0x7. PRI_15 byte 0xE000ED23 (D1.2.236 p.1847, byte-accessible with Main
  Extension, TRM §1.2) ← 0x80 = level 4 via REG8 (no functional effect in 2.4). Handler: local static tick (no
  volatile: only the handler touches it), toggle GPIO4_PTOR bit 18 every 500 ticks. main: RVR → CVR → Hello →
  PRI_15 → CSR → empty for(;;); COUNTFLAG waits + busy-wait deleted; FP lines kept (FPCA).
  Saved-file review: 2 lvalue errors (GPIO4_PTOR = …, SYST_CSR = … without REG32), GPIO4->PTOR/LED_PIN from a
  generic draft, leftover waits, PRI_15 after enable, commented-out code → fixed before build.
Predictions LOCKED (17:42). Score 7/9 (P9 ungraded: no single value).
  P1 0/0 ✅
  P2 FLASH 588 ❌ → 584 B. main+pool 320 → 268 B (−52); SysTick_Handler 0x74–0xab 56 B (leaf shape, cmp.w #500
     modified immediate, 2 literals). Reasons used guessed ranges, not the size rules.
  P3 RAMB 12 ✅ (bss 8: g_zero + tick @0x2000_8008)
  P4 10 ON/10 s + "Hello" ✅
  P5 slot 15 = handler|1 ✅ (0x0000_0075, handler 0x74)
  P6 IPSR 0xF ✅ ($xpsr 0x0900_000F)   P7 VECTACTIVE 0xF ✅ (ICSR 0x0000_080F; RETTOBASE 1)
  P8 EXC_RETURN 0xFFFFFFF1 ❌ → 0xFFFF_FFE9 (S1 DCRS1 FType0 Mode1 SPSEL0 ES1) = log 1c. F1 contradicts the
     user's own field rows (FType 0, Mode 1) → back-decode not done; SPSEL reason = reset value (½ pattern).
  P9 ungraded → 26 words (0x2000_FFF0 − 0x2000_FF88 = 0x68)
  P10 LSPACT 1 ✅ (FPCCR 0xC000_001D: ASPEN LSPEN HFRDY THREAD S LSPACT; USER 0)
Result: objdump SysTick_Handler 0x74, main 0xac (stop-at-main 0xb2), loop nop/b.n 0x15a/0x15c, uart4_puts
  0x210, string 0x23c. Build 0/0, FLASH 584, RAMB 12, text 580 data 4 bss 8. sp at main 0x2000_FFF0
  (= 0x2001_0000 − push{r7,lr} 8 − sub #8). Frame @0x2000_FF88: R0 0xA, R1 0x2000_800C, R2 7, R3 0xE000_E010,
  R12 4, LR 0x225 (stale, inside uart4_puts), ReturnAddress 0x15A, RETPSR 0x0910_0000; then 18 words FP space
  reserved, NOT written (stale 0x14F, 0x243, 0x2000_FFE0 from old uart4_puts frames). No Additional state
  context (DCRS 1). SPREALIGN 0. LED 10/10 s, terminal "Hello". PASS.
Closed: stacked RETPSR bit 20 = SFPA (D1.2.221 p.1817), measured 1.
New open (inference): APSR.Q = 1 in 1b.4a, 1c and 2.4 RETPSR/xPSR — no saturating instruction in our code;
  set before main (ROM?).
Learned: lazy preservation = 18-word FP slot reserved at entry, filled only if the handler executes an FP
  instruction (FPCCR.LSPACT 1, FPCAR → slot; Phase 5). LR in the frame is whatever LR held (stale here).
Weak points: back-decode composed hex against your own field table (P8); use measured log values over reset
  values (SPSEL); sizes from the logged rules, not ranges (P2); citations from attached docs only (C1 user
  guide); verify the saved file matches the composition (generic draft leaked into the .c twice).
Process: "continue" doubled again in the paste (IDE echo, consistent). Bench Map v40.

### Session 10 summary (2026-10-27 / 2026-10-28)
Scores: 2.4 7/9 (P9 ungraded).
Tutor corrections: (1) session-9 "continue typed twice" notes withdrawn (IDE echo); (2) RBLQS/DCRS said to be
  unknowable — log 1b.4a/1c already measured it.
Prompt/guide fixes: exceptions = G1.6–G1.11 (not Part 2); G1.7 Reset −3 vs DDI0553 −4 (guide fix pending).
Weak points this session: generic-system answers (150 MHz, GPIOx->PTOR, typical-MCU priorities) instead of
  tracing our files/log; back-decode of composed hex (P8); reset value instead of measured (SPSEL); guessed
  size ranges (P2); non-attached citation; placeholder sources ("the log entry showing…"); saved file not
  matching the agreed composition (twice).
Strengths: B3.19 rule filtering done to the end; RSHNX vs RBLQS resolved correctly; FPCCR decode right
  (ASPEN/LSPEN/TS); C5 ordering (Hello before enable) chosen for a clean ReturnAddress landmark; weak-alias
  vector design; predicted LSPACT 1, RAMB, slot-15 Thumb bit.

## Session 11 (2026-10-28 / 2026-10-29)

### Session-11 intake notes
- learning_log.md: session 10 present once (1093–1187), ends with "Session 10 summary". session10_log_blocks.md
  not attached → no cross-check needed.
- Bench Map v40 read; stale items fixed in v41 (Phase 2 chip "2.0", trail marker at 2.0, Phase 5 text still
  asking to verify SFPA — closed in 2.4).
- Guide fix DONE (user decision yes): G1.7 now "Reset (−4), NMI (−2), and HardFault (−1) … Secure HardFault
  becomes −3 (above NMI) only when AIRCR.BFHFNMINS = 1 (DDI0553 B3.9 RCMTC p.80)". Open question closed.
- Next step chosen: (B) 2.5 NVIC external IRQ pended by software. 2.4b lazy-FP detour moved to Phase 5.
- 2.5 split (proposed): 2.5a table to vector 55 + own IRQ 39 handler (static) · 2.5b GDB pend/enable ·
  2.5c priority vs SysTick · 2.5d pend from C.
- Sources verified: NVIC_IABRn D1.2.182 p.1766 (RO, 0xE000E300+4n, word only); NVIC_ICPRn p.1768;
  NVIC_ISERn D1.2.186 p.1770 (W1S, 0xE000E100+4n, word only, resets 0); NVIC_ISPRn D1.2.187 p.1771–1772 (W1S,
  0xE000E200+4n, word only, writes of 0 ignored, resets 0); NVIC_ITNSn D1.2.188 p.1773 (0xE000E380+4n, resets
  0 = targets Secure); B3.13 ICTFJ/RCJDM/RRGCH/RVJPG/RRKCQ p.93 (states; lower number wins; preempt only with
  lower group priority value); B3.18 RKLWH (pended async exception taken if enabled + sufficient priority),
  RVLDB, RWYWW (entry sequence; step 9 pending→0, active→1) p.104; RM 3.3.1 p.31 (8 levels, 3 bits per IPR
  field); RM Table 15 p.31–37 (columns: Address, Vector, IRQ, NVIC non-IPR register number, NVIC IPR register
  number, Alias, Source; LP_FLEXCOMM4 row: 0x00DC, 55, 39, 1, 9); RM Table 158 p.220–221 (header rows
  0x00–0x34; row 0x38 "Vector table — Cortex-M33 core's vector table entries").

### 2.5a Vector table to vector 55 + own IRQ 39 handler — composition issued (session 11, 2026-10-28)
C1 table length/offset · C2 slots 16..54 contents (Table 158) · C3 handler name + weak alias · C4 handler body
(GDB-visible trace, no FP) + LPUART4 CTRL interrupt enables (RM 50.5.5.8) · C5 comment cleanup lines 51/127.
No NVIC register writes in 2.5a. Bench Map v41.

### 2.5a Vector table to vector 55 + own IRQ 39 handler — session 11, 2026-10-29
Composition (user, tutor-checked): C1 56 words / 0xE0, slot 55 @0xDC (RCMTC p.80 16+39; RWPRT p.141; RM Table 15
  p.33). C2 slots 16–54 plain .word Default_Handler (Table 158 p.221 row 0x38: vector entries, no header fields);
  slot 55 .word LP_FLEXCOMM4_IRQHandler; misfire of 16–54 → .S Default_Handler "b ." . C3 name from Table 15
  Alias LP_FLEXCOMM4 + block prefix; .weak + .thumb_set → Default_Handler (pattern of SysTick); misspelling check
  = word @0xDC vs Default|1 (+ gc-sections would drop an unreferenced C body — inference, cf. 1a.3b). C4 body
  g_lpflexcomm4_irq_count++ (volatile, option B: main reader planned 2.5c/d; .bss, no initializer — log 2.4 tick
  shows "= 0" also lands in .bss).
  LPUART4 cannot raise IRQ 39 itself: CTRL ORIE 27, NEIE 26, FEIE 25, PEIE 24, TIE 23, TCIE 22, RIE 21, ILIE 20,
  MA1IE 15, MA2IE 14 all 0 in 0x0008_0000 (RM 50.5.5.8 p.1880–1883); BAUD LBKDIE 15 / RXEDGIE 14 = 0 in
  0x0F00_001A (RM 50.5.5.6 p.1871); proviso FIFO (0x500B_4028) TXOFE 9 / RXUFE 8 (RM 50.5.5.12 p.1895) unmeasured → read in
  2.5b. Scope: RM 50.2.2.4 p.1718 single interrupt from the configured function(s); .c PSELID = 1 (UART only),
  PERSEL 000b before our write (log 1e.4a). ISTAT FF4h (p.1720) identifies the source.
  Theory note: if TIE were 1, TDRE (set while TX idle) would keep IRQ 39 pending once ISER enables it.
  Review fixes: slot 55 first drafted as Default_Handler; "linker pads a table boundary" and "post-build tool
  patches header words" (generic, no source — our build has no post-build tool); .bss loop cited as .S 44–50
  (twice) → 56–63 (old .S) → 99–106 (new .S after the table + 2-line comment); "explicit = 0 → .data"
  contradicted by log 2.4; volatile reason "compiler may elide the store" + "same as 2.4 tick" contradicted the
  2.4 decision (tick not volatile; LED was the evidence); saved .c first dropped volatile; reason 2 (PERSEL
  routes pins / ROM set 001b) contradicted log 1e.4a/1e.3c → rewritten as a scope line.
  Line 54 comment still states option A's reason ("only the IRQ handler writes it, GDB reads it") — fix pending
  (comment-only).
Predictions LOCKED (17:27, P7 addendum 17:30). Score 4.5/6 (P5, P6 ungraded: symbolic, no number).
  P1 0/0 ✅
  P2 FLASH 744 ❌ → 772 (+160 table, +28 handler 0x14C–0x167 = 24 B leaf shape + 1 shared literal 0x2000_8004).
     Reason used a guessed range "~6–8 B" (4th size miss in a row: 2.3b, 2.4, 2.5a).
  P3 RAMB 16 ✅ (bss 12)   P4 .isr_vector 0xE0 ✅
  P5 ungraded → 0x0000_02C9 (Default 0x2C8|1; computable: log 2.4 Default 0x20C + 0xA0 table + 0x1C handler)
  P6 ungraded → 0x0000_014D (handler 0x14C|1 ≠ Default → strong overrides weak)
  P7 0x2000_8004 ✅ (.data 4 B; source order: counter 0x8004, g_zero 0x8008, tick.0 0x800C)
  P8 ½: Hello ✅ (RESET button); LED 1/10 s ❌ → 10/10 s (= log 2.4; toggles counted as flashes)
Result: Build 0/0, FLASH 772, RAMB 16, size text 768 data 4 bss 12. .isr_vector 0x00–0xDF; .text from 0xE0
  (0x220 B); SysTick_Handler 0x114 (56 B, slot 15 = 0x115); LP_FLEXCOMM4_IRQHandler 0x14C (literal @0x164);
  stop-at-main pc 0x16E (main prologue 0x168–0x16D — inference from handler end, confirm with objdump);
  Reset_Handler 0x274 (word1 0x275); Default_Handler 0x2C8 (+ nop pad 0x2CA); .data LMA 0x300.
  Words 0x20/0x24/0x28/0x34 = 0. Cold power-on (USB replug): terminal empty; RESET button → "Hello" (inference:
  VCOM re-enumerates on replug, terminal reconnects after the boot print). LED 10 ON / 10 s. PASS (static).
Weak points: sizes from the logged rules, not ranges; compute addresses from the log instead of symbolic answers
  (P5/P6); check the log for an already-measured value (LED rate, 2.4); saved file = agreed composition
  (volatile dropped once); line numbers from the file that is built.
Process: use the RESET button (not USB replug) for terminal checks. Bench Map v42.

### Session 11 summary (2026-10-28 / 2026-10-29)
Scores: 2.5a 4.5/6 (P5, P6 ungraded).
Decisions: G1.7 guide fix done (Reset −4); next step = 2.5 (lazy-FP detour to Phase 5).
Tutor corrections: none.
Weak points this session: guessed size ranges (P2, 4th time); symbolic answers where the log allowed a number
  (P5/P6/P7 first draft); already-measured value ignored (LED rate); generic-system claims (linker padding,
  post-build patching); wrong/stale line citations (.S 44–50 twice); saved file drifted from the composition
  (volatile dropped); a "reason" contradicting the log (PERSEL/ROM, 1e.4a).
Strengths: C1 right first time; C2/C3 corrected within one round; CTRL interrupt-enable bit table exact;
  scope line rewritten correctly from RM p.1718 + log 1e.4a; P7 address computed from .data size + source
  order; .S table offsets/labels all consistent (script-checked).

## Session 12 (2026-10-29 / 2026-10-30)

### Session-12 intake notes
- learning_log.md: session 11 present once (1189–1268), ends with "Session 11 summary".
  session11_log_blocks.md identical to that tail (diff clean) → nothing missing or duplicated.
- Bench Map v42 read → v43 (session 12, 2.5b split, 2.5b-1 current).
- Cleanup done (comment only, saved, not yet rebuilt): .c line 54 now reads
  `volatile uint32_t g_lpflexcomm4_irq_count;   /* .bss (zeroed by Reset_Handler .S lines 99–106); written by
  the IRQ 39 handler, read in main from 2.5c/d onward → volatile: main's reads must not be kept in a register. */`
  (one line; line numbers unchanged). First attempt resent the old line unchanged; template method used.
- 2.5b split: 2.5b-1 pend with ISER1 bit 7 = 0 · 2.5b-2 enable → taken at bp 0x14C.
- Sources verified: D1.2.182 NVIC_IABRn p.1766; D1.2.186 NVIC_ISERn p.1770; D1.2.187 NVIC_ISPRn p.1771–1772
  (word only; "write-one-to-set. Writes of zero are ignored"); D1.2.188 NVIC_ITNSn p.1773; ICSR D1.2.126
  ISRPENDING bit 22 p.1678, VECTPENDING 20:12 p.1679 ("highest priority pending and enabled interrupt";
  C_MASKINTS note), RETTOBASE bit 11 p.1679 (UNKNOWN in Thread); B3.18 RKLWH/RKFRF/RVLDB/RWYWW p.104;
  B3.13 ICTFJ/RCJDM/RRGCH/RVJPG/RRKCQ p.93; RM Table 15 p.33 row 0x00DC/55/39/1/9; RM 50.5.5.12 FIFO 28h
  p.1892–1896 (layout + reset row p.1893; TXOF 17 / RXUF 16 W1C p.1893–1894; TXOFE 9 / RXUFE 8 p.1895;
  TXFE 7 p.1895; TXFIFOSIZE 6:4, RXFE 3, RXFIFOSIZE 2:0 p.1896). D1.2.184 = NVIC_ICPRn, D1.2.185 = NVIC_IPRn,
  D1.2.189 = PAC_KEY_P_n (not IPR). FP_CTRL D1.2.109 p.1654–1655. SHCSR D1.2.233 p.1836 (0xE000ED24),
  SYSTICKACT bit 11 p.1840 (for 2.5c).

### 2.5b-1 Pend IRQ 39 with ISER1 bit 7 off — composition (session 12, 2026-10-29 / 2026-10-30)
objdump --disassemble=main (image 2.5a): main 0x168 (prologue push {r7,lr}; sub sp,#8; add r7,sp,#0
  0x168–0x16D confirmed), stop-at-main 0x16E; SYST_CSR = 7 at 0x210–0x214; for(;;) = nop 0x216 + b.n 0x216 at
  0x218; pool pad nop 0x21A (never a bp); literals 0x21C–0x270. uart4_puts 0x2CC (bl at 0x206); string
  "Hello\r\n" 0x2F8 (literal 0x268; 0x2F8 + 8 = 0x300 = .data LMA). Breakpoint: break *0x216 (tutor-confirmed).
C1 IRQ 39 → n = 39 div 32 = 1, m = 39 mod 32 = 7, mask 0x0000_0080; ISER1 0xE000E104 (D1.2.186 p.1770),
  ISPR1 0xE000E204 (D1.2.187 p.1771), IABR1 0xE000E304 (D1.2.182 p.1766), ITNS1 0xE000E384 (D1.2.188
  p.1773); RM Table 15 p.33 non-IPR register number 1 = n ✓.
C2 set *(unsigned int *)0xE000E204 = 0x00000080 (logged form set {unsigned int}… equivalent) — word only
  ("Halfword and byte accesses are UNPREDICTABLE", p.1771); no RMW: "write-one-to-set. Writes of zero are
  ignored" (p.1771). Theory: an RMW on ISPR in running code could re-pend an IRQ taken between the read and
  the write (RWYWW step 9 p.104).
C3 FIFO 0x500B_4028 decode: TXEMPT 23, RXEMPT 22, TXOF 17 / RXUF 16 (W1C), TXFLUSH 15, RXFLUSH 14, bit 13
  reserved, RXIDEN 12:10, TXOFE 9, RXUFE 8 (the only FIFO bits that let LPUART4 pend IRQ 39), TXFE 7,
  TXFIFOSIZE 6:4 (RO, 010 = 8), RXFE 3, RXFIFOSIZE 2:0 (RO, 010 = 8). Back-decode of the RM reset row =
  0x00C0_0022 ✓. ROM evidence for LPUART4: BAUD read 0x0F00_0004 before our write (1e.4c-1), PSELID PERSEL
  000b (1e.4a). TXEMPT timing: last DATA write ('\n', line 65) a few instructions before 0x216; GDB reads
  take ms vs ≈ 87 µs per character; LPUART keeps transmitting while the core is halted (inference). Do not
  read DATA (0x500B_401C) — a read can set RXUF (p.1894; inference for debugger reads).
Review fixes: TXFIFOSIZE first 000, then 001 (bit 5 misplaced) → 010; bits 13:11 "reserved" overlapped
  RXIDEN; reasons cited 1e.4b / line 125 for CTRL (→ 1e.4c-2, line 128); TXEMPT cited old bp 0xBA (→ 0x216);
  "TDRE = char complete" (→ buffer has room, RM 50.5.5.7); TXFE "single write succeeds" not evidence;
  C1 page list shuffled (ISER/ISPR/IABR pages swapped) while section IDs were right; C2 called 0xE000E204 a
  "Secure alias" (PPB: NVIC_ISPRn not banked, p.1771; 0xE002E204 = _NS view, RES0 to the debugger).

### 2.5b-1 Pend IRQ 39 with ISER1 bit 7 off (GDB, image 2.5a, bp 0x216) — session 12, 2026-10-30
Predictions LOCKED 10:11 (W2 VECTPENDING had two values → first, 0x027, graded). Score 5.5/14.
  S1 FIFO 0x00C0_0022 ✅   S2 ISER1 0 ½ (p.1766 = IABR; ROM claim unsupported)
  S3 ISPR1 0 ½ (p.1770 = ISER)   S4 IABR1 0 ½ (p.1771 = ISPR; circular)
  S5 ITNS1 0 ½ (ROM claim unsupported, never measured at main)
  S6 IPR9 0 ½ (D1.2.189 = PAC_KEY_P_n, IPR = D1.2.185 p.1769; PRI_15 write is .c line 138 not 136;
     log 2.2a IPR9 = 0 at main not cited)
  S7 ICSR 0/0/0 ½ (D1.2.184 = NVIC_ICPRn; ICSR = D1.2.126)
  S8 counter 0 ½ (.ld 31–35 = .data rule; .bss = 38–45)
  W1 ISPR1 0x80 ½ (p.1770 = ISER; ISPR = p.1771)
  W2 ICSR 0 / ISRPENDING 1 / VECTPENDING 0x027 ½ → measured 0x0040_0000 (VECTPENDING 0: "pending and
     enabled", D1.2.126 p.1679; 0x27 = 39 ≠ exception 55)
  R1 ISPR1 0 ❌ → 0x80   R2 IABR1 0 ½ (right value, wrong reason)   R3 ICSR 0/0/0 ❌ → 0x0040_0000
  R4 counter 1 ❌ → 0 (handler is .c 87–90, not 71). R1–R4 assumed IRQ 39 taken, contradicting locked S2
     ISER1 = 0.
Result (raw): pc at main 0x16E; bp 0x216 (x/2i: nop / b.n 0x216 → image 2.5a); FIFO 0x00C0_0022, ISER1 0,
  ISPR1 0, IABR1 0, ITNS1 0, IPR9 0, ICSR 0, counter 0. set {unsigned int}0xE000E204 = 0x00000080 → ISPR1
  0x0000_0080, ICSR 0x0040_0000. continue → bp 0x216: ISPR1 0x80, IABR1 0, ICSR 0x0040_0000, counter 0.
  (Typo read x/1wx 0xE000E3 → "Cannot access memory", discarded.) Extra: ISPR1 re-written (harmless, W1S)
  and a further continue (bp 2 "hit 3 times") → still ISPR1 0x80, ISER1 0 (third sample).
PASS: pending + disabled → not taken (RKLWH p.104). ISRPENDING (bit 22, p.1678) = 1 for a pending but
  DISABLED IRQ; VECTPENDING (20:12, p.1679) = 0 → only pending AND enabled. ITNS1 = 0 at for(;;) (IRQ 39
  targets Secure). LPUART4 FIFO TXOFE/RXUFE = 0 → open question closed.
Caveat (inference) at the time: run between halts = nop + b.n only; GDB may step over the bp with interrupts
  masked (C_MASKINTS, p.1679) → closed by 2.5b-2 (same continue, enabled → taken).
Weak points: page numbers shuffled again right after the flagged C1 fix (copy from the corrected
  composition); section IDs cited without opening them (D1.2.184, D1.2.189); line numbers (.ld 31–35,
  .c 136, .c 71); predictions contradicting own locked rows (RKLWH needs enabled AND priority);
  ROM claims without evidence for never-measured registers (ISER1, ITNS1).
Strengths: all snapshot values right (8/8); FIFO reason with measured ROM evidence; W1 predicted the target
  register; C3 back-decode loop converged.
Bench Map v44.

TUTOR CORRECTION (session 12): "few HW comparators (DWT NUMCOMP = 4)" mixed two units — DWT NUMCOMP (log 1a:
  4) = data watchpoint comparators; HW breakpoints = BPU, FP_CTRL 0xE0002000 (TRM Table 13-1, PDF p.82: reset
  0x1000_0040 = 4 / 0x1000_0080 = 8 instruction comparators). TRM config table (PDF p.11): Reduced = 2 DWT +
  4 BP; Full = 4 DWT + 8 BP. Closed by measurement below (8).

### Session 12 pause (2026-10-30 17:17) + FP_CTRL decode
FP_CTRL 0xE0002000 = 0x1000_0081 at bp 0x216 (raw). Decode (DDI0553 D1.2.109 p.1654–1655): REV 0001 = "Flash
  Patch Breakpoint version 2 implemented"; NUM_CODE [14:12] 000 + [7:4] 1000 = 8 instruction comparators;
  NUM_LIT 0 (RAZ/WI, "Remapping is not supported in Armv8-M"); KEY 0 (reads-as-zero; write key); ENABLE 1
  (resets 0 on a Cold reset). Masking ENABLE → 0x1000_0080 = 8 (TRM Table 13-1, PDF p.82) → "Full set"
  (TRM config table PDF p.11), consistent with DWT NUMCOMP 4. ENABLE writer: likely the debugger (GDB
  "automatically using hardware breakpoints"; our .c never writes 0xE0002000) — inference; ROM not excluded
  (FP_CTRL never read at reset).
Review fixes: KEY/ENABLE cited as "RM p.1654" (→ DDI0553 p.1655); TRM values cited from "search results" and an
  "MCX Nx4x" claim (other chip family; → TRM Table 13-1 PDF p.82 directly); "only writer = debugger" omitted
  the boot ROM (again); REV code read as "revision 1" (→ version 2); "FP_CTRL resets to zero" (→ only ENABLE
  resets to 0); "five… six free" (→ 6); planned symbolic bp `break LP_FLEXCOMM4_IRQHandler` (lands after the
  prologue → break *ADDR from the log).
Board unplugged after Terminate → GDB-only state lost; state redone before 2.5b-2.

### 2.5b-2 Enable IRQ 39 while pending (GDB, image 2.5a) — session 12, 2026-10-30
C4 (user, tutor-checked): set {unsigned int}0xE000E104 = 0x00000080 (word only, p.1770; no RMW: SETENA on a
  write 0 = "No effect", p.1770); break *0x14C (log 2.5a handler 0x14C = slot 55 0x14D − 1; first draft gave
  0x216 = main's for(;;) — corrected); order write → bp → continue; comparators in use 2 of 8.
Predictions LOCKED 20:11 (P4 symbolic, no addendum → ungraded). Score 10/13.
  P1 pc 0x14C ✅   P2 xPSR 0x0100_0037 ❌ → 0x0900_0037 (APSR.Q bit 27 missed; logged in every xPSR incl. 2.4
  handler 0x0900_000F)   P3 LR 0xFFFF_FFE9 ✅ (2.4 measured)   P4 ungraded → 0x2000_FF88 (computable from log
  2.4: 0x2000_FFF0 → 0x2000_FF88)   P5 ICSR 0x37/1/0/0 ✅ (0x0000_0837)   P6 ISPR1 0 ½ (no source, hedge;
  RWYWW step 9 p.104)   P7 IABR1 0x80 ✅   P8 ISER1 0x80 ✅   P9 counter 1 ❌ → 0 (bp at first instruction
  halts before the increment; handler = .c 87–90, increment line 89)   Q1 pc 0x216 ✅   Q2 ISPR1 0 ✅
  Q3 IABR1 0 ✅   Q4 ICSR 0/0/0 ✅   Q5 counter 1 ½ (reason built on P9)
Result (raw): main 0x16E; bp 0x216: sp 0x2000_FFF0, ISPR1 ← 0x80 → 0x80, ISER1 0 → ← 0x80; bp 3 0x14C; continue
  → 0x14C: xPSR 0x0900_0037, LR 0xFFFF_FFE9, sp 0x2000_FF88, ICSR 0x0000_0837, ISPR1 0, IABR1 0x80, ISER1
  0x80, counter 0. continue → 0x216: ISPR1 0, IABR1 0, ICSR 0, counter 1.
PASS: pending + enabled + priority 0 vs execution priority 256 (Thread, RVJPG p.93) → taken (RKLWH, RVLDB
  p.104). RWYWW p.104 observed: step 7 IPSR = 55; step 9 ISPR1 bit 7 → 0, IABR1 bit 7 → 1; step 4 LR =
  EXC_RETURN 0xFFFF_FFE9 (same as SysTick 2.4: FPCA 1, Thread, MSP, Secure); frame 0x68 = 26 words (8 State +
  18 FP reserved), same as 2.4. RETTOBASE 1 → no nesting. After return: IABR1 0, ICSR 0, counter 1, no
  re-pend (LPUART4 FIFO/CTRL IE bits 0). Contrast with 2.5b-1 closes the C_MASKINTS caveat. APSR.Q = 1 again
  (open question).
Weak points: use logged measured values when they exist (Q bit, 2.4 sp pair); a bp halts BEFORE the
  instruction at its address executes; give a number, not a rule (P4); cite the rule for hardware actions
  (RWYWW step 9) instead of "Hmm".
Strengths: page citations correct this time; handler address double-checked from two log sources; ICSR
  four-field prediction exact; Q rows consistent with P rows.
Bench Map v45.

### Session 12 summary (2026-10-29 / 2026-10-30)
Scores: 2.5b-1 5.5/14; 2.5b-2 10/13 (P4 ungraded).
Decisions: 2.5b split into 2.5b-1/2.5b-2; next 2.5c (IRQ 39 priority vs SysTick).
Tutor corrections: 1 — DWT NUMCOMP (watchpoints) was used as the HW breakpoint count; FP_CTRL measured 8.
Closed: LPUART4 FIFO TXOFE/RXUFE (0); C_MASKINTS caveat; HW breakpoint count (8).
New facts: ISRPENDING counts pending-but-disabled IRQs, VECTPENDING only pending+enabled; ITNS1 = 0 at for(;;);
  IRQ 39 exception: xPSR 0x0900_0037, EXC_RETURN 0xFFFF_FFE9, 26-word frame, ICSR 0x837; main 0x168,
  for(;;) 0x216/0x218, uart4_puts 0x2CC, string 0x2F8.
Weak points this session: page/section/line citations (shuffled pages after a flagged fix; D1.2.184/189
  cited unopened; .ld 31–35; .c 136/71); predictions contradicting own locked rows (2.5b-1 R rows);
  boot ROM omitted (FP_CTRL ENABLE writer; ISER1/ITNS1 claims); logged measured values not used (Q bit, sp);
  breakpoint timing (halts before the instruction executes); symbolic answers (P4); other-chip /
  "search results" sources (MCX Nx4x); first draft of the handler bp = 0x216.
Strengths: composition loops converged (C3 back-decode, C4); snapshot values 8/8; 2.5b-2 pages right;
  ICSR field predictions exact; good self-check with two independent log sources for 0x14C.

## Session 13 (2026-10-31 / 2026-11-01)

### Session-13 intake notes
- learning_log.md ends with session 12 exactly once (## Session 12 line 1270, summary 1396, EOF 1410).
  session12_log_blocks.md not attached → no diff possible; log is canonical.
- Bench Map read at v45 → republished v46 … v49 during the session.
- Saved .c line 54 = session-12 comment, one line; declaration unchanged; line numbers match the prompt
  (g_init 56, g_zero 57, putc 59–66, puts 68–74, SysTick 76–82, IRQ39 84–90, main 92–144).

### Rebuild check after the line-54 comment change (image 2.5a) — session 13, 2026-10-31
Predictions (locked): B1 FLASH 772 · B2 RAMB 16 · B3 768/4/12 · B4 main 0x168 · B5 stop-at-main 0x16E ·
  B6 for(;;) nop 0x216 — reasons: quotes of log 2.5a / 2.5b-1 values.
Result (raw): Clean build 0/0, FLASH 772 B, RAMB 16 B, size text 768 data 4 bss 12 dec 784.
  objdump main: 0x168 push {r7,lr}; 0x16A sub sp,#8; 0x16C add r7,sp,#0; 0x16E first C line;
  bl uart4_puts 0x206 → 0x2CC; strb PRI_15 0x20E; SYST_CSR = 7 at 0x210–0x214; nop 0x216 / b.n 0x218;
  pad 0x21A; literals 0x21C–0x270. GDB: $pc 0x16e; x/2i 0x216 = nop / b.n 0x216.
Grades: B1–B6 values ✅, reasons ½ each (cited the earlier number without why the edit cannot change it;
  B4 also cited the 2.5a line marked "inference … confirm with objdump" instead of session-12 objdump
  log 1292–1293). Score 3/6.
Closed: comment-only change leaves the image identical (session-12 inference confirmed).
Weak point (new): a logged earlier value is a source, but the reason must say why nothing since then
  changed it (same pattern as "reset value alone is not a reason").

### Theory Q&A during 2.5c-1 composition (session 13)
- "Preempt" = interrupt and pause, not replace: running handler's State context stacked (RWYWW step 1 p.104),
  stays ACTIVE (ICTFJ p.93 "was being handled … preempted"), resumes on return; configured priorities
  (IPR/SHPR) unchanged, only the execution priority (RVJPG p.93) changes. Analogy: bookmark.
- Bookmark = whole State context (R0–R3, R12, LR, ReturnAddress, RETPSR), not just the address; LR ←
  EXC_RETURN (RWYWW step 4); `bx lr` with EXC_RETURN in PC → hardware unstacks (B3.22 p.118).
- Debug halt: no instructions, no exceptions taken, SysTick (RQSKV) and CYCCNT stopped; GDB reads/writes go
  through the debug port; peripherals may continue (LPUART inference, open).
- Exception entry = hardware only, no instructions: RWYWW steps 1–10 p.104 (state context, additional state,
  FP context/lazy, LR = EXC_RETURN, FP clear, IT/FPCA/SFPA cleared, IPSR set, SPSEL 0, pending→0/active→1,
  Security state) + vector fetch (RWPRT p.141) + late-arrival re-prioritisation (RNMTT p.124).

### 2.5c-1 IRQ 39 (level 0) pended inside SysTick_Handler (level 4) → preemption — session 13, 2026-10-31
Composition (user, tutor-checked, several rounds): IPR9 PRI_N3 not written (0 at main 2.2a, 0 at bp 0x216 2.5b-1;
  no IPR literal in main 0x21C–0x270 / .c 59–74; new session) → level 0 vs SysTick level 4 (.c 138 0x80);
  PRIGROUP 0 (AIRCR 0xFA05_0000) → all 3 bits group (IDPSP p.93); RCJDM/RRKCQ/RVJPG p.93, RVLDB p.104;
  execution priority at 0x114 = level 4 (only SysTick active; taken from Thread; log 2.4 ICSR 0x80F).
  bps *0x114 (log 2.5a + slot 15 = 0x115 today) and *0x14C (objdump today + slot 55 = 0x14D), 2 of 8.
  Today's objdump: SysTick_Handler 0x114–0x142 (+ literals 0x144 tick 0x2000_800C, 0x148 PTOR) = 56 B;
  LP_FLEXCOMM4_IRQHandler 0x14C–0x162 (+ literal 0x164 = 0x2000_8004) = 28 B; .isr_vector word0 0x2001_0000,
  word1 0x275, slot 15 0x115, slot 55 0x14D.
  At (a): ISPR1 ← 0x80, ISER1 ← 0x80 (word, W1S; order irrelevant while halted). Discriminator for (d):
  tick 0x2000_800C + CYCCNT 0xE000_1004 (D1.2.59 p.1566). Composition errors fixed on the way: bp at stop-at-main
  (temporary, already consumed), "at main or …" two options, ISER1 write missing, print/x control_s (needs
  info registers), SHCSR D1.2.33 → D1.2.233, N = 26 copied from Thread mode, RVJKL/RRPGL cited for the return
  address (they are fault-on-return / Security-state rules; right: RKPSS/RNCQN p.118), (d) list that could not
  separate resumed vs new entry.
Predictions LOCKED (27 rows). Score 11/27.
Result (raw):
  (a) pc 0x114, xPSR 0x0900_000F, LR 0xFFFF_FFE9, sp 0x2000_FF88, control_s 0, ICSR 0x0000_080F, SHCSR 0x800,
      IABR1 0, ISPR1 0, counter 0, tick 0, CYCCNT 0x0001_1F18.
  (b) ISPR1 0x80, ISER1 0x80, ICSR 0x00C3_780F (ISRPREEMPT 1, ISRPENDING 1, VECTPENDING 55, RETTOBASE 1, VECTACTIVE 15).
  (c) pc 0x14C, xPSR 0x0900_0037, LR 0xFFFF_FFF1, sp 0x2000_FF60, control_s 0, ICSR 0x0000_0037, SHCSR 0x800,
      IABR1 0x80, ISPR1 0, counter 0. Frame @0x2000_FF60: 0xA, 0x2000_8010, 7, 0xE000_E010, 4, 0xFFFF_FFE9,
      0x116, 0x0900_020F.
  (d) pc 0x114, LR 0xFFFF_FFE9, sp 0x2000_FF88, ICSR 0x80F, SHCSR 0x800, IABR1 0, counter 1, tick 1,
      CYCCNT 0x0001_DA98 (Δ from (a) 0xBB80 = 48,000). bp 2 hit 2 times. delete; continue → LED 10 ON / 10 s.
Learned: nesting = SYSTICKACT 1 + IABR1 bit 7 1 + RETTOBASE 0; handler→handler EXC_RETURN 0xFFFF_FFF1
  (FType 1, Mode 0); in a handler CONTROL_S = 0 (FPCA, SFPA cleared at entry, RWYWW step 6) → standard frame;
  RETPSR SFPA 0. Frame R0–R3/R12 = main's values when SysTick was taken (same pattern as 2.4).
  push {r7} at 0x114 executed before IRQ 39 was taken: ReturnAddress 0x116 (RQLHB p.105),
  sp 0xFF88−4 = 0xFF84 → realign −4 (RPBVF p.109, RETPSR.SPREALIGN 1) → −0x20 = 0xFF60. Inference: debugger
  steps over the breakpointed instruction with interrupts masked, although ISRPREEMPT read 1 (D1.2.126 p.1678).
  → the paused SysTick_Handler resumed at 0x116 (past the bp), completed; (d) = next SysTick entry
  (tick 1, CYCCNT Δ exactly one period 48,000).
Grades: A1 ❌ (T bit; log 2.4 0x0900_000F) A2 ✅ A3 ❌ (Thread sp; log 2.4 handler sp 0xFF88) A4 ❌ (Thread CONTROL)
  A5 ✅ A6 ✅ A7 ½ (line 59 → 77) B1 ❌ (ISRPREEMPT) C1 ½ (copied) C2 ❌ (copied Thread EXC_RETURN) C3 ❌
  C4 ❌ (0x4 = FPCA 1, contradicts own reason) C5 ❌ (copied; RETTOBASE 0) C6 ✅ C7 ½ (copied) C8 ✅ C9 ✅
  C10 ½ (2.4 stacked LR was 0x225) C11 ❌ (old-image 0x15A; is in IRQ handler today) C12 ❌ (SFPA 0, SPREALIGN 1)
  D1 ❌ D2 ½ D3 ½ (wrong mechanism) D4 ✅ D5 ✅ D6 ❌ D7 ❌.
Tutor correction: composition check accepted "resumes at 0x114" and N = sp(a) − sp(c) without allowing for the
  debugger executing the breakpointed instruction on continue.
Weak points (new): copying values measured in a different context (2.5b-2 Thread-mode EXC_RETURN/ICSR/RETPSR);
  using the pre-entry sp instead of the handler sp from the same log entry; value contradicting own reason (C4).
Strengths: two-column discriminator (tick + CYCCNT) worked and exposed the surprise; FPCA-at-entry reasoning
  (C4 reason, N = 8 / FType 1).
GDB-only state after: ISER1 bit 7 = 1; no breakpoints; board free-running.

### 2.5c-1b chosen — step-over test (composition round 1, session 13, 2026-10-31)
Plan: new session; same writes; at (b) delete bp 0x114 before continue → IRQ 39 taken before or after push {r7}?
  Deciding row: stacked ReturnAddress (RQLHB p.105); also SPREALIGN (RPBVF p.109) and sp at (c), both depending
  on sp(a) alignment; (d): bp 0x114 set at (c) catches the return landing; tick + CYCCNT separate resumed vs new.
Round-1 check: C1 ✅ new session (clears ISER1 bit 7). Fixes open: C2 stop-at-main 0x16E (not 0x7a), one timing
  for bp 0x14C, `clear 1` → `delete N` (clear takes a location; stop-at-main temp = #1, so 0x114 was #2);
  C3 writes happen at (a); C4 sp row: push {r7} (b480) = 1 word + 1 RPBVF pad word, not R7/LR; C5 (b) add ICSR
  (ISRPREEMPT), (c) x/8xw $sp (x/2xw $sp+0x14 = stacked LR + ReturnAddress; ReturnAddress +0x18, RETPSR +0x1C);
  C6 fate of bp 0x14C at (c) + comparator count, (d) add pc + sp, end with delete (all).

### Session 13 pause (2026-11-01 00:19)
Board terminated and unplugged; GDB-only state: none (ISER1 bit 7 write lost — intended, C1 = new session).

### Session 13 summary (2026-10-31 / 2026-11-01)
Scores: rebuild check 3/6; 2.5c-1 11/27.
Decisions: 2.5c split into 2.5c-1 (nesting), 2.5c-1b (step-over test, chosen next), 2.5c-2 (level 5 → tail-chain).
Tutor corrections: 1 — composition check accepted "resumes at 0x114" / sp-difference frame check without the
  debugger step-over.
Closed: comment-only change leaves the image identical; preemption of a handler by a more urgent IRQ (measured).
New facts: handler→handler EXC_RETURN 0xFFFF_FFF1; RETTOBASE 0 while nested; CONTROL_S 0 in handlers (FPCA, SFPA
  cleared at entry); ICSR.ISRPREEMPT bit 23 = 1 at a halt with an enabled pending IRQ; RETPSR.SPREALIGN measured 1;
  SysTick entry-to-entry = 48,000 cycles exactly (CYCCNT); continue from a bp executes the bp'd instruction
  first (inference — 2.5c-1b tests it).
Weak points this session: reasons that quote an earlier value without saying why it still holds; values copied
  from a different context (Thread-mode 2.5b-2/2.4, old image 0x15A); pre-entry sp vs handler sp; value
  contradicting own reason (CONTROL 0x4); citations to rules not opened (RVJKL/RRPGL, "RAGCH", "Ippse",
  D1.2.33, log 2.3a for IPR9, 1a.5b for CYCCNT, .c line 59 for tick); composition items with two options
  ("at main or halt (a)", twice); old-image addresses (0x7a); GDB syntax (print/x control_s, clear 1); frame
  offsets (+0x14 = stacked LR).
Strengths: tick + CYCCNT discriminator design; FPCA-at-entry reasoning; objdump two-source breakpoint check;
  C1 IPR9 evidence chain (measured at main + at for(;;) + literal check + GDB state).

## Session 14 (2026-11-02)

### Session-14 intake + image check
- learning_log.md: ## Session 13 once (line 1412) after ## Session 12 (1270); session13_log_blocks.md — all 8
  blocks present exactly once, contiguous at the end. Nothing missing or duplicated.
- Bench Map read at v49 → republished v50 … v53 during the session.
- Saved .c line 54 = session-12 comment (one line); line numbers unchanged. No rebuild (sources = session-13
  build). New debug launch → stop at main: $pc 0x16e; x/2i 0x216 = nop / b.n 0x216 → image 2.5a in flash.

### 2.5c-1b composition accepted (rounds 2–5, session 14, 2026-11-02)
Halts: main 0x16E: break *0x14C (#2), break *0x114 (#3), continue → (a) 0x114: pc, sp, tick 0x2000_800C,
  CYCCNT 0xE000_1004 → (b) same halt: ISPR1/ISER1 ← 0x00000080 ({unsigned int}), info breakpoints, delete 3,
  read ISPR1, ISER1, ICSR, continue → (c) 0x14C: pc, sp, x/8xw $sp, x/1xw $sp+0x20, tick, CYCCNT; delete 2,
  break *0x114 (#4), continue → (d) 0x114: pc, sp, tick, CYCCNT → end: delete, continue, LED 10 ON / 10 s.
  Comparators: 2 / 2 / 1 / 1 / 1 / 0.
Rule (d): resumed → tick = tick(a), Δ small; new entry → tick(a)+1, Δ ≥ 48,000.
C4: ReturnAddress (+0x18 at (c), RQLHB p.105) decides whether push {r7} ran → sp at entry = sp(a) or sp(a)−4
  → aligned? pad 0/4 + SPREALIGN 0/1 (RPBVF p.109) → sp(c) = entry − pad − 32 B (FPCA 0: RWYWW step 6 p.104,
  log 2.5c-1 CONTROL_S 0; RSGKY p.108). push {r7} = −4 B (2.5c-1 frame analysis 0xFF88 → 0xFF84).
Composition errors fixed on the way: generic read lists (no tick/CYCCNT), stop-at-main from print &main,
  W1S "setup preamble", (b) as a separate pc, stepi at (c) (would itself be the step-over), return bp 0x118
  (not in objdump; merges both outcomes), #1 listed as alive (temporary, consumed), comparator count via
  "status registers"/DWT, sp(a) used as sp at entry.

### 2.5c-1b step-over test — session 14, 2026-11-02
Composition: accepted round 5 (see composition block). Predictions LOCKED (26 rows), score 8/26.
Result (raw):
  (a) pc 0x114, sp 0x2000_FF88, tick 0, CYCCNT 0x0001_1F18 (= 2.5c-1 (a), reproducible).
  (b) info breakpoints #2 0x14C, #3 0x114 (hit 1); delete 3; ISPR1 0x80, ISER1 0x80, ICSR 0x00C3_780F.
  (c) pc 0x14C, sp 0x2000_FF68; frame R0 0xA, R1 0x2000_8010, R2 7, R3 0xE000_E010, R12 4,
      LR 0xFFFF_FFE9, ReturnAddress 0x114, RETPSR 0x0900_000F; @0x2000_FF88 = 0xA; tick 0; CYCCNT 0x0001_1F29.
  (d) bp #4: pc 0x114, sp 0x2000_FF88, tick 0, CYCCNT 0x0001_1F52. delete; continue → LED 10 ON / 10 s.
Learned: with no bp at the current pc, IRQ 39 is taken before the instruction at 0x114 executes
  (ReturnAddress 0x114, RQLHB p.105); entry sp 0xFF88 aligned → SPREALIGN 0, no pad (RPBVF p.109);
  8-word frame (FPCA 0 in handler) → 0xFF68. Word above the frame = SysTick's stacked R0 (its Thread frame
  starts at 0xFF88). (d) = resumed SysTick (tick 0, Δ(a→d) 58 cycles) — bp 0x114 catches a return landing.
  CLOSED: 2.5c-1 ReturnAddress 0x116 / SPREALIGN 1 were caused by the bp at the current pc — the debugger
  executes the breakpointed instruction on continue (measured contrast). Rule: delete the bp at the current
  pc before continue when the first instruction matters.
  Cycles: (a)→(c) 17 (entry, no handler instruction; incl. debug restart — inference); (c)→(d) 41
  (IRQ 39 handler + return); CYCCNT at first SysTick entry 0x1_1F18 in both sessions.
Grades: A1 ✅ A2 ❌ (16-bit; "Thread baseline" — 0xFF88 is handler sp) A3 ½ (no source) A4 ❌ (log 2.5c-1:
  0x1_1F18) B1 ✅ B2 ✅ B3 ❌ (in handler; log 2.5c-1 0x00C3_780F) C1 ✅ C2 ❌ (contradicts own C4/C9)
  C3–C7 ❌ (log 2.5c-1 frame) C8 ❌ (stacked = SysTick's EXC_RETURN 0xFFFF_FFE9) C9 ½ (no rule/cause)
  C10 ❌ (Q, IPSR 15) C11 ❌ (contradicts C9) C12 ❌ C13 ❌ D1 ✅ D2 ❌ D3 ❌ D4 ❌ D5 ✅ E1 ✅.
Weak points (new): not using the log for values measured at the SAME halt in an earlier run — only the
  rows the change affects should differ (13 of 18 misses); 16-bit addresses after a Lock flag;
  decimal CYCCNT guesses with no source.
Strengths: composition converged to a design that separated both outcomes; D5 relation right.
Still open: 2.5b-2 ReturnAddress (0x218 expected by the same mechanism — inference).

### Suspend operation timeout on Terminate — session 14, 2026-11-02 13:50
Context: after 2.5c-1b end (console delete → continue, free-running, ISER1 bit 7 = 1).
  IDE Breakpoints view still listed 0x14C and 0x114 (console delete does not update the IDE list).
LED at the time: blinking (core running our code — probe failed to halt it).
Recovery: OK → console `interrupt` → GDB "No support for interrupting the remote target." → then
  "Program received signal SIGINT" → halted in main for(;;) (.c line 142). print/x $pc typed while running:
  no output; not re-read before Terminate → pc at the SIGINT halt: not read (source line 142 only).
  Terminate succeeded after the SIGINT halt (no Disconnect, no unplug).
Rule: Breakpoints view → Remove All before every launch (IDE list ≠ GDB list).

### Probe lost after the timeout + recovery — session 14, 2026-11-02 13:58–14:04
New debug launch after Terminate: incremental build (text 768 — image unchanged), then
  TargetDiscovery → "SWD Configuration: 0 available SWD Devices detected". Breakpoints view empty.
Fixed by ending a leftover process in Task Manager (name not recorded); no USB replug.
Launch: GDB "Program stopped. 0x20018040 in ?? ()" at connect → Temporary breakpoint 1, main line 94 →
  print/x $pc $2 = 0x16e ($1 not typed by the user — IDE-internal, inference). GDB-only state none.
  CP1252→UTF-32 'main' warning: host encoding, harmless (inference).
Rule: after a Suspension Timeout, check Task Manager for a leftover debug process before replugging.
Open question (new): core halted at 0x2001_8040 (RAM outside RAMB/our image) at probe connect — whose
  code? (inference: LinkServer flash routine or connect/reset sequence; test: x/4i 0x20018040 at the next
  connect, before stop-at-main — optional, low priority).

### 2.5c-2 composition accepted (rounds 1–4, session 14, 2026-11-02)
Same as 2.5c-1b except IPR9 PRI_N3: set {unsigned char}0xE000E427 = 0xA0 (level 5 = 0b101 in [7:5], log 2.2b;
  NVIC_IPRn n 0–123 byte-accessible, D1.2.185 p.1769) at (b), read back x/1xb.
Halts: main 0x16E: break *0x14C (#2), *0x114 (#3), continue → (a) 0x114: pc, sp, tick, CYCCNT →
  (b) same halt: ISPR1/ISER1 ← 0x80, IPR9 byte ← 0xA0, info breakpoints, delete 3, read ISPR1, ISER1,
  IPR9 byte, ICSR, continue → (c) 0x14C: pc, sp, x/8xw $sp, $sp+0x20, tick, CYCCNT, $lr, ICSR, SHCSR;
  delete 2, break *0x216 (#4), continue → (d) 0x216: pc, sp, counter 0x2000_8004, tick, IABR1, ICSR →
  end: delete, continue, LED 10 ON / 10 s. Comparators 2/2/1/1/1/0.
C4 discriminators at (c) (preempted ← log 2.5c-1 (c) / 2.5c-1b (c) vs taken after SysTick's body):
  tick (.c line 78), stacked ReturnAddress (+0x18), ICSR.RETTOBASE (bit 11), SHCSR.SYSTICKACT (bit 11),
  live LR EXC_RETURN.Mode (bit 3, D1.2.96 p.1628–1629). Limit: tail-chain vs return-then-new-entry → 2.6.
Theory: RRKCQ/RCJDM/RVJPG p.93, RKLWH p.104, RKPSS p.118, RFKXX p.127, RSGKY p.108, RQRWD p.119 (all opened).
Composition errors fixed on the way: IPR byte 0x20 (= level 1, would preempt), "IP0–IP115" (CMSIS name),
  reset-value reason for IPR9, VECTACTIVE as discriminator (55 in both cases), (c) reads missing for two
  discriminators, ISRPREEMPT value inside a composition, landing bp 0x114 (= next SysTick) → 0x216, bp 0x216
  set at main (would halt before the first SysTick), ICSR/LR preempted values copied from (b)/(a)
  (different context), EXC_RETURN.Mode polarity inverted.
Tutor correction: the bp-0x216-at-main review said "log 2.5c-1b: (a)→(d) shows one period = 48,000" — the
  48,000-cycle period is log 2.5c-1 ((a)→(d) new entry); 2.5c-1b (a)→(d) was 58 cycles. Conclusion unchanged.

### 2.5c-2 IRQ 39 level 5 pended inside SysTick_Handler — session 14, 2026-11-02
Predictions LOCKED (31 rows). Score 15/30 (B1 ungraded: ISPR1 read typed as 0xE000E20 → discarded).
Result (raw):
  (a) pc 0x114, sp 0x2000_FF88, tick 0, CYCCNT 0x0001_1F18 (= 2.5c-1/2.5c-1b).
  (b) IPR9 byte ← 0xA0 → x/1xb 0xa0; ISER1 0x80; ICSR 0x0043_780F (ISRPREEMPT 0, ISRPENDING 1,
      VECTPENDING 55, RETTOBASE 1, VECTACTIVE 15). delete 3; continue.
  (c) pc 0x14C, sp 0x2000_FF88; frame R0 0xA, R1 0x2000_8010, R2 7, R3 0xE000_E010, R12 4, LR 0x0000_02E1,
      ReturnAddress 0x218, RETPSR 0x0910_0000; @0x2000_FFA8 = 0; tick 1; CYCCNT 0x0001_1F53;
      $lr 0xFFFF_FFE9; ICSR 0x0000_0837; SHCSR 0. delete 2; break *0x216 (#4); continue.
  (d) pc 0x216, sp 0x2000_FFF0, counter 1, tick 1, IABR1 0, ICSR 0. delete; continue → LED 10 ON / 10 s.
Learned: level 5 (0xA0) vs active SysTick level 4 → no preemption (RRKCQ p.93); ICSR.ISRPREEMPT 0 while
  halted (vs 1 at level 0, 2.5c-1b) — "will not handle" (D1.2.126 p.1678). IRQ 39 taken after SysTick's body
  (tick 1, SHCSR 0, RETTOBASE 1) on SysTick's Thread frame (sp 0xFF88) with SysTick's EXC_RETURN 0xFFFF_FFE9
  kept in LR → consistent with tail-chaining (RFKXX p.127); return-then-new-entry would leave the same
  sp/frame → cycle test 2.6. SysTick's first entry interrupted main at b.n 0x218; RETPSR 0x0910_0000 (= 2.4:
  Q, T, SFPA 1, IPSR 0); stacked LR = main's stale LR 0x2E1 (return address inside uart4_puts — inference).
  CYCCNT (a)→(c) 59 cycles = SysTick body (no bp at 0x114) + transition into IRQ 39.
Grades: A1 ✅ A2 ½ (called "Thread" sp) A3 ½ (no source) A4 ½ (no why-unchanged) B1 ungraded B2 ✅ B3 ✅
  B4 ❌ (ISRPREEMPT) C1 ✅ C2 ❌ (nested sp) C3–C7 ½ ×5 (no why-same) C8 ❌ (Thread frame LR slot) C9 ❌ (0x218;
  log could not decide → "no prediction") C10 ❌ (log 2.4 0x0910_0000) C11 ❌ (FP reserved word, not pad)
  C12 ✅ C13 ❌ (period ≠ handler cost) C14 ❌ (E1 bit 3 = 0, own reason) C15 ❌ (bit 11 = 0, own reason)
  C16 ✅ D1 ✅ D2 ❌ (Thread sp 0xFFF0) D3 ❌ (increment ran) D4 ✅ D5 ✅ D6 ✅ E1 ✅.
Weak points (new): copying values from the previous step into rows its one change affects (B4, C2, C11);
  back-decode still skipped (C14, C15); paste typo in an address (B1).
Strengths: composition converged on a 5-row discriminator; bp 0x216 landing design caught 0x218.

### Stop after free-run — session 14, 2026-11-02 14:38–14:54
After 2.5c-2 end (console delete → continue, free-running): console `interrupt` → no output for 10 min.
Tutor correction: the session-14 rule "interrupt → wait for SIGINT → Terminate" was built on one sample in
  which GDB printed "No support for interrupting the remote target"; the SIGINT then most likely came from
  the IDE's own suspend (inference). Console `interrupt` is NOT a working halt path on this setup (2/2).
Recovery: Suspend/Terminate hung → OK → Remove All Terminated → leftover process ended → USB unplugged.
Rule: end every GDB run while HALTED at a breakpoint and Terminate from there (worked 2/2 later today);
  to stop a free run use the Debug view Suspend (Thread #1); if it hangs → OK → Remove All Terminated →
  end leftover process → unplug. Console `interrupt` is unreliable here.
Open question: Suspension Timeout now 3 of 3 stops after a free run started by a console `continue`
  (1e.4e-2, 2× session 14) — cause unknown; test idea: free-run with the IDE Resume button instead.

### 2.5d composition accepted (rounds 1–3, session 14, 2026-11-02)
main pends IRQ 39 itself after SysTick enable: REG32(NVIC_ISER1) = NVIC_ISER1_IRQ39_SET;
  REG32(NVIC_ISPR1) = NVIC_ISPR1_IRQ39_SET; dsb; isb; then reads g_lpflexcomm4_irq_count → uart4_puts
  "IRQ 39 taken\r\n" (> 0) or "IRQ 39 not taken yet\r\n". Macros (after .c line 53): NVIC_ISER1 0xE000E104u
  (D1.2.186 p.1770), NVIC_ISPR1 0xE000E204u (D1.2.187 p.1771–1772), NVIC_ISER1_IRQ39_SET / NVIC_ISPR1_IRQ39_SET
  (1u << 7). IPR9 not written (measured 0 at main, no code writes it; Thread execution priority 256, RVJPG p.93).
Order enable/pend irrelevant in running code (2.5b-1 pending-but-disabled stays pending; 2.5b-2 taken on
  enable). Barriers: DSB completes the SCS write (RSJQJ p.222), alone not enough for visibility (RNPDJ
  p.222); ISB = context synchronization event → side effects visible to following instructions (RWQQB
  p.268, RHMNM p.222). Inference: visible ≠ handler finished (RKLWH p.104 "finite time").
Composition errors fixed on the way: raw addresses instead of macros, pointer-cast macros inside REG32,
  "v1.5" header, line numbers overlapping line 54, uncommented new lines, DSB claimed sufficient,
  two-option C5, "pending" label for a not-yet-taken read, generic priority reason.

### 2.5d pend IRQ 39 from C — session 14, 2026-11-02
Saved file (tutor-checked; user did not send the line table — from the next edit the user sends it):
  header line 13; macros 55–58; counter 60; g_init 62; g_zero 63; uart4_putc 65–72; uart4_puts 74–80;
  SysTick_Handler 82–88 (increment 84); LP_FLEXCOMM4_IRQHandler 90–96 (comment 90–92, line 92 now "no FP, no
  peripheral access — main and GDB read the counter.", increment 95); main 98–161 (Hello 142, PRI_15 144,
  SysTick enable 146, ISER1 148, ISPR1 149, dsb 150, isb 151, if 153–157, for(;;) 159–160).
Size predictions LOCKED (10 rows). Score 3/10.
Result: Clean build 0/0, FLASH 876 B, RAMB 16 B, size text 871 data 4 bss 12 dec 887.
  objdump: main 0x168 (stop-at-main 0x16E, $pc 0x16e); ISER1 0x216–0x21B, ISPR1 0x21C–0x221, dsb 0x222,
  isb 0x226, ldr/ldr/cmp/beq 0x22A–0x231, then ldr r0/bl 0x232/0x234, b.n 0x238, else ldr r0/bl 0x23A/0x23C,
  join nop 0x240; for(;;) nop 0x242 / b.n 0x244; pad 0x246; literals 0x248–0x2B0 (27); main ends 0x2B4
  (+64 B); Reset_Handler 0x2B4; Default_Handler 0x308; uart4_puts 0x30C; strings Hello 0x338,
  "IRQ 39 taken\r\n" 0x340, "IRQ 39 not taken yet\r\n" 0x350; .text 0xE0 size 0x287 (ends 0x367);
  .data LMA 0x368; .bss LMA 0x36C.
  Terminal after RESET (no debugger): "Hello" / "IRQ 39 taken". LED 10 ON / 10 s.
  Debug launch → stop at main → Terminate while halted: no timeout.
New size rules: dsb / isb = 4 B each (f3bf 8f4f / f3bf 8f6f); -O0 if/else = cmp + beq + then (… + b.n) +
  else + join nop (2 B); string literals start 4-aligned (pad after a 15-byte string); FLASH Used = .data
  LMA end — text + data + alignment pad when text ends unaligned (876 = 871 + 4 + 1).
Grades: S1 ❌ (estimate; ≠ own S3+S4) S2 ½ (bss split missed tick.0) S3 ❌ (~ estimates) S4 ✅ S5 ½ (counter 4 B,
  tick.0 missing) S6 ½ (uart4_puts is after main) S7 ❌ (0x242) S8a ❌ (0x340) S8b ❌ (0x350) S9 ½ (no mechanism).
Weak point (again): sizes estimated instead of counted per instruction from the logged rules (5th time).

### 2.5d-2 composition accepted (rounds 1–3, session 14, 2026-11-02)
Image 2.5d, no rebuild. main 0x16E: break *0x14C (#2); clear COM3 by hand; continue → (a) 0x14C: pc, sp,
  x/8xw $sp (ReturnAddress +0x18, RETPSR +0x1C — State context at the lowest addresses of the 26-word frame,
  B3.19 p.107, measured 2.5c-2), $lr, counter, ISPR1, IABR1, ICSR; delete 2; break *0x242 (#3); continue →
  (b) 0x242: pc, sp, counter, ICSR → Terminate while halted (no free run).
Thread-context proof: LR bit 3 (D1.2.96 p.1628), ICSR.RETTOBASE bit 11, RETPSR [8:0] (D1.2.221 p.1817).
ReturnAddress table (RQLHB p.105): 0x222 = ISPR1 store done, dsb not; 0x226 = dsb done; 0x22A = isb done;
  0x22C = counter address loaded, counter not → all consistent with "IRQ 39 taken"; ≥ 0x22E = counter
  loaded before the handler → would print "not taken yet"; < 0x220 impossible (nothing pending yet).
Composition errors fixed: frame offsets +0x68/+0x6C (State context is at $sp, not above the FP area),
  "CPACR sets FPCA" (first FP instruction does, 1b.4b; FPCA = FP context active, D1.2.13), "during" an
  instruction, "later" row unsplit, main pc 0x168 (stop-at-main 0x16E — 3rd time), invented GDB command
  "shell clear_com3", end-of-run free run contradicting C5.

### 2.5d-2 where is IRQ 39 taken (image 2.5d) — session 14, 2026-11-02
Predictions LOCKED (20 rows; A3–A7 "no prediction" flagged at Lock as gradable if the log/objdump gives
  them; A8 two values → first graded). Score 7/20.
Result (raw): main 0x16E → break *0x14C (#2), continue →
  (a) pc 0x14C, sp 0x2000_FF88; frame R0 0xA, R1 0x2000_8010, R2 0x80, R3 0xE000_E204, R12 4, LR 0x321,
      ReturnAddress 0x226, RETPSR 0x0910_0000; $lr 0xFFFF_FFE9; counter 0; ISPR1 0 (read twice); IABR1 0x80;
      ICSR 0x0000_0837. delete 2; break *0x242 (#3); continue →
  (b) "Breakpoint 3, 0x00000242 in main" (print/x $pc not typed; a duplicate break *0x242 #4 typed instead);
      sp 0x2000_FFF0; counter 1; ICSR 0. COM3: "Hello" / "IRQ 39 taken". Then Terminate while halted.
Learned: IRQ 39 was taken with the dsb at 0x222 completed and the isb at 0x226 not executed
  (RQLHB p.105) → on this core the pend took effect when the DSB completed the SCS store (RSJQJ p.222,
  RBMLS p.268) — one sample, implementation behaviour; architecturally the ISB is still required
  (RNPDJ p.222: DSB alone does not guarantee visibility). Frame R2/R3 = the ISPR1 write's registers;
  stacked LR = main's stale LR (0x321 = old 0x2E1 + 0x40 shift of uart4_puts); ISPR1 → 0 at entry
  (RWYWW step 9, again). Thread entry from main: sp 0xFFF0 → 0xFF88 (26 words), RETPSR 0x0910_0000.
Grades: A1 ✅ A2 ❌ (Thread sp 0xFFF0; 26 words = 104 B) A3–A7 ❌ (log 2.5c-2 / objdump 0x21C–0x21E gave them)
  A8 ❌ (Thread frame LR slot = main's LR) A9 ❌ (0x226) A10 ❌ (0x0910_0000) A11 ❌ (E1 bit 3 = 0 — repeat of
  2.5c-2 C14) A12 ✅ A13 ❌ (RWYWW 9) A14 ✅ A15 ❌ (bit 11 — repeat of 2.5c-2 C15) B1 ✅ (stop line) B2 ❌
  (repeat of 2.5c-2 D2) B3 ✅ B4 ✅ B5 ✅.
Weak point (new, TOP): corrections from the previous step's grade line not carried into the next
  predictions (5 repeats within one hour). Process: re-read the latest "Grades" line before predicting.

### Session 14 pause (2026-11-02 23:08)
Session ended at the user's request after 2.5d-2. Board: Terminate while halted at 0x242, then unplug
  (user to confirm at session 15 intake). GDB-only state: none (new session). Image 2.5d in flash.

### Session 14 summary (2026-11-02)
Scores: 2.5c-1b 8/26; 2.5c-2 15/30 (B1 ungraded); 2.5d size 3/10; 2.5d-2 7/20.
Closed: debugger step-over on continue from a bp at the current pc (2.5c-1b contrast: ReturnAddress 0x114,
  SPREALIGN 0, resumed at 0x114); level-5 IRQ 39 does not preempt SysTick level 4 and is taken after
  SysTick's body (2.5c-2); firmware-only pend works ("IRQ 39 taken" after RESET, no debugger, 2.5d);
  IRQ 39 taken after the DSB, before the ISB (2.5d-2, one sample).
New facts: ICSR.ISRPREEMPT 0 for a pending enabled IRQ without sufficient priority (0x0043_780F);
  CYCCNT at the first SysTick entry = 0x0001_1F18 in three runs with image 2.5a
  (2.5c-1, 2.5c-1b, 2.5c-2; deterministic from reset; not measured with image 2.5d); entry from (a) to the IRQ handler's first instruction 17 cycles
  (incl. debug restart, inference); SysTick body + transition to IRQ 39 = 59 cycles; IRQ 39 handler +
  return = 41 cycles; size rules for dsb/isb/if-else/strings/FLASH pad.
Debugger: 2 Suspension Timeouts after console-continue free runs; console `interrupt` does not halt;
  leftover process blocks the probe ("0 SWD devices") → end it in Task Manager; IDE Breakpoints view is
  not updated by console `delete`; core at 0x2001_8040 at probe connect (open). End runs halted + Terminate.
Tutor corrections: 2 — (1) "interrupt → SIGINT → Terminate" rule built on one ambiguous sample;
  (2) 2.5c-2 review cited 2.5c-1b for the 48,000-cycle period (it is 2.5c-1).
Weak points this session: rows copied from the wrong earlier run (2.5c-1b: too little copying; 2.5c-2:
  copying nested values into rows the change affects); the latest Grades line not re-read (5 repeats in
  2.5d-2); "Thread baseline 0xFF88" (it is the handler sp; Thread sp 0xFFF0) ×4; back-decode skipped
  (0xFFFF_FFE1, 0x37 vs RETTOBASE 1) ×2 each; EXC_RETURN predicted in a Thread frame's LR slot ×3; sizes
  estimated with "~" (5th time); "no prediction" where the log/objdump gives the value; main pc 0x168 vs
  stop-at-main 0x16E ×3; generic/invented items in compositions (IP0–IP115, shell clear_com3, pointer
  macros, v1.5 header); line-number table never sent; address typos in pastes (0xE000E20); missing
  print/x $pc at a halt.
Strengths: compositions converged each time; discriminator designs (tick + CYCCNT; 5-row C4; 0x216/0x242
  landing bps) worked; C5 barrier reasoning with the right rule IDs; firmware pend worked first build.

## Session 15 (2026-11-03)

### Session-15 intake + image check (2026-11-03)
- learning_log.md: ## Session 14 once (line 1525), last session; session14_log_blocks.md — all 14 blocks
  present exactly once, contiguous, whitespace-normalised diff clean. Nothing missing or duplicated.
- Bench Map read at v53 → republished v54 (intake; Phase 2 chip "2.5c" → "2.6"; two open-question
  items still carried the withdrawn "interrupt → SIGINT → Terminate" rule → replaced by the session-14 rule).
- Sources = session-14 build (line table matches) → no rebuild. .c line 92 wording question → see the
  tutor correction in the 2.6 composition round-1 block.
- New debug launch → stop at main: $pc 0x16e; x/2i 0x242 = nop / b.n 0x242 → image 2.5d in flash.
- objdump -s -j .isr_vector (image 2.5d): word0 0x2001_0000, word1 0x2B5, slots 2–7/11/12/14/16–54 0x309,
  8/9/10/13 = 0, slot 15 0x115, slot 55 (0xDC) 0x14D → inference from the +0x40 shift CONFIRMED.
- Board left halted at main (stop-at-main #1 consumed). GDB-only state: none.

### Theory mid-step: tail-chaining ELI5 (session 15)
- User read DDI0553 p.127 + TRM PDF p.28, asked for an ELI5. Explained: entry = packing a suitcase
  (26 words, sp 0xFFF0 → 0xFF88), return = unpacking; tail-chain = skip unpack + repack when a waiting
  exception beats the priority being returned to (Thread 256 vs level 5 = 0xA0 = 160). RFKXX p.127 1–3
  mapped to our board; IFCGM diagram; ISJSX p.127 (tail-chaining of async exceptions is IMP DEF);
  TRM §2.7.1 PDF p.28 ("The processor supports tail-chaining"); RNMTT p.124 late arrival (mirror case).
  Why 2.5c-2 could not tell: identical frame/sp/EXC_RETURN either way → only time differs.

### 2.6 composition round 1 + SysTick_Handler objdump (session 15, 2026-11-03)
objdump --disassemble=SysTick_Handler (image 2.5d): 0x114 push {r7}; 0x116 add r7,sp,#0; 0x118–0x120
  ++tick (ldr ← 0x144, ldr, adds, ldr ← 0x144, str); 0x122–0x126 reload + cmp.w #500; 0x12A bcc.n 0x13A;
  0x12C–0x130 tick = 0; 0x132–0x138 PTOR ← 0x40000 (literal 0x148); 0x13A nop (join); 0x13C mov sp,r7;
  0x13E ldr.w r7,[sp],#4 (4 B); 0x142 bx lr; literals 0x144 = 0x2000_800C, 0x148 = 0x5009_E04C. 56 B.
Round 1 rejected: SysTick listing not from our .axf (literal 0x150 inside LP_FLEXCOMM4_IRQHandler; no
  epilogue/cmp; bx lr "0x140" = middle of ldr.w); C1 answered the end halt, .c lines 141–142 (= 148–149),
  "no GDB writes" although main's level-0 IRQ 39 is taken before the first SysTick (2.5d-2) and nothing
  re-pends it; C3 bp on bx lr = the instruction under test (step-over, possibly interrupt-masked — inference);
  C4 bp 0x14C timing vs main's own IRQ 39; C5 baselines unnamed, 8-word vs 26-word mismatch; C6 ReturnAddress
  (R, ≥1 instr) discriminates; C7 p/x $eax (x86), no bp for halt (a), C2 write missing. C2 accepted.
Tutor correction: session-14 2.5d block recorded .c line 92 as "main and GDB read the counter."; the built
  file reads "main and GDB reads the counter after the run." (comment only; image unchanged).
Weak point (again): a generic/invented listing used as a source instead of our objdump (session 10, 14).

### 2.6 composition accepted (rounds 1–6, session 15, 2026-11-03)
Question: IRQ 39 level 5 pended while SysTick level 4 is active — tail-chain (T, RFKXX p.127: no unstack,
  no restack, no background instruction) or return-to-Thread + new entry (R)? ISJSX p.127: tail-chaining of
  async exceptions is IMP DEF; TRM §2.7.1 PDF p.28: "The processor supports tail-chaining".
Image 2.5d, GDB-only. main's own level-0 IRQ 39 is taken before the first SysTick (2.5d-2) → nothing pending.
C1 writes at (a) = first SysTick entry 0x114: ISER1 NOT written (bit 7 already 1 from .c line 148, no ICER1);
  IPR9 PRI_N3 set {unsigned char}0xE000E427 = 0xA0 (level 5, D1.2.185 p.1769, log 2.2b); ISPR1
  set {unsigned int}0xE000E204 = 0x00000080. Order irrelevant while halted (no instruction executes).
C3 start (b) = 0x13C (mov sp,r7; both if-paths join at nop 0x13A); bp deleted before continue → no
  single-step (2.5c-1b). Epilogue mov + ldr.w + bx lr (0x13C–0x142) inside Δ(b→c), cost unmeasured.
C4 end (c) = 0x14C, bp set at (b) (after main's IRQ 39 already ran).
C5 rule: Δ(b→c) < 17 → (T). Reason: (R) needs ≥ one 8-word entry (2.5c-1b: 17 cycles incl. one restart;
  26-word lazy frame ≥ 8-word cost — inference) + epilogue (≥ 3, 1 cycle/instr — inference) + an unstack;
  Δ(b→c) holds one restart too. Δ ≥ 17 → undecided (unstack not separable from 41 = 8 unmeasured IRQ 39
  instructions 0x14C–0x15A + shared epilogue 0x15C–0x162 + return).
C6: ReturnAddress discriminates only (R, ≥ 1 main instruction) (RQLHB p.105); R0–R3/R12/sp, live LR, ICSR
  identical under (T) and (R, 0 instr) — evidence log 2.5d-2 (a) and 2.5c-2 (c): LR 0xFFFF_FFE9, ICSR 0x837.
Cross-check: [Δ(a→b) + Δ(b→c)] − 59 (2.5c-2 (a)→(c), one restart) = one extra restart → tests the
  "restart overhead constant" inference. SysTick_Handler image 2.5a vs 2.5d: same 0x114, 56 B, literals
  0x2000_800C / 0x5009_E04C, source 82–88 unchanged since 2.4 → identical code (inference).
Halts: main 0x16E: break *0x114 (#2), continue → (a) 0x114: pc, sp, x/8xw $sp, lr, counter 0x2000_8004,
  ISER1, ISPR1, ICSR, CYCCNT; IPR9 byte ← 0xA0, ISPR1 ← 0x80; read back IPR9 byte, ISPR1, ICSR;
  info breakpoints, delete 2, break *0x13c (#3), continue → (b) 0x13C: pc, sp, CYCCNT; delete 3,
  break *0x14C (#4), continue → (c) 0x14C: pc, sp, CYCCNT, x/8xw $sp, lr, ICSR → Terminate while halted.
  Comparators 1/1/1/1.
Composition errors fixed on the way: generated SysTick listing (literal 0x150, bx lr "0x140" mid-ldr.w);
  C1 lines 141–142 (= 148–149) ×2; "no GDB writes needed"; ISER1 "still needed"; bp on bx lr (instruction
  under test); bp 0x14C at main (main's own IRQ 39) ×2; baselines unnamed / 17 = "entry/exit" / 17 + 41
  (contains R) / symbolic overheads ×4; LR "bit 3 = 0" and ICSR "nesting" in C6; p/x $eax; shell clear_com3
  (again); missing ISPR1 write and CYCCNT read at (a); bp #2 reused at (b); "zero restarts"; "matching
  opcodes" without an opcode dump.
Weak point (new): a later round reintroduced fixes from an earlier round (bp 0x14C at main, shell
  clear_com3) → build each round from the last ACCEPTED version.

### 2.6 tail-chain vs return-then-new-entry (CYCCNT) — session 15, 2026-11-03
Predictions LOCKED (34 graded rows + 5 no-prediction). Score 13.5/34 (B3, C3, D1, D2, D4 ungraded).
Result (raw): main 0x16E → break *0x114 (#2), continue →
  (a) pc 0x114, sp 0x2000_FF68; frame R0 0x6E, R1 0x2000_8010, R2 0x500B_401C, R3 0, R12 4, LR 0x321,
      ReturnAddress 0xF0, RETPSR 0x6910_0000; $lr 0xFFFF_FFE9; counter 1; ISER1 0x80; ISPR1 0; ICSR 0x80F;
      CYCCNT 0x0001_1F18. IPR9 byte ← 0xA0 (reads 0xa0), ISPR1 ← 0x80 (reads 0x80), ICSR 0x0043_780F.
      info breakpoints #2 0x114 (hit 1); delete 2; break *0x13c (#3); continue →
  (b) pc 0x13C, sp 0x2000_FF64, CYCCNT 0x0001_1F42. delete 3; break *0x14C (#4); continue →
  (c) pc 0x14C, sp 0x2000_FF68, CYCCNT 0x0001_1F59; frame identical to (a); $lr 0xFFFF_FFE9; ICSR 0x837.
  Terminate while halted (not confirmed in the paste — confirm at session 16 intake).
Δ(a→b) 0x2A = 42; Δ(b→c) 0x17 = 23; [42 + 23] − 59 (2.5c-2) = 6.
Learned: Δ(b→c) 23 ≥ 17 → locked C5 rule: UNDECIDED. Extra halt/restart ≈ 6 cycles (inference: same
  SysTick path, code identical — inference). Inference chain (restart 6 constant; 1 cycle/instr; 26-word ≥
  8-word): epilogue + transition = 17 net; 8-word entry = 11 net → (R) would need unstack ≤ 3 cycles for
  ≥ 8 words → favours tail-chaining (TRM §2.7.1 PDF p.28), not proven (→ 2.6b). Frame/LR/ICSR identical at
  (a)/(c) (C6 confirmed); ReturnAddress unchanged 0xF0 → excludes (R, ≥ 1 main instruction) (RQLHB p.105).
  First SysTick CYCCNT 0x1_1F18 on image 2.5d too (main up to SYST_CSR store 0x210–0x214 unchanged) → 4 runs.
  SURPRISE: first SysTick (1 ms after enable) interrupted main inside uart4_putc (ReturnAddress 0xF0) while
  "IRQ 39 taken\r\n" was still transmitting (87 µs/char, 8-deep TX FIFO — inference); R0 0x6E = 'n',
  LR slot 0x321 = uart4_puts return after bl uart4_putc (2.5d-2 inference now supported); RETPSR 0x6910_0000
  = Z, C, Q, T, SFPA 1, SPREALIGN 0, IPSR 0; (b) sp 0xFF64 = push {r7} at 0x114.
  [R3 / Thread-sp decode: see the uart4_putc block — tutor correction there.]
Grades: A1 ✅ A2 ❌ (image 2.5d changed main after enable) A3 ❌ (nested 2.5c-1 (c) frame) A4 ½ A5 ❌ A6 ❌
  A7 ½ A8 ❌ (0x321; log 2.5d-2) A9 ❌ A10 ❌ (Q/SFPA, 3rd time) A11 ✅ A12 ✅ A13 ✅ A14 ✅ A15 ❌ (copied
  AFTER-pend value; 2.5c-1 (a) 0x80F) A16 ❌ (no prediction; log 0x1_1F18 deterministic) A17 ✅ A18 ✅
  A19 ❌ (ISRPREEMPT 0 at level 5, 2.5c-2 (b) — repeat of B4) B1 ✅ B2 ❌ (push {r7} at 0x114 ignored) B3 ungr.
  C1 ✅ C2 ❌ (value from A2; reason right) C3 ungr. C4 ❌ C5 ✅ C6 ❌ C7 ❌ C8 ✅ C9 ❌ C10 ❌ C11 ❌
  C12 ❌ (0xFFFF_FFED vs cited 2.5c-2 0xFFFF_FFE9; contradicts A11/C6) C13 ✅ D1/D2/D4 ungr. D3 ½.
Weak points (new): AFTER-write value copied into a BEFORE row (A15); prologue push ignored for sp inside a
  handler (B2); nested-frame values used for a Thread frame (A3–A10). Repeats: Q/SFPA, ISRPREEMPT level 5,
  EXC_RETURN miscopied, "no prediction" with a logged value.
Strengths: composition's one-sided rule gave the honest verdict; D4 cross-check yielded restart cost;
  C6 verdicts confirmed.

### uart4_putc objdump + 2.6 landing decode (session 15, 2026-11-03)
uart4_putc (image 2.5d): 0xE0 push {r7}; 0xE2 sub sp,#12; 0xE4 add r7,sp,#0; 0xE6 mov r3,r0; 0xE8 strb r3,[r7,#7];
  0xEA nop; poll loop 0xEC ldr r3 ← 0x10C (STAT 0x500B_4014), 0xEE ldr r3,[r3], 0xF0 and.w #0x800000 (TDRE),
  0xF4 cmp #0, 0xF6 beq.n 0xEC; 0xF8 ldr r2 ← 0x110 (DATA 0x500B_401C); 0xFA ldrb; 0xFC str; 0xFE nop;
  0x100 adds r7,#12; 0x102 mov sp,r7; 0x104 ldr.w; 0x108 bx lr; 0x10A pad nop; literals 0x10C, 0x110.
2.6 (a) frame decoded: ReturnAddress 0xF0 = after the STAT read (0xEE), before and.w (RQLHB p.105);
  R3 0 = raw STAT (TDRE 0, TC 0 — transmitter busy); Z 1 from the previous cmp; R0 0x6E = 'n' argument;
  R2 = DATA address from the previous character (inference). Thread sp at entry 0xFFD0 (main 0xFFF0 → puts
  −16 (non-leaf shape, not disassembled) → putc push −4, sub −12) → − 0x68 = 0xFF68, aligned → SPREALIGN 0.
Tutor correction: 2.6 review said "R3 0 = STAT & TDRE" — and.w at 0xF0 had not executed; R3 = raw STAT.
Recommendation (tutor, accepted by user): 2.6b before moving on (measure the (R) unstack cost directly).

### 2.6b composition accepted (rounds 1–4, session 15, 2026-11-03)
Purpose: measure epilogue + unstack + 1 restart (plain return, nothing pending) → two-sided rule for 2.6.
Image 2.5d, GDB-only, no writes. Proceed only if at (a): ISPR1 bit 7 = 0, counter shows main's pend taken
  (2.5d-2), ICSR VECTPENDING = 0 and ISRPENDING = 0 (D1.2.126 p.1678–1679); LPUART4 IE bits 0 (2.5b-1);
  SysTick period 48,000 ≫ window.
Start (b) 0x13C, deleted before continue (= 2.6 C3; restart overhead identical — inference).
End = bp at the stacked ReturnAddress read at (a) ($sp+0x18), set at (a): core stays in SysTick_Handler until
  bx lr → no Thread fetch of that address before the landing (precedent 2.5c-1b (d)).
Landing proofs: sp = (a) sp + 0x68 (FType 0 → 26-word extended frame, D1.2.96 p.1628–1629); live xPSR IPSR
  (B3.5 RZFHH p.74, D1.2.142 p.1707); ICSR VECTACTIVE (RETTOBASE UNKNOWN in Thread); live LR = stacked LR
  slot (RNCQN p.118 return procedure); CONTROL_S FPCA = NOT EXC_RETURN.FType (RQRWD p.119, D1.2.13 p.1480).
Rule for 2.6: (R) ⇒ Δ(b→c)_2.6 ≥ Δ_2.6b + 17 − 6 = Δ_2.6b + 11 (restart 6 from 2.6 D4 — inference;
  26-word Thread entry ≥ 8-word 17 — inference); Δ(b→c)_2.6 < Δ_2.6b + 11 ⇒ (T).
Halts: main 0x16E: break *0x114 (#2), continue → (a) 0x114: pc, sp, x/8xw $sp, lr, counter, ISPR1, ICSR,
  CYCCNT, $sp+0x18; delete 2; break *0x13c (#3); break *<RA> (#4); continue → (b) 0x13C: pc, sp, CYCCNT;
  delete 3; continue → landing <RA>: pc, sp, CYCCNT, xpsr, control_s, lr, ICSR → Terminate while halted.
  Comparators 1/2/1/1.
Composition errors fixed: ISPR1 read missing; "no other IRQs enabled" without evidence → ICSR; landing reason
  (RQLHB alone) → Handler-until-bx-lr + precedent; bp timing (a) vs (b); 26-or-8-word frame; "Thread
  baseline" sp; RETPSR rule cited for live xPSR; RETTOBASE in Thread; LR rule unnamed; C5 bound without
  Δ_2.6b and without restart count; values instead of conditions; FType 0 decoded as standard; 26 words = 100 B.

### 2.6b unstack-alone baseline (plain return to Thread) — session 15, 2026-11-03
Predictions LOCKED (29 rows). Score 9/26 (L3, D2, D3 ungraded).
User asked mid-run what "RA" means → explained (ReturnAddress = word at $sp+0x18, 7th of x/8xw).
Result (raw): main 0x16E → break *0x114 (#2), continue →
  (a) pc 0x114, sp 0x2000_FF68; frame 0x6E, 0x2000_8010, 0x500B_401C, 0, 4, 0x321, RA 0xF0, RETPSR 0x6910_0000;
      $lr 0xFFFF_FFE9; counter 1; ISPR1 0; ICSR 0x80F; CYCCNT 0x0001_1F18; $sp+0x18 = 0xF0 → conditions met.
      delete 2; break *0x13c (#3); break *0x000000f0 (#4); continue →
  (b) pc 0x13C, sp 0x2000_FF64, CYCCNT 0x0001_1F42. delete 3; continue →
  landing "Breakpoint 4, 0x000000f0 in uart4_putc (c=110 'n')": pc 0xF0, sp 0x2000_FFD0, CYCCNT 0x0001_1F57,
      xPSR 0x6900_0000, CONTROL_S 0xC, $lr 0x321, ICSR 0. Terminate (not confirmed in the paste).
Δ(a→b) 42 (= 2.6, reproducible); Δ_2.6b (b→landing) 0x15 = 21.
Learned: (R) bound for 2.6 = 21 + 17 − 6 = 32 > 23 → TAIL-CHAINING (T) decided (accepted rule). Without the
  restart-6 inference, (R) would need a restart ≥ 15 cycles. Tail-chain transition ≈ plain return + 2 cycles.
  Closes the 2.5c-2 open question; ISJSX IMP DEF settled for this core, consistent with TRM §2.7.1 PDF p.28.
  Inferences kept: restart constant; 26-word entry ≥ 8-word 17; one sample each.
  Return decoded with RWCGV p.121 (opened): State context popped → LR 0x321 = stacked LR slot (unstack proof);
  APSR/EPSR/IPSR from RETPSR → xPSR 0x6900_0000 (Z C Q T, IPSR 0; SFPA not in xPSR); CONTROL.SFPA ←
  RETPSR.SFPA → CONTROL_S 0xC (SFPA 1, FPCA 1 = NOT FType 0 per RQRWD p.119, nPRIV 0, SPSEL 0); sp 0xFFD0 =
  0xFF68 + 0x68 (Thread sp inside uart4_putc now MEASURED). RFVTL p.112: exception entry, incl. tail-chaining,
  clears CONTROL.SFPA. GDB "c=110 'n'" → R0 0x6E = character argument CONFIRMED. First SysTick CYCCNT
  0x1_1F18 — 5th run.
Grades: A1 ✅ A2 ❌ A3 ❌ A4 ✅ A5 ❌ A6 ❌ A7 ✅ A8 ❌ A9 ½ A10 ❌ A11 ✅ A12 ✅ A13 ½ A14 ❌ (after-pend value
  again) A15 ❌ (2.6 logged) B1 ✅ B2 ❌ (push {r7} again) B3 ❌ (2.6 logged) L1 ✅ L2 ❌ (own C4 said 0xFFD0)
  L3 ungr. L4 ❌ (Q/flags) L5 ❌ (contradicts own FPCA reason; nPRIV 0 = privileged) L6 ❌ (mechanism right,
  value from A8) L7 ✅ D1 ❌ (2.6 = 42) D2/D3 ungr. D4 ❌ ((T); "undecided" not an option).
Weak point (new, TOP): "unchanged from 2.6" rows copied the 2.6 PREDICTIONS, not the 2.6 RESULT line
  (11 rows) → copy from the log block's "Result (raw)" line only.
Repeats: before/after a write; prologue push; "Thread baseline 0xFFF0" (5th); Q lost; value vs own reason.
Strengths: two-sided rule decided the question; landing proofs (sp, LR, CONTROL) worked as composed.
Source refinement: return procedure = RWCGV p.121 (more precise than RNCQN p.118 cited in the composition).

### End-of-Phase-2 core_cm33.h mapping (read-only) — session 15, 2026-11-03
Source: SDK_2.x_FRDM-MCXN236 26.06 package (C:\Users\LENOVO\mcuxpresso\02\SDKPackages\…p2site_26.6.0…)
  → copied to Documents\core_cm33_sdk26.6.h (3245 lines); header NOT in any build. How the SDK was installed:
  not recorded (inference: IDE SDK installer, from the p2site folder name). Copies also inside capstone and
  example projects (frdmmcxn236_led_blinky_peripheral, frdmmcxn236_lpuart_edma_transfer) — not used.
Bases (Select-String paste): SCS_BASE 0xE000E000UL 2038; DWT_BASE 0xE0001000UL 2040; SysTick_BASE = SCS_BASE +
  0x0010 2044; NVIC_BASE + 0x0100 2045; SCB_BASE + 0x0D00 2046; _NS group SCS_BASE_NS 0xE002E000UL 2072–2077.
Members: SysTick_Type CTRL/LOAD/VAL 984/985/986 (0/4/8); SCB_Type ICSR 524 (0x004), SHPR[12] uint8_t 529
  (0x018 → [11] = 0x023 = SHPR3 PRI_15), CPACR 546 (0x088); NVIC_Type ISER[16] 487 (0x000), ISPR 491 (0x100),
  IABR 495 (0x200 → [1] = 0xE000E304), IPR[496] uint8_t 499 (0x300 → [39] = 0xE000E427); DWT_Type CYCCNT 1136.
  All 11 own addresses = base + offset. CTRL; matches 984 (SysTick), 1135 (DWT), 1452 (MPU), 1567 (SAU).
CMSIS vs DDI0553: IABR declared __IOM (R/W) — DDI0553 D1.2.182 RO; ISER/ISPR W1S not expressible (plain __IOM).
Bit macros: SysTick_CTRL_COUNTFLAG 991–992, CLKSOURCE 994–995, TICKINT 997–998, ENABLE 1000–1001; no per-IRQ
  mask — __NVIC_EnableIRQ 2358/2363: ISER[IRQn >> 5] = 1 << (IRQn & 0x1F).
SysTick_Config 3108: LOAD 3115 → NVIC_SetPriority(SysTick_IRQn, (1 << 3) − 1 = 7 → byte 0xE0, lowest) 3116 →
  VAL 3117 → CTRL = 0x7 3118–3120. Ours: RVR 138 → CVR 139 → Hello 142 → PRI_15 level 4 (0x80) 144 → CSR 146.
  Both satisfy IPPGV; both set priority before enable. TZ_SysTick_Config_NS 3137 (SysTick_NS, 3147–3149) —
  second SysTick (RXPCW); _NS functions/bases only for Secure code (__ARM_FEATURE_CMSE == 3).
Errors fixed over 4 rounds: line numbers not from the file (rounds 1–2, incl. bit-macro lines carried over
  after a flag); base macros quoted as literals; IABR1 computed as 0xE000E204 (= ISPR1); SysTick_Config
  "exactly matches" without the NVIC_SetPriority write; own PRI_15 position "before RVR"; level 4 credited to
  2.3b (it is 2.4); _NS group explained as "architectural" instead of the logged PPB _NS-view fact.
Weak point (again, TOP): citations from memory — fixed only once the tool output was pasted. Rule: line
  numbers come from a Select-String / grep paste.

### Phase 2 self-test (paper) — session 15, 2026-11-03: 3.3/5
Q1 0.75: a ✅ 0x60 · b ✅ · c ½ (0x00C3_780F + decode right; source 2.5c-1b (b) not cited) · d ½ (RRKCQ p.93
  not cited). Q2 0.75: a ½ (0x176FF/95,999; IVHDT not named) · b ✅ 0x7 · c ½ (SysTick_Config(96000), level 7;
  line 3116 missing; IPR/SHPR two answers) · d ✅ 5 ON/10 s. Q3 0.8: a ✅ 0xFFF0 → 0xFF88 · b ½ (18 = FP
  context, not "Additional FP context") · c ✅ 0xFFFF_FFE9 decode · d ½ (two RETPSR values; RA right, first
  RETPSR value 0x6910_0000 wrong — board 0x0910_0000) · e ✅ LR slot = thread R14 (0x321).
  Q4 0.5: all 12 values right; no log entries cited (asked). Q5 0.5: a ½ (bound right; "tail-chain 6 cycles"
  confuses the restart cost) · b ½ (RNPDJ p.222 / RWQQB p.268 not named) · c ½ (0; RFVTL p.112 not named).
Improvement: session-14 TOP weak points answered right (Thread vs handler sp, LR slot, EXC_RETURN decode,
  ISRPREEMPT level 3 vs 5, Q/SFPA kept). Weak point (TOP, continuing): citations dropped even when the
  question asks for them.

### Phase 2 self-test GDB check — session 15, 2026-11-03
main 0x16E → break *0x114 (#2) → 1st hit: RA 0xF0 (uart4_putc, = 2.6) → continue (bp kept) → 2nd hit:
  pc 0x114, sp 0x2000_FF88; frame R0 0xA, R1 0x2000_8010, R2 0x500B_401C, R3 0, R12 4, LR 0x321, RA 0x242,
  RETPSR 0x0910_0000; $lr 0xFFFF_FFE9; info breakpoints: hit 2 times.
  IPR9 byte ← 0x60 → reads 0x60; ISPR1 ← 0x80 → ICSR 0x00C3_780F; delete 2; break *0x14C (#3); continue →
  pc 0x14C, sp 0x2000_FF68, $lr 0xFFFF_FFF1, ICSR 0x0000_0037, SHCSR 0x0000_0800. (Terminate not confirmed.)
Confirmed: Q1 a–d (level 3 = 0x60 preempts level 4: ISRPREEMPT 1, nesting EXC_RETURN 0xFFFF_FFF1, RETTOBASE 0,
  SYSTICKACT 1, 8-word frame from aligned 0xFF88 because the bp at the current pc was deleted — 2.5c-1b);
  Q3 a, c, e and RA (2nd SysTick lands in for(;;); LR slot 0x321 = puts' last bl putc).
Q3 d RETPSR first value 0x6910_0000 ❌ (board 0x0910_0000). Self-test stays 3.3/5.
Tutor correction: Z/C = 1 inferred from uart4_puts' final cmp 0,0 — board shows Z = C = 0 at main's for(;;);
  cause open (uart4_puts never disassembled).
New fact: on image 2.5d the 1st SysTick lands in uart4_putc (0xF0), the 2nd in main's for(;;) (0x242).
PHASE 2 CLOSED (self-test 3.3/5, GDB-checked).

### Field Guide document (session 15, 2026-11-03)
User asked for an ELI5 reference of everything learned. Created Claude Docs doc "M33 Field Guide: Phases 0–2
  in Plain Words" — https://claude.ai/artifact/Hgv9YwBtgSqw28xe3czTpQ (doc id 8724fa96-262c-4178-a6a3-5f101e3bf97d,
  body node 4f013e27-65c1). 16 sections: how to use · memory map · booting · register→pin · UART · code size ·
  CYCCNT/SysTick · exceptions/NVIC · frame/EXC_RETURN/return · preempt/wait/tail-chain · barriers · debugger
  rules · own macros vs CMSIS · prediction lessons · quick reference · still open. Written from the
  continuation prompt + session-15 record, not a fresh pass over all logs. Plan: extend at the end of each phase.

### Session 15 pause (2026-11-03 19:09)
Session ended at the user's request after Phase 2 closed. Board: last run ended halted at 0x14C — Terminate
  not confirmed in any session-15 paste (2.6, 2.6b, self-test check) → confirm at session 16 intake; unplug
  before the next session. GDB-only state: none (new session). Image 2.5d in flash (unchanged since session 14).

### Session 15 summary (2026-11-03)
Scores: 2.6 13.5/34; 2.6b 9/26; Phase 2 self-test 3.3/5 (GDB-checked). Compositions: 2.6 (6 rounds),
  2.6b (4 rounds), mapping (4 rounds).
Closed: image 2.5d .isr_vector (+0x40 shift confirmed); tail-chain vs return-then-new-entry → TAIL-CHAIN
  (2.6 Δ 23 vs (R) bound 21 + 17 − 6 = 32); first-SysTick landing in uart4_putc 0xF0 (R0 'n', Thread sp
  0xFFD0 measured); debug restart ≈ 6 cycles (inference); return procedure RWCGV p.121; CMSIS mapping;
  Phase 2.
New facts: Δ(a→b) SysTick 0x114 → 0x13C = 42 (twice); epilogue → tail-chained IRQ 23; epilogue → plain return
  21; first SysTick CYCCNT 0x1_1F18 in 5 runs (images 2.5a + 2.5d); 2nd SysTick lands in for(;;); landing
  xPSR 0x6900_0000 / CONTROL_S 0xC / LR 0x321; level 3 preempts (0xFFFF_FFF1, ICSR 0x37, SHCSR 0x800).
Tutor corrections: 3 — .c line 92 wording in the session-14 log; "R3 = STAT & TDRE" (raw STAT); Z/C = 1 at
  main's for(;;) (board Z = C = 0).
Weak points this session: copying own earlier PREDICTIONS instead of RESULTS (11 rows, 2.6b); after-write values
  in before rows (2.6 A15, 2.6b A14); prologue push ignored (B2 twice); "Thread baseline 0xFFF0" (5th);
  Q/SFPA lost (3rd); generated listings and line numbers not from our files (SysTick listing, header lines
  ×2); fixed errors reintroduced in later composition rounds; symbolic rules ×4; citations missing when asked
  (self-test). Improvement: self-test values right on all session-14 TOP items.
Strengths: compositions converged; one-sided then two-sided cycle rule; landing proofs; D4 restart cross-check.

## Session 16 (2026-11-03 / 2026-11-04)

### Session-16 intake + image check (2026-11-03)
- learning_log.md: ## Session 15 once (line 1760), last session; session15_log_blocks.md — all 14 blocks
  present exactly once, contiguous, whitespace-normalised diff clean. Nothing missing or duplicated.
- Bench Map read at v60 → republished v61 (intake), v62 (image + Z/C + 3.0 issued).
- Sources = session-14 build → no rebuild.
- Terminate/unplug: last session-15 run Terminated + board unplugged (user, 2026-11-04 10:49);
  session-16 pause Terminate while halted at main went through cleanly, then unplugged (user).
- New debug launch → stop at main: $pc 0x16e; x/2i 0x242 = nop / b.n 0x242 → image 2.5d in flash.
- objdump --disassemble=uart4_puts (image 2.5d): 0x30C push {r7,lr}; 0x30E sub sp,#8; 0x310 add r7,sp,#0;
  0x312 str r0,[r7,#4]; 0x314 b.n 0x326; body 0x316–0x324 (ldr, ldrb, mov r0,r3, bl 0xE0 at 0x31C, ldr,
  adds r3,#1, str); test 0x326 ldr, 0x328 ldrb, 0x32A cmp r3,#0, 0x32C bne.n 0x316; 0x32E nop; 0x330 nop;
  0x332 adds r7,#8 (0x3708); 0x334 mov sp,r7; 0x336 pop {r7,pc}. 44 B, no literals.
- Z/C open question CLOSED: 0x3708 = ADD (immediate) T2, setflags = !InITBlock() (DDI0553 C2.4.5 p.537) →
  last flag setter before main's for(;;) = adds r7,#8: 0x2000_FFE0 + 8 = 0x2000_FFE8 → N Z C V = 0 →
  RETPSR 0x0910_0000 (2nd SysTick from for(;;), session 15) explained. main after bl: b.n / nop only.
- puts frame = push 8 + sub 8 = −16 (now objdump, was "not disassembled") → putc entry sp 0xFFE0 − 4 − 12 =
  0xFFD0 = 2.6b landing sp. bl uart4_putc at 0x31C → return 0x320|1 = 0x321 = stacked LR slot (2.5d-2/2.6
  inference now CONFIRMED by objdump).
- Tutor corrections: (1) session 15 "Z = C = 1 from uart4_puts' final cmp" — missed the flag-setting -O0
  epilogue adds. (2) Log/prompt cite CCR bit 9 RES1 as D1.2.9 p.1472 → it is p.1473; bit 0 RES1 p.1474.

### Session 16 pause (2026-11-03 22:40)
Paused at the user's request after 3.0 was issued (F1–F9), before predictions. Board halted at
  stop-at-main 0x16E → Terminate while halted, then unplug (confirmed at resume). GDB-only state:
  none. Image 2.5d in flash. Bench Map v63. Resumed 2026-11-04 10:49 (Bench Map v64).

### 3.0 fault-register audit (read-only, stop-at-main 0x16E) — session 16, 2026-11-04
Predictions LOCKED (9 rows; F5/F6 "no prediction"). Score 5/7 (F5, F6 ungraded).
Result (raw): $pc 0x16e; CCR 0xE000ED14 0x0000_0201; SHCSR 0xE000ED24 0; CFSR 0xE000ED28 0; HFSR 0xE000ED2C 0;
  MMFAR 0xE000ED34 0x2000_FFFC; BFAR 0xE000ED38 0x2000_FFFC; msplim_s 0x0; psplim_s 0x0.
Decode: CCR bits 9 (p.1473) and 0 (p.1474) RES1, DIV_0_TRP 4 = 0, UNALIGN_TRP 3 = 0 (D1.2.9 p.1474).
  SHCSR SECUREFAULTENA 19 / USGFAULTENA 18 (p.1837), BUSFAULTENA 17 / MEMFAULTENA 16 (p.1838) all 0 →
  configurable faults escalate to HardFault ("When disabled … escalate to HardFault"). CFSR (D1.2.11 p.1477)
  MMFARVALID 7 / BFARVALID 15 = 0 → MMFAR/BFAR contents not valid (D1.2.166 p.1739, D1.2.6 p.1466; TRM
  Table 3-1 PDF p.31: no reset value). MSPLIM D1.2.177 p.1757, PSPLIM D1.2.218 p.1814: 0 = no limit.
  F9 (paper): DIV_0_TRP 0 → SDIV divisor 0 → result 0, no trap (C2.4.177 p.841; E2.1.234 p.2023
  IntegerZeroDivideTrappingEnabled() = CCR.DIV_0_TRP == '1').
Grades: F1 ✅ F2 ½ F3 ½ F4 ½ (reset value + own code; boot ROM not addressed; log 1b.4b had CFSR/HFSR/SHCSR 0
  measured at for(;;)) F5 ungr. F6 ungr. F7 ✅ (1a.5a at-main value) F8 ½ (reset value; ROM not addressed;
  no earlier PSPLIM read) F9 ✅ (N).
New open question: MMFAR = BFAR = 0x2000_FFFC — shared register (DDI0553 allows it) or same latched address?
  0x2000_FFFC = top stack word (inference: main's push {r7,lr} lr slot, or a ROM leftover). Test idea: GDB-write
  one, read the other (optional).
Weak point (again): Warm-reset value used as the at-main value — the boot ROM runs after the reset (4 rows).
Strengths: all 7 values right; F5/F6 "no prediction" correctly justified by the UNKNOWN reset rule; F9 option
  answered with the right rule and page.

### 3.1a build (composition + build predictions) — session 16, 2026-11-04
Composition accepted: .c header line 14 "3.1a: …"; line 65 volatile uint32_t g_div_result; lines 161–163
  volatile uint32_t numerator = 100u; volatile uint32_t divisor = 0u; g_div_result = numerator / divisor;
  (after the if/else 155–159, blank 160, before for(;;) 165). C2 first round gave "after line 58" (= NVIC
  macro; g_zero was line 63) → fixed. C3 cited UDIV; operation is C2.4.260 p.972 (SDIV C2.4.177 p.841).
  Saved file diffed against 2.5d: 4 insertions only.
Tutor correction: line table sent before the saved file was off by +1 from main on (main 101 → 100,
  ISER1/ISPR1 151/152 → 150/151, dsb/isb 153/154 → 152/153, if/else 156–160 → 155–159).
Build predictions LOCKED (10 rows; B4 addendum 900 → 904 before build). Score 8/10.
Result (raw): Clean build 0/0; FLASH 896 B, RAMB 20 B; text 891 data 4 bss 16 dec 911. .isr_vector 0x00–0xDF;
  .text 0xE0 size 0x29B (ends 0x37A); .data VMA 0x2000_8000 LMA 0x37C; .bss 0x2000_8004 size 0x10 (LMA 0x380).
  Symbols: g_init 0x2000_8000, g_lpflexcomm4_irq_count 0x2000_8004, g_zero 0x2000_8008, g_div_result
  0x2000_800C, tick.0 0x2000_8010 (SysTick literal 0x144 = 0x2000_8010). main 0x168: push {r7,lr}; sub sp,#16;
  add r7,sp,#0; stop-at-main 0x16E; f at [r7,#12] (0x17A/0x17C/0x184); … if/else 0x22E–0x23C (no join nop);
  0x240 movs r3,#100; 0x242 str [r7,#8]; 0x244 movs r3,#0; 0x246 str [r7,#4]; 0x248 ldr r2,[r7,#8]; 0x24A
  ldr r3,[r7,#4]; 0x24C udiv r3,r2,r3 (fbb2 f3f3); 0x250 ldr r2 ← 0x2C4 (0x2000_800C); 0x252 str r3,[r2];
  for(;;) nop 0x254 / b.n 0x256; literals 0x258–0x2C4 (28, no pad). uart4_puts 0x320 (bl at 0x206/0x234/
  0x23C). Strings 0x34C / 0x354 / 0x364. SysTick_Handler 0x114–0x14B unchanged except literal 0x144.
Grades: B1 ❌ (899; "loads/stores 12" = 8; missed −2 join nop and −2 pool pad) B2 ✅ B3 ✅ B4 ❌ (904; rule right,
  value from B1) B5 ✅ B6 ✅ B7 ✅ B8 ✅ B9 ✅ (objdump; board-checked 3.1a) B10 ✅.
Rule updates (objdump, one sample): -O0 if/else join nop only when nothing follows the if/else; pool pad
  only when code ends ≡ 2 mod 4 (the 2.5d "same parity" wording was a special case).
INVALID from now on: for(;;) 0x242, uart4_puts 0x30C, tick 0x2000_800C, Thread sp in main's loop 0x2000_FFF0
  (now 0x2000_FFE8), Reset 0x2B4 / Default 0x308, strings 0x338/0x340/0x350.
Weak point (again): sizes — over-counted a group ("loads/stores 12") instead of per instruction; layout
  side effects (join nop, pool alignment) not checked.
Strengths: all addresses (B5/B6), frame size, sp, stop-at-main and the instruction choice right; B6 spotted
  that tick.0 moves.

### 3.1a divide by zero, DIV_0_TRP 0 (board) — session 16, 2026-11-04
Predictions LOCKED (12 rows; addendum before launch: P3 0x2000_FFE0 → 0x2000_FFE8, P12 reason → adds
  Z 1 C 1 T 1, value 0x6100_0000). Score 7/12.
Result (raw): launch → $pc 0x16e; x/2i 0x254 = nop / b.n 0x254 → image 3.1a. break *0x24C (#2), break *0x254
  (#3), continue → (a) pc 0x24C: r2 0x64, r3 0, sp 0x2000_FFE8, [sp+4] 0, [sp+8] 0x64, [sp+12] 0x4040_0000,
  g_div_result 0. delete 2; continue → (b) pc 0x254: r3 0, g_div_result 0, CFSR 0, HFSR 0 (read twice;
  xPSR first missing → re-read at the same halt: pc 0x254, xPSR 0x4900_0000).
Learned: UDIV by zero with CCR.DIV_0_TRP 0 → result 0, no fault recorded (C2.4.260 p.972; E2.1.234 p.2023)
  → 3.0 F9 (N) confirmed on hardware. udiv r3, r2, r3 = Rd, Rn (dividend r2 0x64), Rm (divisor r3 0).
  main frame (image 3.1a): sp 0x2000_FFE8 (B9 confirmed); [sp+4] divisor, [sp+8] numerator, [sp+12] f 3.0f.
  xPSR 0x4900_0000 = Z (movs r3,#0 at 0x244, MOV imm T1 C2.4.119 p.741–742: carry kept) + C 0 (from puts'
  adds r7,#8 at 0x346: 0x2000_FFD8 + 8, no carry) + Q + T.
Grades: P1 ❌ P2 ❌ (operands swapped; listing 0x248/0x24A) P3 ✅ P4 ½ (divisor written at 0x246, not
  unwritten) P5 ✅ P6 ❌ (f at [r7,#12]; g_div_result address is in the literal pool 0x2C4) P7 ✅ P8 ½
  (UDIV p.841 → p.972) P9 ✅ P10 ✅ P11 ✅ P12 ❌ (movs at 0x240/0x244 skipped; adds gave Z/C 1 against the
  logged N Z C V 0; Q dropped — 4th time).
Weak points (new): register roles read from the C source instead of the objdump (which ldr loads which
  slot; Rn vs Rm); a stack slot named without checking the listing's [r7,#n] users. Repeats: Q lost; a
  decode already in the log (adds → NZCV 0) contradicted; value vs own reason (P3, P12 before addendum).
Strengths: P3 sp and P7–P11 fault-state rows right; addendum discipline (both value/reason mismatches fixed
  before launch).

### 3.1b composition accepted (rounds 1–2) + image 3.1a vector table — session 16, 2026-11-04
C1 Terminate while halted → Remove All / Remove All Terminated → new launch → stop-at-main 0x16E; CCR write
  + bp while halted (order irrelevant). C2 set {unsigned int}0xE000ED14 = 0x00000211 (0x201 measured 3.0 +
  DIV_0_TRP bit 4; D1.2.9 p.1471/1473/1474). C3 nothing before continue (evidence 2.5b-2 ISER1 GDB write took
  effect on continue; mechanism + extension to CCR = inferences). C4 slot 3 offset 0x0C (RCMTC p.80, RWPRT
  p.141), escalation D1.2.233 p.1838, .S line 19 plain .word Default_Handler; bp from objdump. C5 reads:
  pc, sp, lr, xpsr, x/8xw $sp, SHCSR, HFSR, CFSR (xPSR IPSR = only read that names the exception).
Round 1 errors fixed: two options in C1 (Restart or re-launch; bp Default or udiv); C3 cited 2.2a/2.2b
  read-backs (no continue) + unlabelled mechanism; C4 "weak alias" for slot 3 + arm-none-eabi-objdump not on
  PATH; C5 predicted values inside the composition, no command list.
objdump (image 3.1a): Default_Handler 0x31C (-t); .isr_vector word0 0x2001_0000, word1 0x2C9 (Reset_Handler
  0x2C8 — inference CONFIRMED), slots 2–7/11/12/14/16–54 0x31D, 8/9/10/13 = 0, slot 15 0x115, slot 55 0x14D.
  bp for 3.1b: break *0x31C (two sources agree).
UFSR DIVBYZERO = bit 9 (D1.2.270 p.1897) = CFSR bit 25.

### 3.1b DIV_0_TRP 1, UsageFault disabled → escalated HardFault — session 16, 2026-11-04
Predictions LOCKED (16 rows; addenda before the set command: Q3 reason → Thread sp 0xFFE8 (value kept
  0x2000_FF88), Q5 → 0x6900_0003 Z C Q T IPSR 3, Q8 → 0x2000_800C, Q14 reason → SYSTICKACT). Score 6/16.
Result (raw): launch → $pc 0x16e; x/2i 0x254 nop / b.n → image 3.1a. set {unsigned int}0xE000ED14 = 0x00000211
  → reads 0x211. break *0x31C (#2), continue → "Program received signal SIGSTOP, Stopped (signal).
  Default_Handler () at startup_m33.S:117" (not "Breakpoint 2"). pc 0x31C, sp 0x2000_FF80, lr 0xFFFF_FFE9,
  xPSR 0x4900_0003; frame R0 0xA, R1 0x2000_8014, R2 0x64, R3 0, R12 4, LR 0x335, RA 0x24C, RETPSR
  0x4910_0000; SHCSR 0x4, HFSR 0x4000_0000, CFSR 0x0200_0000.
Learned: CCR.DIV_0_TRP 1 → udiv by zero → UFSR.DIVBYZERO (CFSR bit 25, D1.2.270 p.1897); USGFAULTENA 0 →
  escalated to HardFault: HFSR.FORCED (p.1671), IPSR 3, SHCSR.HARDFAULTACT (p.1842). RA 0x24C = the faulting
  udiv (synchronous; = 1b.4a/1c). GDB CCR write at stop-at-main took effect on continue (C3 inference
  supported, one sample). Frame sp 0xFFE8 → 0xFF80 (26 words; FType 0). R1 0x2000_8014 = __bss_end__ of
  image 3.1a (Reset_Handler bss loop, .S 100–106; 2.5d 0x2000_8010 same mechanism — inference). LR slot 0x335
  = puts' bl uart4_putc return (puts 0x320 + 0x14 | 1): pop {r7,pc} does not restore LR (stale-LR inference
  CONFIRMED on two images: 0x321, 0x335). RETPSR 0x4910_0000 = Z + Q + T + SFPA (D1.2.221 p.1817).
SIGSTOP (inference): DEMCR 0x011007F0 (logged at main) = TRCENA 24 + SDME 20 + VC_HARDERR 10 … VC_MMERR 4
  (DDI0553 D1.2.37 p.1522–1527; VC_HARDERR p.1525 "Halting debug trap on HardFault enabled") → halt on
  HardFault entry by vector catch, reported by GDB as SIGSTOP. Closes "DEMCR bit 20 unverified" on paper
  (bit 20 = SDME, p.1523).
At the fault halt (ungraded): DEMCR 0xE000EDFC 0x011007F0 (VC_HARDERR 10 set; also VC_CHKERR 6, D1.2.37
  p.1527 "UsageFault … alignment check error or divide-by-zero trap"); DFSR 0xE000ED30 0 — even HALTED/BKPT
  0 although halted (W1C, D1.2.38 p.1529 → inference: the debugger clears DFSR after reading the halt cause)
  → DFSR cannot discriminate. SIGSTOP = vector catch stays an inference; test: rerun without the bp.
Grades: Q1 ✅ Q2 ✅ Q3 ❌ (value kept 0xFFF0-based; own reason gave 0xFF80) Q4 ✅ Q5 ❌ (C 1; 3.1a measured
  0x4900_0000 — repeat of P12) Q6 ❌ (0xA: puts mov r0,r3 last char '\n') Q7 ❌ (bss loop end) Q8 ❌ (dividend
  0x64 at the fault; 3.1a P1) Q9 ½ (divisor, not "zero before result") Q10 ½ (no source; logged R12 4) Q11 ❌
  (0x2E1 = image 2.5a) Q12 ❌ (udiv 0x24C; 0x24A = ldr) Q13 ❌ (copied 0x0910_0000; Z) Q14 ❌ (Thread → no
  SYSTICKACT) Q15 ✅ Q16 ✅.
Weak points (repeats, TOP): frame values copied from older images/contexts (Q3/Q11/Q13/Q14); the closest
  measured run (3.1a) not used (Q5/Q8); value vs own reason (Q3, Q12). New: R0/R1 not traced to the last
  writer (puts mov r0; Reset_Handler bss loop r1).
Strengths: fault-status rows Q15/Q16, CCR value, vector-slot reasoning, EXC_RETURN all right; addenda fixed
  three value/reason mismatches before the run.

### 3.1c composition accepted (rounds 1–3, option G) — session 16, 2026-11-04
Decision: USGFAULTENA by GDB now (G); C enables + barriers later (3.1d, proposed) — one new variable per step
  (2.5b → 2.5d pattern).
C1 void UsageFault_Handler(void) { for (;;) {} } after LP_FLEXCOMM4_IRQHandler; never returns (3.1b RA 0x24C =
  faulting udiv → a return would re-fault). C2 .S line 22 slot 6 → .word UsageFault_Handler; after line 125
  .weak / .thumb_set UsageFault_Handler, Default_Handler (absent strong def → slot 6 = Default_Handler; 2.5a
  precedent). C3 SHCSR RMW set {unsigned int}0xE000ED24 = {unsigned int}0xE000ED24 | 0x00040000 (D1.2.233
  p.1836 RMW recommendation; full 32-bit decode, pages p.1836–1842). C4 CCR 0x211 (GDB writes lost on reset;
  0x201 from 3.0 + bit 4). C5 at stop-at-main (pc from new objdump): C4 + C3 + bp at the UsageFault_Handler
  symbol → reads pc/sp/lr/xpsr/frame/SHCSR/HFSR/CFSR; arrival = pc = symbol, IPSR 6, USGFAULTACT bit 3 (p.1842);
  SIGSTOP possible (VC_CHKERR). C6 .c line 15 "3.1c: UsageFault handler and vector slot 6 override"; .S line 6
  " * 3.1c: UsageFault vector override to UsageFault_Handler*/".
Errors fixed: file names with "_3" suffix; C2 line 20 (= MemManage) → 22; C3 D1.2.33 → D1.2.233, no decode
  table, plain write against the p.1836 RMW recommendation, 9 wrong field pages, missing bits 6/9/20/21/31:22;
  C5 bp 0x31C (= Default_Handler) in round 2 after a correct placeholder in round 1; C6 no text (r1), "3.1b"
  label and a .S line outside the comment (r2); pre-computed line shifts (main 98 → 99) dropped.
Weak point (again): a later round reintroduced an error (C5 bp address) — build from the last accepted item.

### 3.1c build — session 16, 2026-11-04
Saved files diffed: .c +line 15 header (stray word "before" — comment only), line 62 comment .S 100–107,
  UsageFault_Handler lines 101–105 (blank 100, main 107); .S line 6 header (*/ moved), line 23 slot 6 →
  UsageFault_Handler, lines 128–130 weak pair. First send: .c unchanged + .S slot 6 not edited → caught by diff.
Build predictions LOCKED (D1–D8). Score 1/8.
Result (raw): Clean build 0/0; FLASH 904 B, RAMB 20 B; text 899 data 4 bss 16 dec 919. Symbols: uart4_putc 0xE0
  (0x34), main 0x168 (0x160), Reset_Handler 0x2C8, Default_Handler 0x31C, uart4_puts 0x320 (0x2C),
  UsageFault_Handler 0x34C (8 B: 0x34C push {r7}; 0x34E add r7,sp,#0; 0x350 nop; 0x352 b.n 0x350).
  .isr_vector: slot 6 (0x18) = 0x34D; all other slots as image 3.1a (word1 0x2C9, Default 0x31D, 15 0x115,
  55 0x14D). main listing identical to 3.1a except string literals 0x2A4 = 0x354, 0x2BC = 0x35C, 0x2C0 = 0x36C.
  .text ends 0x462; .data LMA 0x464 (899 + 1 pad + 4 = 904).
Grades: D1 ❌ (base 2.5d 871 instead of 3.1a 891) D2 ❌ (rule right, value from D1) D3 ½ (value; listed sub sp +
  padding instead of add r7 / nop / b.n) D4 ❌ D5 ❌ D6 ½ (reason contradicts own D4/D5) D7 ❌ D8 ❌.
Confirmed (objdump -h source\frdmmcxn236_m33_scratch.o): .text.uart4_putc / SysTick_Handler /
  LP_FLEXCOMM4_IRQHandler / main Algn 2**2; .text.uart4_puts / UsageFault_Handler Algn 2**1 →
  --sort-section=alignment: 4-aligned first, then 2-aligned, source order within a group (MEASURED).
  startup_m33.o .text Algn 2**2, size 0x58 → 0x2C8 + 0x58 = 0x320 = uart4_puts (MEASURED; order within the
  4-aligned group = link order: frdmmcxn236_m33_scratch.o then startup_m33.o).
  All image-3.1a main addresses remain valid (stop-at-main 0x16E, udiv 0x24C, for(;;) 0x254, Default 0x31C).
Weak points: base value from an older image (2.5d instead of 3.1a); source order assumed despite the
  --sort-section hint and the puts precedent; a reason that contradicts the same message's other rows.

### 3.1c USGFAULTENA 1 + own UsageFault_Handler (slot 6), DIV_0_TRP 1 — session 16, 2026-11-04
Predictions LOCKED (18 rows). Score 8/18.
Result (raw): launch → $pc 0x16e; x/2i 0x34C = push {r7} / add r7,sp,#0 → image 3.1c. set CCR 0x211; SHCSR RMW
  |0x00040000 → SHCSR 0x0004_0000, CCR 0x211. break *0x34C (#2), continue → "Program received signal SIGSTOP,
  Stopped (signal). UsageFault_Handler () at …:102". pc 0x34C, sp 0x2000_FF80, lr 0xFFFF_FFE9, xPSR 0x4900_0006;
  frame R0 0xA, R1 0x2000_8014, R2 0x64, R3 0, R12 4, LR 0x335, RA 0x24C, RETPSR 0x4910_0000; SHCSR 0x0004_0008,
  HFSR 0, CFSR 0x0200_0000.
Learned: USGFAULTENA 1 → the divide-by-zero UsageFault is taken directly as exception 6 (IPSR 6, SHCSR
  USGFAULTACT bit 3 p.1842, HFSR 0 — no FORCED) → both sides of D1.2.233 p.1838 measured (3.1b vs 3.1c). Frame,
  sp, EXC_RETURN identical to 3.1b (same udiv, same Thread state); only exception number + SHCSR/HFSR differ.
  CFSR DIVBYZERO set either way. SIGSTOP again at the handler's first instruction (2nd sample; DEMCR VC_CHKERR
  bit 6 — inference: vector catch, not our bp).
Grades: E1 ✅ E2 ✅ E3 ❌ ((B); VC_CHKERR note) E4 ✅ E5 ❌ E6 ✅ E7 ❌ (Z) E8 ❌ E9 ❌ E10 ❌ E11 ✅ E12 ✅
  E13 ❌ E14 ❌ E15 ❌ E16 ❌ (USGFAULTENA from own E1 dropped) E17 ✅ E18 ✅.
Weak point (TOP, repeat of session 15): "unchanged from 3.1b" rows copied the 3.1b PREDICTIONS, not the 3.1b
  Result line (E5, E8, E9, E10, E13, E14, E15). Fix: keep the previous Result (raw) line open while predicting.
  Also: own earlier row (E1) not carried into a later row (E16).
Strengths: method ("does this step's change touch this value?") right; exception-number, HFSR and CFSR rows
  right; RMW write + read-back as composed.
Board: Terminate while halted at 0x34C → confirm at session-17 intake.

### Session 16 pause / end (2026-11-04 18:48)
Session ended at the user's request after 3.1c was graded; 3.1d (proposed) not yet issued. Image 3.1c in flash
  (sources = saved 3.1c .c/.S; .c line 15 may still carry the stray word "before" — comment only). GDB-only
  state: none. Terminate after the 3.1c run not confirmed in a paste → confirm at session 17 intake. Bench Map v66.

### Session 16 summary (2026-11-03 / 2026-11-04)
Scores: 3.0 5/7; 3.1a build 8/10, board 7/12; 3.1b 6/16; 3.1c build 1/8, board 8/18. Compositions: 3.1a (2 rounds),
  3.1b (2 rounds), 3.1c (3 rounds + 2 saved-file rounds).
Closed: Z/C at main's for(;;) (puts' flag-setting -O0 epilogue adds); puts −16 frame + stale-LR mechanism
  (pop {r7,pc} keeps LR; 0x321 / 0x335); divide by zero three ways (trap off → 0; trap + UsageFault disabled →
  HardFault FORCED; trap + enabled → own UsageFault handler, no FORCED); section placement by
  --sort-section=alignment (measured from .o Algn); DEMCR bit 20 = SDME; Terminate after session-15/16-pause runs.
New facts: CCR 0x201 / SHCSR 0 / CFSR 0 / HFSR 0 / MSPLIM_S 0 / PSPLIM_S 0 at stop-at-main; MMFAR = BFAR = 0x2000_FFFC
  (not valid); image 3.1a/3.1c addresses (stop-at-main 0x16E, udiv 0x24C, for(;;) 0x254, Thread sp 0x2000_FFE8,
  Reset 0x2C8, Default 0x31C, puts 0x320, UsageFault_Handler 0x34C, tick.0 0x2000_8010, g_div_result 0x2000_800C);
  synchronous-fault frame from main (sp 0xFF80, RA 0x24C, RETPSR 0x4910_0000, R1 = __bss_end__ 0x2000_8014).
Tutor corrections: 3 — Z/C from puts' final cmp (session 15); CCR bit 9 page p.1472 → p.1473; 3.1a pre-save line
  table off by +1 from main on.
Weak points this session: copying own earlier PREDICTIONS instead of RESULTS (TOP, 3.1c 7 rows); values from older
  images/contexts (2.5d/2.5a frame values, 0xFFF0 sp, 871 text base); register roles from C source instead of
  objdump (operand order); boot ROM ignored for at-main values (3.0); sizes by group instead of per instruction;
  a later composition round reintroduced a fixed error; value vs own reason (caught by Lock flags 7×).
Strengths: fault-status rows (CFSR/HFSR) right every run; addenda discipline; method "does this change touch this
  value?" stated correctly; vector-slot reasoning; composition loops converged and caught 3 file errors by diff.

### Session-17 intake (2026-11-04)
learning_log.md (2,247 lines) ends with the session-16 summary exactly once; 137 "###" headings, no
  duplicates; all session-16 blocks (3.0, 3.1a build/board, 3.1b composition/run, 3.1c composition/
  build/run, pause 18:48, summary) present once. No session16_log_blocks.md attached → no diff.
Saved sources = continuation prompt: .c 174 lines (UsageFault_Handler 101–105, division 168–170,
  for(;;) 172–173); line 15 "3.1c: UsageFault handler and vector slot 6 override" — stray "before"
  gone (comment only; image unaffected per session-13 rebuild check). .S 130 lines (slot 6 line 23,
  weak pair 129–130). .ld unchanged (45 lines by wc; last line has no newline).
Bench Map v68 (intake).

### Session 17 pause (2026-11-04 22:54)
Paused after intake, before the launch check. Image 3.1c in flash; GDB-only state none; no rebuild.
  Open: Terminate after the 3.1c run (halted at 0x34C) + unplug — not answered. Resume: answer that →
  new launch → print/x $pc (expect 0x16e) + x/2i 0x34C → 3.1d composition. Bench Map v69.

### Session 17 launch check (2026-11-05 11:24)
New launch → print/x $pc 0x16e; x/2i 0x34C = push {r7} / add r7,sp,#0 <UsageFault_Handler> → image
  3.1c in flash, no rebuild. Probe connected normally. (Terminate after the 3.1c run never explicitly
  answered; no leftover-process symptoms.) Bench Map v70.
3.1d composition issued (C1 macros CCR 0xE000ED14 D1.2.9 p.1471/1474, SHCSR 0xE000ED24 D1.2.233
  p.1836/1837, CFSR 0xE000ED28 D1.2.11 p.1477 · C2 CCR write form · C3 SHCSR RMW · C4 placement +
  barriers RSJQJ/RNPDJ p.222, RWQQB p.268 · C5 g_cfsr in handler · C6 UART from handler yes/no ·
  C7 header lines · C8 run plan). Addresses re-checked in DDI0553 (pdftotext) this session; also
  HFSR 0xE000ED2C (D1.2.123 p.1671).

### 3.1d composition round 1 — session 17, 2026-11-05
Accepted: C2 plain write 0x211 (3.0 measured 0x201 + DIV_0_TRP bit 4, D1.2.9 p.1474); C3 SHCSR RMW
  |USGFAULTENA (D1.2.233 p.1836/1837); C4 order CCR → SHCSR and DSB + ISB; C5 name/type/volatile;
  C6 "no UART from the handler"; C7 .S unchanged.
Fix in round 2: C1 exact line (53 = SYST_CSR_COUNTFLAG); C4 exact line (151 Hello vs 162–166 if/else —
  two places), RNPDJ misattributed (completion = RSJQJ/RBMLS; RNPDJ = DSB alone not enough; CSE:
  RHMNM p.222, RWQQB p.268), "pipeline flush" unsourced; C5 reason "asynchronous" (UsageFault here is
  synchronous, RA = udiv 0x24C) + "read by main" (handler never returns), W1C clear destroys the C8
  cross-check, raw 0xE000ED28u instead of REG32(SCB_CFSR), no exact lines; C6 hedged reason, "printed
  by main" impossible, UsageFault "low-priority" (SHPR1 = 0 at main → priority 0); C7 new header line =
  16 (not 15), no leading "* "; C8 bp at main's for(;;) unreachable, bp-1 "yes" contradicts its own
  reason, CCR/SHCSR/HFSR/CFSR reads missing, discrimination test identical for vector catch and bp,
  two termination options.

### 3.1d composition round 2 — session 17, 2026-11-05
Accepted: C4 placement after the if/else (before pre-edit line 168; up-to-else addresses unchanged),
  DSB + ISB reasoning; C5 complete (volatile reason: synchronous fault, handler parks, GDB the only
  reader; no CFSR clear — C8 cross-check; REG32(SCB_CFSR); global after g_div_result); C8 two bps
  (handler entry + handler park loop), stop-line discriminator, HFSR 0xE000ED2C (D1.2.123 p.1671),
  terminate at park bp.
Fix in round 3: C1 line 58 = NVIC_ISPR1 (not ISPR1_IRQ39_SET, which is 60) → start line wrong; mask
  macros accepted in round 1 dropped; C2 "nothing before writes CCR" clause owed; C4 RBMLS p.268 (not
  p.222), "fetch" not in RWQQB; C6 "UART unreachable" false; C7 still line 15 with "* "; C8 no halt-1
  reads, x/8xw $sp at the park loop ignores the handler prologue push.

### 3.1d composition accepted (rounds 1–3) — session 17, 2026-11-05
C1 macros after NVIC_ISPR1_IRQ39_SET (pre-edit 62–66 → post-edit 63–67; blank line kept before the IRQ-
  counter global): SCB_CCR 0xE000ED14u (D1.2.9 p.1471), SCB_CCR_DIV_0_TRP_Msk (1u << 4) (p.1474),
  SCB_SHCSR 0xE000ED24u (D1.2.233 p.1836), SCB_SHCSR_USGFAULTENA_Msk (1u << 18) (p.1837), SCB_CFSR
  0xE000ED28u (D1.2.11 p.1477). C2 REG32(SCB_CCR) = 0x00000211u (3.0 measured 0x201 + bit 4; no writer
  before the line: main, SysTick_Handler, LP_FLEXCOMM4_IRQHandler). C3 SHCSR RMW |USGFAULTENA (p.1836).
  C4 after the if/else, before the division: CCR → SHCSR → dsb (RSJQJ p.222 / RBMLS p.268) → isb
  (RNPDJ / RHMNM p.222, RWQQB p.268 quoted verbatim). C5 volatile uint32_t g_cfsr after g_div_result;
  handler: g_cfsr = REG32(SCB_CFSR); for(;;) — no W1C clear (keeps the live-CFSR cross-check). C6 no
  UART from the handler (one variable per step; GDB read is the evidence). C7 .c header line 16
  "3.1d: Self-setup of DIV_0_TRP and USGFAULTENA in C with CFSR logging", */ → 17; .S unchanged.
  C8 no GDB writes; halt 1 bp handler entry: GDB stop line (SIGSTOP vs Breakpoint N), pc, g_cfsr;
  halt 2 bp handler park loop: pc, sp, lr, xpsr, g_cfsr, x/1wx CCR / SHCSR / HFSR 0xE000ED2C / CFSR,
  x/8xw $sp+<prologue bytes from the new objdump>; Terminate while halted at halt 2.
Errors fixed across rounds: approximate lines ("around"); line 58 content; masks dropped then
  restored; "=" in #define; #defne; RNPDJ cited for completion; RBMLS page p.222 → p.268;
  "fetch"/"pipeline flush" unsourced; "asynchronous" UsageFault; "read by main"; W1C clear vs C8
  cross-check; raw address instead of macro; UsageFault "low-priority" (SHPR1 0); "UART
  unreachable"; header line 15 / "* " / embedded */ (would close the header early → stray */ =
  compile error); bp at main's unreachable for(;;); invented "info stop"; non-leaf "sub sp,#8" for a
  leaf handler.
Weak points (repeats): fixes not carried to the next round (C7 ×2); accepted parts dropped (C1 masks);
  line content from memory; invented command; function shape copied from another function.

### 3.1d saved-file checks (rounds 1–3) — session 17, 2026-11-05
Round 1: only C1 (macros 63–67) + C7 (header 16) applied; C2–C5 missing; line 66 comment source "…".
Round 2: g_cfsr line 74, handler store line 111, CCR write line 177 added; C3 SHCSR RMW + C4 dsb/isb
  still missing (without C3 → HardFault escalation like 3.1b, both handler bps never hit); no line table.
Round 3 ACCEPTED (189 lines): header 16 (*/ 17); macros 63–67; IRQ-counter global 69; g_init 71, g_zero
  72, g_div_result 73, g_cfsr 74; putc 76–83; puts 85–91; SysTick 93–99; LP_FLEXCOMM4 104–107;
  UsageFault_Handler 109–114 (store 111, for(;;) 112–113); main 116–189: IRQ-39 dsb/isb 168/169 (old
  comment "RSJQJ/RNPDJ p.222" kept), if/else 171–175, CCR = 0x211 line 177 (comment: D1.2.9 p.1474,
  3.0 0x201 + DIV_0_TRP, no other writer), SHCSR RMW 178, dsb 179, isb 180, blank 181–182, numerator
  183, divisor 184, division 185, for(;;) 187–188. .S unchanged (130 lines). Comment-only fix owed:
  line 179 "RBMLS p.222" → "p.268" (reintroduced; fix status before the build NOT confirmed).
  User line table too coarse ("177 to 189") → full table by tutor.
Weak point (repeat of 3.1c): saved file ≠ composition for 2 rounds — diff caught it before any build.

### 3.1d build — session 17, 2026-11-05
Predictions LOCKED (D1–D12; addenda D8 → 26 (from 24), D9 → 0x330, D10 wording "up"). Score 5/12.
Result (raw): Clean 0/0; FLASH 956 B, RAMB 24 B; text 951 data 4 bss 20 dec 975. .text 0x2D7 @0xE0
  (ends 0x3B6); .data LMA 0x3B8. .o Algn: putc / SysTick / LP_FLEXCOMM4 / UsageFault_Handler / main
  2**2, puts 2**1 (UsageFault_Handler 0x18 now 4-aligned: literal pool). Symbols: uart4_putc 0xE0,
  SysTick_Handler 0x114, UsageFault_Handler 0x168 (0x18), main 0x180 (0x184), Reset_Handler 0x304,
  Default_Handler 0x358, uart4_puts 0x35C (0x2C); g_div_result 0x2000_800C, g_cfsr 0x2000_8010,
  tick.0 0x2000_8014, __bss_end__ 0x2000_8018. UsageFault_Handler: 0x168 push {r7}; 0x16A add
  r7,sp,#0; 0x16C ldr r3,[pc,#8] (0x178 = 0xE000_ED28); 0x16E ldr r3,[r3]; 0x170 ldr r2,[pc,#8]
  (0x17C = 0x2000_8010); 0x172 str r3,[r2]; 0x174 nop; 0x176 b.n 0x174. main: prologue 0x180–0x184
  (push {r7,lr}; sub sp,#16; add r7) → stop-at-main 0x186; IRQ-39 dsb/isb 0x23A/0x23E; if/else
  0x242–0x254 (ldr counter 0x242, cmp 0x246, beq 0x248, then 0x24A/0x24C, b.n 0x250, else
  0x252/0x254); CCR 0x258 ldr / 0x25A movw r2,#0x211 / 0x25E str; SHCSR 0x260 ldr r3 / 0x262 ldr
  [r3] / 0x264 ldr r2 (same literal 0x2FC) / 0x266 orr.w #0x40000 / 0x26A str; dsb 0x26C; isb 0x270;
  0x274 movs r3,#100; 0x276 str [r7,#8]; 0x278 movs r3,#0; 0x27A str [r7,#4]; 0x27C ldr r2,[r7,#8];
  0x27E ldr r3,[r7,#4]; 0x280 udiv r3,r2,r3; 0x284 ldr r2 ← 0x300 (0x2000_800C); 0x286 str; for(;;)
  nop 0x288 / b.n 0x28A; literals 0x28C–0x300 (30 words; strings 0x388 / 0x390 / 0x3A0). .isr_vector:
  word0 0x2001_0000, word1 0x305, slots 2–5/7/11/12/14/16–54 0x359, slot 6 0x169, 8/9/10/13 0,
  slot 15 0x115, slot 55 0x14D.
Learned: a literal pool makes a function 4-aligned → sorted into the 4-aligned group in SOURCE
  order (handler before main, main +0x18 → stop-at-main 0x186). New main code 28 B before the udiv
  (+8 B literals after the code). Function-local static (tick.0) moves after the new global again.
Grades: D1 ✅ D2 ✅ D3 ✅ D4 ✅ D5 ✅ D6 ❌ (base = 3.1c tick.0 0x8010, not g_div_result 0x800C)
  D7 ❌ (alignment group right; source order within the group ignored) D8 ❌ (addendum 24 → 26:
  ldr-literal 4 B, sub sp,#8, nop missing) D9 ❌ (0x174) D10 ❌ (0x280: main +0x18, code 28 not 36)
  D11 ❌ (0x186) D12 ❌ (0x169).
Weak points: placement within the group (logged source-order rule); non-leaf shape for a leaf
  (repeat of C8 same hour); logged size rules overridden (ldr-literal 2 B, for(;;) nop + b.n);
  addendum made a right value wrong; base from another symbol of the older image (D6).
Strengths: D1 per-function split (main +36 = 28 code + 8 literals, handler +16) and D4 pad exact.

### 3.1d DIV_0_TRP + USGFAULTENA set in C, handler records CFSR — session 17, 2026-11-05
Predictions LOCKED (H1–H20; addenda 1–5: H2 → 0x168, H4 → 0x174, H19 → 0x280; others reasons only).
  Score 11.5/20.
Result (raw): launch → $pc 0x186; x/2i 0x168 push {r7} / add r7,sp,#0 → image 3.1d. break *0x168
  (#2, line 110), break *0x174 (#3, line 112). continue → "Program received signal SIGSTOP, Stopped
  (signal). UsageFault_Handler () at …:110": pc 0x168, g_cfsr 0. continue → "Breakpoint 3,
  UsageFault_Handler () at …:112": pc 0x174, sp 0x2000_FF7C, lr 0xFFFF_FFE9, xPSR 0x4900_0006,
  g_cfsr 0x0200_0000, CCR 0x211, SHCSR 0x0004_0008, HFSR 0, CFSR 0x0200_0000; x/8xw $sp+4
  @0x2000_FF80: R0 0xA, R1 0x2000_8018, R2 0x64, R3 0, R12 4, LR 0x371, RA 0x280, RETPSR 0x4910_0000.
Learned: firmware-only setup (CCR movw 0x211 + SHCSR RMW + DSB + ISB, 0x258–0x273) arms the trap for
  the udiv at 0x280 → UsageFault taken directly (IPSR 6, HFSR 0, USGFAULTACT); g_cfsr = live CFSR =
  DIVBYZERO → cause recorded without GDB. Entry sp 0x2000_FF80 (Thread 0xFFE8 − 0x68) → 0x2000_FF7C
  after push {r7}. R1 = __bss_end__ 0x2000_8018 (3rd image), LR slot 0x371 = puts 0x35C + 0x14 | 1
  (stale LR, 3rd image), R2 = dividend 0x64 (ldr r2,[r7,#8] 0x27C), R3 0 (ldr r3,[r7,#4] 0x27E).
  xPSR/RETPSR Z from movs r3,#0 0x278, C 0 from puts' adds. Same run: SIGSTOP at 0x168 (bp #2 there)
  vs "Breakpoint 3" at 0x174.
Grades: H1 ❌ (logged SIGSTOP-with-bp note; = 3.1c E3) H2 ✅ H3 ✅ H4 ✅ H5 ❌ (0xFF88 = old base; prologue)
  H6 ✅ H7 ❌ (Z) H8 ✅ H9 ✅ H10 ✅ H11 ✅ H12 ✅ H13 ❌ (0xA puts mov r0) H14 ❌ (__bss_end__, in own H3)
  H15 ❌ (dividend; Lock flag) H16 ✅ H17 ½ (no source) H18 ❌ (0x371; 0x2E1 = image 2.5a) H19 ✅ H20 ❌ (Z).
Weak points (repeats): logged notes not used (H1); handler sp vs prologue + old base (H5); Z dropped
  (H7/H20, 3rd step); last writers not traced, even when in own row / Lock flag (H13–H15/H18);
  addenda reword reasons without re-deriving values (5 addenda, 1 value change; Lock flagged
  invented listing lines "movs r0,#100" / "movs r1,#0", a wrong IPSR citation D1.2.19 p.1481 =
  Dn FP register, and a measurement "in this build" before any run).
Strengths: fault-status rows H8–H12 all right; H19/H16 from the listing; composition → saved-file
  → build loop caught 4 missing pieces before any run.
Board: Terminate at 0x174 confirmed; board unplugged. User chose 3.1e (time: no edit/build) over 3.2.

### 3.1e composition accepted (rounds 1–2) — session 17, 2026-11-05
E1 bp only *0x174 (safety net vs a free run into the park loop; none at 0x168). E2 at the first halt:
  stop line, pc, DEMCR 0xE000EDFC (D1.2.37 p.1522; VC_CHKERR bit 6 p.1527 — read live, not from older
  sessions), g_cfsr. E3 (1) 0x168 + SIGSTOP + VC_CHKERR set → SUPPORTS vector catch (debugger-set DEMCR
  still inference); (2) 0x174 + "Breakpoint N" → the 0x168 halt needed our bp (mechanism open);
  (3) other pc / no halt → unexpected; recover Debug view → Thread #1 → Suspend. E4 Terminate at the
  first halt. Round-1 fixes: DEMCR read added, third case added, "proving" → "supports".

### 3.1e run 1 VOID — session 17, 2026-11-05
Predictions LOCKED: P1 (B), P2 0x174, P3 DEMCR 0x011007F0 (addendum reason: debugger sets it at
  connect the same way every session; no log entry named), P4 g_cfsr 0x0200_0000.
Launch → "Temporary breakpoint 3, main ()"; break *0x174 → "Note: breakpoint 1 also set at pc 0x174"
  → Breakpoint 4. continue → SIGSTOP at 0x168. info breakpoints: #1 0x174 (keep, line 112), #2 0x168
  (keep, line 110), #4 0x174 → the 3.1d console bps (*0x168, *0x174) survived Terminate + unplug + new
  launch and were re-installed before stop-at-main. Halt at 0x168 had bp #2 under it = 3.1d
  conditions → not graded, E3 not applied. (Whether Remove All was pressed before run 1: asked; reply
  "Yes" ambiguous — confirm at session-18 intake.)
Tooling (new): console `break` bps persist across debug launches in this IDE → Breakpoints view
  Remove All (view empty) before EVERY launch; stop-at-main must say "Temporary breakpoint 1"; run
  `info breakpoints` after setting bps.

### 3.1e handler-entry halt without a bp at 0x168 — session 17, 2026-11-05
Score 0.5/4 (run 2).
Result (raw, run 2): launch → "Temporary breakpoint 1, main ()", $pc 0x186; x/2i 0x168 push {r7} / add
  r7,sp,#0 → image 3.1d. break *0x174 → Breakpoint 2 (no "also set" note); info breakpoints: only #2
  0x174. continue → "Program received signal SIGSTOP, Stopped (signal). UsageFault_Handler () at
  …:110": pc 0x168, DEMCR 0xE000EDFC 0x011007F0, g_cfsr 0.
Learned: with NO bp at the handler entry, the divide-by-zero UsageFault still halts at 0x168 →
  E3 case 1: vector catch (DEMCR VC_CHKERR bit 6, D1.2.37 p.1527 "UsageFault … divide-by-zero trap"),
  read live this session. With 3.1d (same image: "Breakpoint 3" at 0x174): our bps report
  "Breakpoint N"; vector-caught fault entries report SIGSTOP whether or not a bp is there. Closes the
  session-16 open question (supported; debugger as DEMCR writer still an inference). g_cfsr 0 at
  0x168 (store at 0x172 not run). Practical: a bp at a fault handler's first instruction is redundant
  while VC_* is set.
Grades: P1 ❌ (logged FAULT HALTS note, 3rd miss) P2 ❌ (0x168) P3 ½ (value right; no log entry) P4 ❌
  (method right — derived from P2; P2 wrong).
Board: Terminate (halted at 0x168) confirmed; board unplugged. GDB-only state none. Image 3.1d in flash.

### Session 17 end (2026-11-05 15:07)
Session ended at the user's request after 3.1e; continuation in a new chat (session 18). Image 3.1d in
  flash (sources = saved 3.1d .c 189 lines / .S 130 / .ld 45 by wc). GDB-only state none. Breakpoints
  view: user to confirm EMPTY before the next launch. Next: session-18 intake → 3.2 UNALIGN_TRP
  (proposed). Bench Map v76.

### Session 17 summary (2026-11-04 / 2026-11-05)
Scores: 3.1d build 5/12, board 11.5/20; 3.1e 0.5/4 (run 1 void). Compositions: 3.1d (3 rounds + 3
  saved-file rounds), 3.1e (2 rounds).
Closed: firmware-only fault setup (CCR 0x211 by movw + SHCSR RMW + DSB + ISB) arms the divide-by-zero
  trap; UsageFault handler records CFSR in g_cfsr (= live CFSR 0x0200_0000); SIGSTOP at fault-handler
  entry = debugger vector catch (no bp there, DEMCR 0x011007F0 VC_CHKERR live) vs "Breakpoint N" for
  our bps; placement of a function with literals (4-aligned group, source order → handler 0x168
  before main 0x180, stop-at-main 0x186); console bps persist across launches; Terminate after the
  3.1c, 3.1d and 3.1e runs.
New facts: image 3.1d map (see 3.1d build block); entry sp from main 0x2000_FF80, 0x2000_FF7C after the
  handler's push {r7}; frame R1 = __bss_end__ 0x2000_8018, LR slot 0x371, RA 0x280, RETPSR 0x4910_0000.
Tutor corrections: none on chip facts. Process note: in round 1 of C4 the tutor table wrote
  "RSJQJ / RBMLS p.268" ambiguously; the user's comment then read "RBMLS p.222" — the page is p.268.
Weak points this session: logged notes not used before predicting (FAULT HALTS — 3 misses: 3.1c E3,
  3.1d H1, 3.1e P1/P2); placement inside an alignment group (source order); leaf vs non-leaf function
  shape (sub sp,#8 — composition C8, build D8/D9, board H5); logged size rules overridden (ldr-literal
  2 B, for(;;) nop + b.n); addenda that reword reasons without re-deriving values (H: 5 addenda, 1 value
  change; D8: right value made wrong); register last writers (R0 puts mov r0, R1 __bss_end__, R2
  dividend, LR stale puts bl return); Z flag (H7/H20); base values from another symbol / older image
  (D6, H5, H18 0x2E1); saved file ≠ composition (2 rounds); fixes not carried between rounds; invented
  command ("info stop") and invented listing lines.
Strengths: fault-status rows right in every run (H8–H12); D1 size split and D4 pad exact; listing
  reading once started (H16, H19); composition/diff loop caught every file error before a build;
  3.1e decision rule with three cases.

### Session-18 intake (2026-11-05 15:18)
learning_log.md (2,463 lines) ends with the session-17 summary exactly once (line 2440); 151 "###"
  headings, no duplicates. session17_log_blocks.md (215 lines, 14 blocks): each heading once in the
  log; whole file present verbatim once, contiguous at the end → nothing missing or duplicated.
Saved sources = continuation prompt: .c 189 lines (UsageFault_Handler 109–114, store 111; CCR 177,
  SHCSR RMW 178, dsb 179, isb 180; division 183–185); line 179 reads "RSJQJ p.222 / RBMLS p.268"
  (fix present in the saved file). Line 168 comment "RSJQJ/RNPDJ p.222" still cosmetic. .S 130 lines
  (slot 6 line 23, weak pair 129–130). .ld 45 lines (wc). Bench Map v77 (intake; stale "3.1c done →
  3.1d" headline corrected to "3.1a–3.1e done → 3.2").

### Session 18 launch check (2026-11-05 18:45)
Remove All → new launch → "Temporary breakpoint 1, main () at …:118"; info breakpoints: none;
  print/x $pc → $2 = 0x186; x/2i 0x168 = push {r7} / add r7,sp,#0 <UsageFault_Handler> → image 3.1d
  in flash, no rebuild. No persisted console bps this launch.
3.2 composition issued (C0 trap-off first? · C1 target address · C2 access instruction/size ·
  C3 fate of the division · C4 CCR value + decode · C5 SHCSR · C6 handler record · C7 macros/header
  lines · C8 run plan). Sources opened (pdftotext): CCR UNALIGN_TRP bit 3 D1.2.9 p.1474 (0 = LDR/LDRH/
  STR/STRH unaligned permitted; 1 = any unaligned → UNALIGNED UsageFault; multiples/exclusives always
  fault; banked); B7.6 p.228 RCWNS (always-faulting list incl. LDRD/STRD, LDM/STM, PUSH/POP multiple,
  exclusives), RMHCM (UNALIGN_TRP 1: LDR{T}/STR{T} non-word, LDR{S}H{T}/STRH{T}/TBH non-halfword),
  RWCVX (Device accesses always aligned), RRNDS (synchronous UNALIGNED UsageFault), RLPVP; UFSR
  UNALIGNED bit 8 D1.2.270 p.1898 (sticky, Warm reset 0); DEMCR VC_CHKERR bit 6 D1.2.37 p.1527
  ("alignment check error or divide-by-zero trap"). Bench Map v78.

### Session 18 intake answers (2026-11-05 18:49)
(a) Breakpoints view empty at launch → closed. (b) Remove All WAS pressed before 3.1e run 1 → the IDE
  view's Remove All did not clear the persisted console bps in session 17; in session 18 3.1e run 2's
  console bp *0x174 did not persist — cause of the difference open; rule kept: info breakpoints after
  every launch. (c) saved .c line 179 = "RSJQJ p.222 / RBMLS p.268"; timing vs the 3.1d build unknown
  — comment only, image unaffected (session-13 rebuild check) → closed. (d) first GDB print is always
  $2 → $1 consumed at launch by the IDE (inference) → normal. Bench Map v79.

### 3.2 composition round 1 — session 18, 2026-11-05
Scope fixed: this composition = 3.2a (trap OFF); 3.2b composed after 3.2a results.
Accepted: C0 trap-OFF first (3.2a); C2 word LOAD via a volatile pointer; C3 remove the division;
  C5 keep SHCSR RMW |USGFAULTENA; C6 keep g_cfsr = REG32(SCB_CFSR); C7 .S unchanged.
Fix in round 2: C0 proof method + base = 3.0 measured CCR (not reset value); C1 "such as/e.g." (one
  address), &g_zero + 1 = +4 bytes (pointer arithmetic on unsigned int *) → aligned, 0x2000_8005 =
  inside g_lpflexcomm4_irq_count (g_zero 0x2000_8008 in 3.1d), memory type from DDI0553 B8.1 RFQSD
  p.265 not RM Table 16/17, word load spans two objects — name both + why known, address relative to
  a symbol; C2 g_bss_array not in our file, result in a local, "guarantees" unsourced → objdump; C3
  "or comment out" (delete rule) + fate of numerator / divisor / g_div_result / DIV_0_TRP mask; C4 two
  values → one for 3.2a; C5 citation + dsb/isb; C6 predicted values in a composition; C7 no line
  numbers/text, line 168 undecided; C8 bp at handler entry unreachable in 3.2a (free run → Suspend
  timeout risk) and redundant under vector catch → bp after the load + exact read list.
New source opened: DDI0553 B8.1 RFQSD p.265 (default system address map). Bench Map v80.

### 3.2a composition round 2 — session 18, 2026-11-05
Accepted: C1 address = byte offset +1 from g_zero via a byte-pointer cast, Normal memory per B8.1
  RFQSD p.265 (MPU never enabled by our code); C2 g_unaligned_val = *(volatile uint32_t *)((uint8_t *)
  &g_zero + 1) into a new volatile global; C3 delete numerator / divisor / g_div_result /
  SCB_CCR_DIV_0_TRP_Msk; C4 keep the CCR write at 0x201 (3.0 measured base); C5 SHCSR RMW + dsb/isb
  kept; C6 unchanged; C7 UNALIGN_TRP mask in the old mask slot (unused), .S unchanged; C8 no handler
  bp, bp at main's for(;;) (objdump address), Terminate.
Fix in round 3: g_zero ≠ 0 at the load (main line 118 at the time: g_zero = g_init) and next object ≠
  g_div_result (deleted in C3); predicted value ("0") in a composition; .S bss loop = lines 101–107;
  "D1.2.8" = BPIALL; "e.g." in a decision; C2 placement; C4 new CCR comment; C7 header must ADD a line
  (keep 16), no embedded "/*" (-Wall → -Wcomment), */ moves; line 168 = 2.5d IRQ-39 dsb; "around line
  74"; per-item line table; C8 x/ type unsigned char, dump from &g_zero covering both objects, print/x.
Sources opened: B7.5 Endianness p.225 (ICTVV figure); AIRCR ENDIANNESS bit 15 D1.2.3 p.1459 (0 =
  little-endian; AIRCR 0xFA05_0000 measured log 2.1). Bench Map v81.

### 3.2a composition accepted (rounds 1–3) — session 18, 2026-11-05
C0 trap OFF first; proof = new volatile global g_unaligned_val read by GDB; base = 3.0 measured CCR
  0x201. C1 (volatile uint32_t *)((unsigned char *)&g_zero + 1); Normal memory per DDI0553 B8.1 RFQSD
  p.265 (MPU never enabled by our code), RLPVP p.228; the 4 bytes span g_zero (last writer main
  g_zero = g_init) and g_cfsr (next .bss object by definition order after deleting g_div_result;
  zeroed by Reset_Handler .S 101–107; written only by UsageFault_Handler). C2 load in main after the
  isb, replacing the division (objdump to confirm one LDR). C3 delete numerator, divisor,
  g_div_result, SCB_CCR_DIV_0_TRP_Msk. C4 keep the CCR write = 0x201 (bits 9, 0 RES1 p.1473/p.1474;
  DIV_0_TRP 0; UNALIGN_TRP 0). C5 SHCSR RMW + dsb/isb kept (D1.2.233 p.1837–1838; 3.1b/3.1c). C6
  handler unchanged. C7 header line 17 + */ 18; SCB_CCR_UNALIGN_TRP_Msk (1u << 3) (unused);
  g_unaligned_val after g_cfsr; line 168 untouched; .S unchanged. C8 no handler bp; bp at main's
  for(;;), info breakpoints, pc, print/x g_unaligned_val, x/1wx CCR 0xE000ED14 / CFSR 0xE000ED28 /
  HFSR 0xE000ED2C, x/16xb &g_zero; Terminate at the halt.
Errors fixed across rounds: "such as/e.g." (one address); &g_zero + 1 = +4 bytes on unsigned int *;
  0x2000_8005 inside the IRQ counter; RM memory map for an Arm memory type; undefined g_bss_array;
  "or comment out"; two CCR values; bp at an unreachable handler and redundant under vector catch;
  g_zero assumed 0; g_div_result as neighbour after deleting it; predicted value in a composition;
  .S lines 100–106; "D1.2.8" = BPIALL; header text with embedded */ and /*; line 168 relabelled
  "3.2a"; "around"/"~" line numbers; post-edit table ignoring the +1 header shift.
Weak points (repeats): embedded comment delimiters (3.1d C7 repeat); line numbers not re-derived
  after an insertion; predicted values inside a composition.

### 3.2a saved-file checks (rounds 1–2) — session 18, 2026-11-05
Round 1 (185 lines): load line (C2) missing — the image would not perform the access; stray ")" in
  the CCR comment; no line table; blank line moved (cosmetic).
Round 2 ACCEPTED (187 lines): header 17 "3.2a: Unaligned word access with UNALIGN_TRP = 0", */ 18;
  SCB_CCR 64, SCB_CCR_UNALIGN_TRP_Msk (1u << 3) 65, SCB_SHCSR 66, USGFAULTENA mask 67, SCB_CFSR 68;
  g_lpflexcomm4_irq_count 70, g_init 72, g_zero 73, g_cfsr 75, g_unaligned_val 76; putc 77–84; puts
  86–92; SysTick 94–100; LP_FLEXCOMM4 105–108; UsageFault_Handler 110–115 (store 112, for(;;) 113–114);
  main 117–187: g_zero = g_init 119, CCR = 0x00000201u 178, SHCSR RMW 179, dsb 180, isb 181,
  g_unaligned_val = *(volatile uint32_t *)((unsigned char *)&g_zero + 1) 183, for(;;) 185–186. .S 130
  / .ld 45 unchanged. Line table by tutor (user table not sent). Cosmetic, left: line 70 ".S lines
  100–107", line 169 old citation.
Weak point (repeat): saved file ≠ composition on the first send (core load line dropped); line
  table not sent twice.

### 3.2a build — session 18, 2026-11-06
Predictions LOCKED (B1–B14; Lock flag B14 reason 0x34C vs own B8 → addendum B14 → 0x169, value
  re-derived). Score 9.5/14.
Result (raw): Clean 0/0; FLASH 948 B, RAMB 24 B; text 943 data 4 bss 20 dec 967. .text 0x2CF @0xE0
  (ends 0x3AF); .data LMA 0x3B0. .o Algn: putc / SysTick / LP_FLEXCOMM4 / UsageFault_Handler / main
  2**2, puts 2**1. Symbols: SysTick_Handler 0x114, LP_FLEXCOMM4_IRQHandler 0x14C, UsageFault_Handler
  0x168 (0x18, unchanged; literal 0x17C now 0x2000_800C), main 0x180 (0x17C), Reset_Handler 0x2FC,
  Default_Handler 0x350, uart4_puts 0x354 (bl target); g_init 0x2000_8000, irq count 0x8004, g_zero
  0x8008, g_cfsr 0x800C, g_unaligned_val 0x8010, tick.0 0x8014, __bss_end__ 0x8018. main: push {r7,lr};
  sub sp,#8; add r7,sp,#0 → stop-at-main 0x186; f at [r7,#4] (0x192); IRQ-39 dsb/isb 0x23A/0x23E;
  if/else 0x242–0x254; CCR 0x258 ldr / 0x25A movw r2,#0x201 / 0x25E str; SHCSR 0x260–0x26A (literal
  0x2F0 shared); dsb 0x26C; isb 0x270; 0x274 ldr r3 ← 0x2F4 (0x2000_8009); 0x276 ldr r3,[r3,#0]
  (unaligned LDR); 0x278 ldr r2 ← 0x2F8 (0x2000_8010); 0x27A str r3,[r2,#0]; for(;;) nop 0x27C / b.n
  0x27E; literals 0x280–0x2F8 (31 words). Strings 0x380 / 0x388 / 0x398. .isr_vector: word0
  0x2001_0000, word1 0x2FD, slots 2–5/7/11/12/14/16–54 0x351, slot 6 0x169, 8/9/10/13 0, slot 15
  0x115, slot 55 0x14D.
Learned: GCC folds &g_zero + 1 into one literal (0x2000_8009) at -O0 — no add. The load = 8 B code
  + 2 literals; deleting the division removed 20 B + 1 literal (net −8). Deleting two locals shrinks
  main's frame sub sp,#16 → #8 (Thread sp in main becomes 0x2000_FFF0).
Grades: B1 ❌ (−6; real −8) B2 ✅ B3 ✅ B4 ❌ (pad rule: 943 + 4 + 1) B5 ✅ B6 ✅ B7 ✅ B8 ✅ B9 ✅
  B10 ❌ (copied 16; own edit deleted two locals) B11 ✅ B12 ½ (instruction right; "add 1" reason —
  folded literal) B13 ❌ (0x27C; cascade of B1) B14 ✅ (addendum).
Weak points (repeats): "unchanged" rows not checked against own edit (B10); logged size rules
  overridden (ldr-literal 2 B; FLASH pad rule). Strengths: placement rows B6–B9/B11/B14 all right.

### 3.2a unaligned LDR with UNALIGN_TRP = 0 — session 18, 2026-11-06
Predictions LOCKED (R1–R13; Lock flags R3/R10/R11/R13; addenda: R3 → 0x0012_3456, R10 → 0x2000_FFE8;
  R11 / R13 reasons re-worded twice, values kept). Score 8.5/13.
Result (raw): launch "Temporary breakpoint 1, main () at …:119"; break *0x27C → Breakpoint 2 (line
  185); info breakpoints: only #2. continue → "Breakpoint 2, main () at ../source/
  frdmmcxn236_m33_scratch.c:185": pc 0x27C, g_unaligned_val 0x123456, g_zero 0x12345678, g_cfsr 0,
  CCR 0x201, SHCSR 0x0004_0000, CFSR 0, HFSR 0; x/16xb &g_zero: 78 56 34 12 | 00 00 00 00 | 56 34 12
  00 | 01 00 00 00 (tick.0 = 1); sp 0x2000_FFF0, r2 0x2000_8010, r3 0x0012_3456, xPSR 0x0900_0000.
Learned: with UNALIGN_TRP 0, the LDR at 0x276 from 0x2000_8009 performs the access (RLPVP p.228; CCR
  D1.2.9 p.1474) — bytes 0x8009–0x800C (56 34 12 | 00) assembled little-endian (B7.5 p.225,
  AIRCR.ENDIANNESS 0) = 0x0012_3456; no UNALIGNED flag, no fault. Thread sp in main = 0x2000_FFF0 on
  image 3.2a. xPSR Q + T only: last flag setter = uart4_puts' adds r7,#8 (via bl 0x24C); NOP
  C2.4.130 p.770 does nothing. One SysTick before the halt (tick 1 — timing inference).
Grades: R1 ½ (file/line) R2 ✅ R3 ✅ R4 ½ (line 118 → 119) R5 ✅ R6 ½ (177 → 178) R7 ½ (178 → 179, no
  base) R8 ✅ R9 ✅ R10 ❌ (sub sp,#16 vs measured B10 #8) R11 ❌ (value not re-derived, 3 addenda)
  R12 ✅ R13 ❌ (3.1a Z copied; its movs deleted; nop named as flag setter).
Weak points (repeats): addenda re-word reasons without re-deriving values (R11 ×3); values from an
  older image / older step (R10 frame, R13 Z); line numbers not re-counted after the header insert.
Strengths: R3 derivation (bytes + objects + endianness, cited); fault-status rows R5/R8/R9 right.
Board: Terminate at 0x27C confirmed (user).

### 3.2b composition accepted (rounds 1–2) — session 18, 2026-11-06
D1 post-edit line 179 REG32(SCB_CCR) = 0x00000209u; comment "D1.2.9 p.1474: 3.2a measured 0x201 +
  UNALIGN_TRP = 1"; bits 9/0 RES1 (p.1473/p.1474), DIV_0_TRP 0 (no division left), UNALIGN_TRP 1
  (p.1474). D2 SCB_CCR_UNALIGN_TRP_Msk kept unused; edited lines 18, 19, 179 only. D3 header line 18,
  */ 19 (text sent twice with a leading "* "; plain in the file). D4 no bp at 0x168 (vector catch),
  own hardware bp *0x174. D5 halt 1 0x168: stop line, pc, CFSR/HFSR/SHCSR/CCR, g_cfsr,
  g_unaligned_val, x/8xw $sp; halt 2 0x174: same + x/8xw $sp+4 (push {r7} at 0x168). D6 Terminate
  at 0x174.
Round-1 fixes: line 177 → 179 (CCR was 178 in 3.2a, +1 header); comment base; bit-4 reason; one D2
  option; "software" → hardware bp; predicted values removed from the read list; halt-2 frame offset
  +4.
Weak points (repeats): line numbers not re-derived after an insert (3rd); predicted values in a
  composition (2nd this session); header "* " format (3rd).

### 3.2b saved-file check — session 18, 2026-11-06
Round 1 ACCEPTED (188 lines; diff vs 3.2a = the 3 agreed edits): header line 18 "3.2b: Unaligned word
  access with UNALIGN_TRP = 1" (plain text), */ 19; CCR line 179 = 0x00000209u. Post-edit: macros
  65–69; irq count 71, g_init 73, g_zero 74, g_cfsr 76, g_unaligned_val 77; UsageFault_Handler 111–116
  (store 113, for(;;) 114–115); main 118–188 (g_zero = g_init 120, CCR 179, SHCSR 180, dsb 181, isb
  182, load 184, for(;;) 186–187). .S / .ld unchanged. Line table by tutor (user table not sent —
  3rd time).

### 3.2b build — session 18, 2026-11-06
Predictions LOCKED (E1–E10, no Lock flags). Score 8.5/10.
Result (raw): Clean 0/0; FLASH 948 B, RAMB 24 B; text 943 data 4 bss 20 dec 967 — identical to 3.2a.
  main 0x180 (0x17C): CCR 0x258 ldr r3 ← 0x2EC / 0x25A movw r2,#521 (0x209) / 0x25E str; SHCSR
  0x260–0x26A; dsb 0x26C; isb 0x270; 0x274 ldr r3 ← 0x2F4 (0x2000_8009); 0x276 ldr r3,[r3,#0]; 0x278
  ldr r2 ← 0x2F8 (0x2000_8010); 0x27A str; for(;;) nop 0x27C / b.n 0x27E. UsageFault_Handler 0x168
  (park nop 0x174 / b.n 0x176, literals 0x178 CFSR / 0x17C g_cfsr 0x2000_800C). Symbols as 3.2a.
  .isr_vector identical to 3.2a (slot 6 0x169).
Learned: changing a movw immediate (0x201 → 0x209) changes no size or address.
Grades: E1–E4 ✅ E5 ❌ (0x258 = the CCR ldr; movw 0x25A) E6 ✅ E7 ✅ E8 ½ (reason cited the 3.1c map,
  handler 0x34C there) E9 ✅ E10 ✅.

### 3.2b unaligned LDR with UNALIGN_TRP = 1 — session 18, 2026-11-06
Predictions LOCKED (H1–H23; Lock flag H9 arithmetic → addendum H9 → 0x2000_FF88, H21 → 0x2000_FF84).
  Score 17.5/23.
Result (raw): launch "Temporary breakpoint 1, main () at …:120", $pc 0x186; break *0x174 →
  Breakpoint 2 (line 114); info breakpoints: only #2. continue → "Program received signal SIGSTOP,
  Stopped (signal). UsageFault_Handler () at …:112": pc 0x168, CFSR 0x0100_0000, HFSR 0, SHCSR
  0x0004_0008, CCR 0x209, g_cfsr 0, g_unaligned_val 0, sp 0x2000_FF88; frame R0 0xA, R1 0x2000_8018,
  R2 0xE000_ED24, R3 0x2000_8009, R12 4, LR 0x369, RA 0x276, RETPSR 0x0910_0000. continue →
  "Breakpoint 2, UsageFault_Handler () at …:114": pc 0x174, g_cfsr 0x0100_0000, CFSR 0x0100_0000,
  sp 0x2000_FF84, lr 0xFFFF_FFE9, xPSR 0x0900_0006; x/8xw $sp+4 @0x2000_FF88 = same frame.
Learned: CCR.UNALIGN_TRP 1 (0x209, D1.2.9 p.1474; RMHCM p.228) → the LDR at 0x276 from 0x2000_8009
  raises UsageFault directly (USGFAULTENA set; IPSR 6, HFSR 0); CFSR = UFSR.UNALIGNED bit 8 =
  0x0100_0000 (D1.2.270 p.1898); synchronous → ReturnAddress = the faulting LDR 0x276 (RRNDS p.228);
  the store at 0x27A never ran → g_unaligned_val 0; vector catch VC_CHKERR covers the alignment check
  (D1.2.37 p.1527) — SIGSTOP at 0x168 (5th run). Entry from Thread sp 0x2000_FFF0 → 0x2000_FF88
  (26 words, FPCA 1), 0xFF84 after push {r7}. Frame R2 = 0xE000_ED24 (last writer ldr r2 at 0x264,
  SHCSR RMW), LR slot 0x369 (puts 0x354 + 0x14 | 1), RETPSR 0x0910_0000 (N Z C V 0 from puts' adds
  r7,#8; Q, T, SFPA).
Grades: H1 ✅ H2 ✅ H3 ✅ H4 ✅ H5 ✅ H6 ½ (line 178 → 179) H7 ✅ H8 ✅ H9 ✅ H10 ✅ H11 ✅ H12 ❌
  (stopped at movw 0x25A; ldr r2 0x264 later) H13 ✅ H14 ✅ H15 ❌ (3.1d 0x371 copied; puts moved
  to 0x354) H16 ✅ H17 ❌ (Z copied from 3.1d; movs deleted — repeat of 3.2a R13) H18 ❌ (bp #2 per
  own info breakpoints; file/line) H19 ✅ H20 ✅ H21 ✅ H22 ✅ H23 ❌ (Z).
Weak points (repeats): "copied from 3.1d" rows not checked against this step's own edits (LR slot,
  Z — 2nd time in one day); register last writer stopped one writer early (R2). Strengths: FAULT
  HALTS note applied (H1 right after 3 misses); fault-status rows all right; sp arithmetic fixed when
  flagged.
3.2 (UNALIGN_TRP) CLOSED. Open: Terminate at 0x174 — not confirmed.

### Session 18 end (2026-11-06 16:33)
Session ended at the user's request after 3.2b; continuation in a new chat (session 19). Image 3.2b in
  flash (sources = saved 3.2b .c 188 lines / .S 130 / .ld 45 by wc). GDB-only state none. Terminate
  after the 3.2b run (halted at 0x174): NOT confirmed — ask at session-19 intake. Next: session-19
  intake → launch check (pc 0x186, x/2i 0x25A) → 3.3 MSPLIM_S / STKOF (proposed). Bench Map v100.

### Session 18 summary (2026-11-05 / 2026-11-06)
Scores: 3.2a build 9.5/14, board 8.5/13; 3.2b build 8.5/10, board 17.5/23. Compositions: 3.2a
  (3 rounds + 2 saved-file rounds), 3.2b (2 rounds + 1 saved-file round).
Closed: unaligned word LDR from Normal memory (0x2000_8009) with UNALIGN_TRP 0 completes (value
  0x0012_3456, little-endian, spans two .bss objects) and with UNALIGN_TRP 1 raises an UNALIGNED
  UsageFault (CFSR 0x0100_0000, RA = the LDR, destination never written); vector catch also covers the
  alignment check; intake questions (Remove All pressed before 3.1e run 1; line-179 comment; $2).
New facts: image 3.2a/3.2b map (see build blocks); Thread sp in main 0x2000_FFF0 (sub sp,#8); fault
  entry sp 0x2000_FF88; frame R2 0xE000_ED24, LR slot 0x369, RETPSR 0x0910_0000; GCC folds constant
  address arithmetic into one literal at -O0.
Tutor corrections: none on chip facts.
Weak points this session: rows copied from an older image without checking this step's edits
  (frame after deleting locals, Z after deleting movs, LR slot after puts moved — 5 rows); register
  last writer stopped early (H12); addenda changing reasons but not values (R11 ×3); line numbers off
  by one after header inserts (3×); size counting (removed literals, new literals, FLASH pad); asked
  instruction vs neighbour (E5); compositions with predicted values (2×), hedged options, header
  "* "/"/*" format (3×); saved file missing the core line (3.2a round 1); line tables not sent (3×).
Strengths: FAULT HALTS applied (3.2b H1); all fault-status rows right in both steps; placement rows
  right; endianness derivation (3.2a R3); arithmetic fixed when flagged (H9/H21).

### Session 19 intake (2026-11-06 17:01)
learning_log.md (2,695 lines) ends with the session-18 summary exactly once (line 2677); 166 "###"
  headings, no duplicates. session18_log_blocks.md (231 lines, 15 blocks): each heading once in the
  log; whole file present verbatim once, contiguous at the end (from line 2465) → nothing missing or
  duplicated.
Saved sources = continuation prompt: .c 188 lines (header 3.2a 17 / 3.2b 18 / */ 19; macros 65–69;
  g_zero 74, g_cfsr 76, g_unaligned_val 77; UsageFault_Handler 111); .S 130 (slot 6 line 23, weak pair
  129–130); .ld 45 (wc). Bench Map v100 read (headline current) → v101 (intake entry, trail, footer).
Asked: (a) 3.2b Terminate at 0x174 + unplug; (b) Breakpoints view empty.

### Session 19 launch check + 3.3 issued — 2026-11-06 17:36
Intake answers: (a) 3.2b run Terminated at 0x174 + board unplugged: yes; (b) Breakpoints view empty:
  yes. Launch: "Temporary breakpoint 1, main () at ../source/frdmmcxn236_m33_scratch.c:120";
  info breakpoints none; print/x $pc → $2 = 0x186; x/2i 0x25A = movw r2,#521 @ 0x209 / str r2,[r3,#0]
  → image 3.2b in flash, no rebuild.
3.3 (STKOF via MSPLIM_S) sources opened (pdftotext): B3.21 p.115–117 RXQDS, IKDPG, RTCXN, RDKSR,
  RZLZG (entry violation → STKOF, SP = limit, no pushes below), RGGRH, RBRLT (MSR to SP/limit not
  checked), IBJHX, IRRDX, RQFPF, IZKSV (limit above SP → next SP-modifying instruction faults), IDSJN,
  RQMRP, RBRCX; B3.8 RMQVJ p.79; B3.19 RVNSK p.108; B3.24 ILBGQ/RNMTT/RMRTR/RGDNT p.124 (derived
  exceptions, escalation/lockup); B3.33 RVKTX/RQSSB/RGJJG p.148; MSPLIM D1.2.177 p.1757 (LIMIT 31:3,
  Warm reset 0, banked); CCR STKOFHFNMIGN bit 10 p.1473; UFSR STKOF bit 4 p.1898; MRS C2.4.125 p.755 /
  MSR (register) C2.4.126 p.760–762 (SYSm 00001010 MSPLIM; Secure → MSPLIM_S.LIMIT = R[n][31:3];
  Rn 13/15 UNPREDICTABLE); DEMCR VC_INTERR bit 9 p.1526 ("stack violations and context stacking"),
  VC_CHKERR bit 6 p.1527 (alignment + divide-by-zero only). Composition C0–C8 issued. Bench Map v102.

### 3.3 composition round 1 — session 19, 2026-11-06 19:00
Accepted: C0 delete the 3.2b load line + CCR back to 0x00000201u (3.0 measured base); C5 g_cfsr =
  REG32(SCB_CFSR) unchanged; C6 header text "3.3: Stack limit checking with MSPLIM".
Fix in round 2: C0 CCR comment text + fate of g_unaligned_val / SCB_CCR_UNALIGN_TRP_Msk; C1 operand
  "limit" undefined (C local → main's frame changes, Thread sp 0xFFF0 no longer valid), empty clobber
  ": )", barrier "per architectural rules" unsourced, placement after SYST_CSR = 0x7 (SysTick entry is
  a checked frame creation, RDKSR p.115); C2 no value + "e.g."; C3 two options; C4 restates the
  question; C6 macros unnamed; C7 "entry or park loop"; C8 halts per read + exact frame-dump command.
Method given with different numbers (sp 0x2000_A010, L 0x2000_A008, push of 12 B). Bench Map v103.

### 3.3 composition round 2 — session 19, 2026-11-06 19:38
Accepted: C0 delete load + g_unaligned_val + SCB_CCR_UNALIGN_TRP_Msk, CCR 0x201; C1 __asm__ volatile
  ("msr msplim, %0" : : "r" (0x2000FFE0)); no barrier (untested, no rule opened); after UART init,
  before SYST_RVR; C5; C6 macros none; C7 no bp at 0x168 + bp at park loop (provisional); C8 provisional.
Fix in round 3: C2 reason contradicts IKDPG (sub sp,#8 lands ON L) + RZLZG (entry rule) cited for an
  instruction; C3 "(or sub sp,#8)"; C4 no analysis; C6 header "* " (4th); C0 exact comment. Bench Map v104.

### 3.3 composition round 3 — session 19, 2026-11-06 22:13
Accepted: C0 comment "D1.2.9 p.1474: 3.0 measured 0x201; UNALIGN_TRP = 0"; C2 L = 0x2000_FFE0 (puts
  push {r7,lr} → 0xFFE8, sub sp,#8 → 0xFFE0 = L allowed, putc push {r7} → 0xFFDC < L); C3 putc push
  {r7} at 0xE0; C6 header plain.
Fix in round 4: puts "0x30C" = image 2.5d (INVALID); C4 FPCA 0 in putc contradicts 3.2b 26-word entry;
  RZLZG for the instruction again; RVKTX read as unconditional lockup; RGDNT not done; handler push
  {r7} not walked; C7/C8 not reconciled. Bench Map v105.

### 3.3 composition round 4 — session 19, 2026-11-06 23:10
Accepted: C2 (puts address from the 3.3 objdump); C3; C4 steps 1–4 (FPCA 1 → 26 words; IBJHX/RBRCX,
  SP not written = inference; entry from 0xFFE0 below L → RZLZG SP = L; RGDNT derived 0 < preempted
  256 → taken; RVKTX conditions none); C6.
New sources opened: RGNVS p.143 (synchronous exception at ≤ execution priority → HardFault, else
  Lockup), RDQRR p.143 (escalated → HFSR.FORCED), RTKCW p.143, RVGMR p.147, IRKJB p.146.
Fix in round 5: C3 listing source; C4 step 5 escalation uncited; step 6 HardFault entry not walked;
  C7 dropped VC_INTERR halt, 0x0C = vector slot not code, "or"; C8 predicted values + "or", no
  Lockup fallback. Bench Map v106.

### 3.3 composition round 5 — session 19, 2026-11-06 23:16
Accepted: C3 source (image 2.5d uart4_putc listing, log line 1866); C4 step 5 handler push {r7} at
  SP = L → 0xFFDC < L; step 6 HardFault entry FPCA 0 → 8 words from L → 0xFFC0 < L → RZLZG;
  STKOFHFNMIGN 0 in 0x201 (IRRDX p.116, p.1473); C7 bp at Default_Handler; C8 no-stop fallback.
Fix in round 6: RVKTX quote "…the processor locks up" INVENTED (not on p.148; real sentence: "the
  point of PE lockup is when, after the exception to be taken has been chosen, the handler for that
  exception is entered"); RGNVS applied to the derived exception (rule = RGDNT); step-6 label;
  C7 bp 0x168 reverts round 2 + logged "redundant" rule; C8 x/8xw $sp at SP = L; lockup PC typed
  0xE000_FFFE (RGJJG 0xEFFFFFFE) + predicted value. Bench Map v107.

### 3.3 composition round 6 — session 19, 2026-11-07 00:26
Accepted: C4 step 6.1–6.2 frame; step 6.4 real RVKTX sentence, "Secure HardFault already active
  during its own entry" unresolved = inference; C7 no bp 0x168 + one hw bp at Default_Handler; C8
  lockup read print/x $pc vs RGJJG p.148 + Suspend fallback.
Fix in round 7: step 5 RGNVS numbers; step 6.1 inference sourced to RZLZG (3rd misuse); step 6.3
  derived exception named HardFault (it is the STKOF UsageFault), no numbers; outcome; C8 halt-1 read
  list missing; dump range vs own numbers. Bench Map v108.

### 3.3 composition accepted (rounds 1–7) — session 19, 2026-11-07 11:47
C0 delete load line + g_unaligned_val + SCB_CCR_UNALIGN_TRP_Msk; CCR 0x00000201u "D1.2.9 p.1474: 3.0
  measured 0x201; UNALIGN_TRP = 0". C1 __asm__ volatile ("msr msplim, %0" : : "r" (0x2000FFE0)); no
  barrier (untested, no rule opened); after UART CTRL, before SYST_RVR. C2 L = 0x2000_FFE0. C3 trigger
  putc push {r7} 0xE0. C4 (inference where marked): instruction violation IBJHX p.116 / RBRCX p.117, SP
  not written (inference); UsageFault entry from 0xFFE0, FPCA 1 → 26 words → below L → RZLZG SP = L;
  RGDNT derived 0 < preempted 256 → taken; RVKTX conditions none; handler push {r7} at SP = L →
  0xFFDC < L; RGNVS UsageFault 0 = execution 0 → HardFault; HardFault entry from L, FPCA 0 → 8 words →
  0xFFC0 < L → RZLZG; RGDNT derived STKOF UsageFault 0 = preempted 0 → HardFault or Lockup; RVKTX
  timing unresolved → outcome HardFault entered = inference; STKOFHFNMIGN 0. Handler/.S unchanged. C5
  g_cfsr kept. C6 header "3.3: Stack limit checking with MSPLIM", no macros. C7 no bp 0x168 (VC_INTERR
  p.1526, DEMCR 0x011007F0); one hw bp at Default_Handler. C8 halt 1 reads + first would-be-frame dump
  → continue; halt 2 reads + x/8xw 0x2000_FFC0; no stop → Suspend + recovery, print/x $pc vs RGJJG
  p.148; Terminate while halted.
Errors fixed across rounds: items restating the question; "or"/"e.g." in decisions; C1 operand from a
  C local; RZLZG used for instruction violations (3×); IKDPG contradiction; puts 0x30C (2.5d); FPCA 0
  in putc; RVKTX read as unconditional; INVENTED RVKTX quotation; RGNVS vs RGDNT scope; derived
  exception named HardFault; comparisons without numbers; 0x0C vector slot as a handler address; bp
  0x168 re-added; predicted values in C8; frame dumps at $sp when SP = L (2×); header "* " (4th).
Bench Map v109.

### 3.3 saved-file check (rounds 1–2) — session 19, 2026-11-07 12:28
Round 1 (191 lines): mask line 67 kept; g_unaligned_val and load commented out instead of deleted;
  no line table; halt-1 dump not sent.
Round 2 ACCEPTED (188 lines; diff vs 3.2b = agreed edits): header 19, */ 20; macros 66–69; irq count
  71, g_init 73, g_zero 74, g_cfsr 76; tick 96; UsageFault_Handler 111–116 (store 113); main 118–188:
  CTRL 154, msr msplim 156, SYST_RVR 160, Hello 164, CCR 181, SHCSR 182, dsb 183, isb 184, for(;;)
  186–187. User line table: 6 rows wrong → tutor table.
Tutor correction: composition C6 said header line 20 / */ 21 — wrong: the new history line takes the
  old */ line 19, */ moves to 20 (user's file right). The continuation prompt's "header history line
  20" was also wrong.

### 3.3 C8 halt-1 dump + line table — session 19, 2026-11-07 12:32–13:00
Halt-1 dump sent 5×: 8-word range from L (2×), 0x2000_F800 (504 words) / slot +0x1F8, then ACCEPTED
  x/26xw 0x2000_FF78 (0x2000_FF78–0x2000_FFDF = 0x68 B below 0x2000_FFE0), RA slot 0x2000_FF90. Halt 2
  x/8xw 0x2000_FFC0, RA slot 0x2000_FFD8. Line table: 14 of 16 rows wrong on resend, then ACCEPTED from
  Select-String (12:40). Bench Map v113.

### 3.3 build — session 19, 2026-11-07 13:13
Predictions LOCKED (E1–E14, addendum 13:10; Lock flags E6 no source, E11 value vs own reason).
  Score 7.5/14.
Result (raw): Clean 0/0; FLASH 944 B, RAMB 20 B; text 939 data 4 bss 16 dec 959. .text 0x2CB @0xE0
  (ends 0x3AB); .data LMA 0x3AC. Symbols: uart4_putc 0xE0, SysTick_Handler 0x114, LP_FLEXCOMM4 0x14C,
  UsageFault_Handler 0x168, main 0x180 (0x178), Reset_Handler 0x2F8, Default_Handler 0x34C,
  uart4_puts 0x350; g_cfsr 0x2000_800C, tick.0 0x8010, __bss_end__ 0x8014. main: 0x20E ldr r3 ←
  0x2C4 (0x2000_FFE0); 0x210 msr MSPLIM, r3; bl puts 0x224; CCR 0x25E–0x264; SHCSR 0x266–0x270; for(;;)
  nop 0x27A; pad nop 0x27E. .isr_vector: word1 0x2F9, Default 0x34D, slot 6 0x169, 15 0x115, 55 0x14D.
Learned: net code −6 plus a NEW 2 B pool pad → main −4 → everything after main −4. msr MSPLIM = f383
  880a (4 B).
Grades: E1 ½ (base 945 vs 943; pad missed) E2–E5 ✅ E6 ❌ E7 ❌ (0x186 = stop-at-main) E8 ❌ (0x10C vs
  unchanged 0x168) E9 ✅ E10 ❌ E11 ❌ E12 ❌ E13 ✅ E14 ✅.
TUTOR CORRECTION: composition C4 accepted with "UsageFault enabled" at the trigger, but the SHCSR RMW
  (line 182) ran after uart4_puts("Hello") (line 164) → USGFAULTENA not set when putc's push faults.
  Program order not checked at acceptance. User chose (B): move CCR/SHCSR/dsb/isb before the msr.

### 3.3 (B) saved-file check + build — session 19, 2026-11-07 14:01–14:06
First send byte-identical (move not applied). Second send ACCEPTED (188 lines): CCR 156, SHCSR 157,
  dsb 158, isb 159, msr 161, SYST_RVR 165, Hello 169, PRI_15 171, SYST_CSR 173, ISER1 175, ISPR1 176,
  dsb 177, isb 178, if/else 180–184, for(;;) 186–187.
Build predictions LOCKED (R1–R8; Lock flags R3 no source, R4 store vs movw). Score 0/8.
Result (raw): Clean 0/0; FLASH 944, RAMB 20; text 939 / 4 / 16 — identical; all symbols identical to
  the 3.3 build. main: CCR 0x20E ldr / 0x210 movw r2,#513 / 0x214 str; SHCSR RMW 0x216–0x220; dsb
  0x222; isb 0x226; 0x22A ldr r3 ← 0x2CC (0x2000_FFE0); 0x22C msr MSPLIM, r3; SYST_RVR 0x230; 0x23E
  ldr r0 ← 0x37C; 0x240 bl uart4_puts (return 0x244); if/else 0x264–0x276; join nop 0x27A; for(;;)
  nop 0x27C / b.n 0x27E; literals 0x280–0x2F4.
Learned: moving a block keeps size; pool reordered; pad nop gone, join nop appears.
Grades: R1–R8 ❌ (all copied 3.2a/3.2b values: 945, 950, 272, 0x350 …).

### 3.3 STKOF via MSPLIM_S — session 19, 2026-11-07 14:30
Predictions LOCKED (H1–H23; addendum 14:22 supersedes 14:17; Lock flags H3/H17 value vs own reason,
  H18/H19 swapped — graded by named register). Score 12/23.
Result (raw): launch "Temporary breakpoint 1, main () at …:120", $2 = 0x186; break *0x34C →
  Breakpoint 2 (startup_m33.S:118, hw); info breakpoints: only #2. continue → "Program received
  signal SIGSTOP, Stopped (signal). UsageFault_Handler () at …:112": pc 0x168, CFSR 0x0010_0000, HFSR
  0, SHCSR 0x0004_0008, CCR 0x201, g_cfsr 0, msplim_s 0x2000_FFE0, sp 0x2000_FFE0, lr 0xFFFF_FFE9,
  xpsr 0x2900_0006; x/26xw 0x2000FF78: all 0. continue → "Program received signal SIGSTOP …
  Default_Handler () at ../source/startup_m33.S:118": pc 0x34C, CFSR 0x0010_0000, HFSR 0x4000_0000,
  SHCSR 0x0004_000C, CCR 0x201, g_cfsr 0, msplim_s 0x2000_FFE0, sp 0x2000_FFE0, lr 0xFFFF_FFF1, xpsr
  0x2900_0003; x/26xw 0x2000FFC0: 0xFFC0–0xFFDF 0; 0xFFE4 0x37C, 0xFFE8 0x2000_FFF0, 0xFFEC 0x245;
  0xFFF4 0x4040_0000, 0xFFFC 0x333; 0x2001_0000+ 0.
Learned: STKOF (UFSR bit 4 = CFSR 0x0010_0000) from putc push {r7} 0xE0 is vector-caught (VC_INTERR
  p.1526): SIGSTOP at 0x168. Entry stacking below L not performed, SP = L (RZLZG) — 26-word frame never
  written → handler could not return (inference). UsageFault taken (IPSR 6, HFSR 0, EXC_RETURN
  0xFFFF_FFE9). Handler push {r7} at SP = L → STKOF at priority 0 = execution 0 → HardFault (RGNVS /
  RDQRR p.143): FORCED, SHCSR 0x4000C, IPSR 3, EXC_RETURN 0xFFFF_FFF1, SP still L, nothing below L;
  VC_HARDERR SIGSTOP at 0x34C (bp #2 there → SIGSTOP, not "Breakpoint 2"); NO lockup (measured; no
  DDI0553 sentence found that predicts it). g_cfsr 0 (handler body never ran). CFSR sticky. xPSR C = 1
  from puts' cmp r3,#0 at 0x36E (r3 'H'); Q, T. Above L intact: puts frame (0x37C, r7 0x2000_FFF0, LR
  0x245), main f 3.0f at 0xFFF4, 0x333 at 0xFFFC (main's stacked LR — inference).
Grades: H1 ✅ H2 ✅ H3 ❌ (addendum moved right 0x0010_0000 → 0x0100_0000) H4 ✅ H5 ✅ H6 ½ H7 ✅ H8 ✅
  H9 ❌ (own RZLZG) H10 ✅ H11 ❌ (Z copied) H12 ❌ H13 ✅ H14 ❌ (0x350 vs 0x34C) H15 ❌ H16 ½ (D1.2.122)
  H17 ❌ H18 ✅ H19 ❌ (handler body never ran) H20 ❌ H21 ❌ (Handler EXC_RETURN) H22 ❌ H23 ✅.
Weak points (repeats): predictions contradicting own accepted composition (4 rows); addendum changing
  a right value to wrong (H3); Z copied (4th); value vs own reason (H17); objdump value not copied
  (H14); Thread EXC_RETURN in a Handler entry (H21).
Board: Terminate at 0x34C confirmed (user). 3.3 CLOSED. Bench Map v119.

### Session 19 end (2026-11-07 14:43)
Session ended at the user's request after 3.3; continuation in a new chat (session 20). Image 3.3 (B)
  in flash (.c 188 / .S 130 / .ld 45): traps on every run before "Hello" (STKOF → 0x168 → HardFault
  0x34C). GDB-only state none. Terminate after the 3.3 run confirmed. Next: session-20 intake →
  launch check (pc 0x186, x/2i 0x22A) → 3.4 MMFARVALID/BFARVALID + real BusFault (proposed).

### Session 19 summary (2026-11-06 / 2026-11-07)
Scores: 3.3 build 7.5/14, (B) build 0/8, board 12/23. Compositions: 3.3 (7 rounds + C8 dump 5 sends +
  2 saved-file rounds + B move 2 sends).
Closed: STKOF via MSPLIM_S (vector-caught by VC_INTERR; SP = limit, frame not written; handler push
  escalates to HardFault FORCED; no lockup); "is STKOF vector-caught?" (yes); intake questions.
New facts: msr MSPLIM encoding f383 880a; image 3.3 map (see build blocks); STKOF CFSR 0x0010_0000;
  HardFault from a UsageFault handler: SHCSR 0x0004_000C, LR 0xFFFF_FFF1.
Tutor corrections: header line 20 → 19 (composition C6); composition C4 accepted without checking
  program order (SHCSR RMW after the trigger) → option B rebuild.
Weak points this session: INVENTED quotation (RVKTX); rules outside scope (RZLZG 3×, RGNVS);
  analysis stopping one step early; frame size not carried (8 vs 26 words); hex arithmetic not
  back-checked; line tables typed not grepped (5 sends); saved file ≠ agreed edit on first send (2×);
  "unchanged" rows copied from older images despite the named source (R1–R8, E1 base); board rows
  contradicting own accepted analysis; Z copied (4th); addendum turning right into wrong (H3).
Strengths: SP walk vs limit (C2) right; FPCA/frame-size reasoning (C4 step 1); vector-catch halts
  right; MSPLIM value; fault-enable/status rows; RZLZG used right for H23.

### Session 20 intake (2026-11-07 14:58)
learning_log.md (2,893 lines, 182 "###" headings, no duplicates) ends with the session-19 summary
  exactly once (line 2878). session19_log_blocks.md (197 lines, 16 blocks): each heading once in the
  log; whole file present verbatim once, contiguous at the end (from line 2697) → nothing missing or
  duplicated. Saved sources = continuation prompt: .c 188 (header 3.3 19 / */ 20; macros 66–69;
  g_cfsr 76; UsageFault_Handler 111–116; CCR 156, SHCSR 157, dsb 158, isb 159, msr 161, SYST_RVR 165,
  Hello 169); .S 130 (slot 6 line 23, weak pair 129–130); .ld 45 (wc). Bench Map read in full (footer
  v120 vs prompt v119 — numbering only) → v121; 3.3 score rows added (missing from the table).

### Session 20 launch check + 3.4 issued — 2026-11-07 21:19
Launch: "Temporary breakpoint 1, main () at ../source/frdmmcxn236_m33_scratch.c:120"; info breakpoints
  none; $2 = 0x186; x/2i 0x22A = ldr r3,[pc,#160] (0x2cc) / msr MSPLIM, r3 → image 3.3 (B), no rebuild.
3.4 sources opened (pdftotext): BFAR D1.2.6 p.1466; BFSR D1.2.7 p.1467–1469 (BFARVALID 7, LSPERR 5,
  STKERR 4, UNSTKERR 3, IMPRECISERR 2, PRECISERR 1 → BFAR written + BFARVALID set, IBUSERR 0);
  MMFARVALID bit 7 p.1740; ITRJJ p.83; RXMRH/RDDJJ/RFLDT/RTSCP p.88; RXPWN/RGBKY p.89; IKJPM p.90;
  BUSFAULTENA bit 17 p.1838; VC_BUSERR bit 8 p.1526; RDWKP p.146; INHFZ p.143; AIRCR.IESB p.1460;
  TRM §1.4.1 PDF p.13 (loads and stores support precise bus errors); RM 50.5.5 p.1862 (LPUART access
  outside valid map → bus error; RO write "can cause"), p.389 eDMA, p.1923 SAI, p.43 RAME2/E3. No RM
  sentence for a CPU access to a gated PORT (log 1e.3a GDB only).
New doc discrepancy: TRM Table 3-3 PDF p.39 "UNALIGN_TRP … fixed at 1" vs measured 3.2a (0 → no fault).
Composition C0–C9 issued. Bench Map v122.

### 3.4 composition round 1 — session 20, 2026-11-07 21:55
Accepted: C0 delete the msr msplim statement; C2 a load; C3 BUSFAULTENA in C by SHCSR RMW + dsb + isb;
  C4 handler records CFSR, BFAR only when BFARVALID; C6 header text (plain).
Fix in round 2: C0 file name (main.c) + line 161; C1 RM p.1862 sentence is LPUART-only — 0x400F_F000 =
  Table 16 p.41 "Reserved" (0x4 NS alias; we run Secure, peripherals 0x5…), no opened sentence says
  Reserved → bus error, "why clocked" missing; C2 volatile C local (main's frame), precise argued from
  RDDJJ/IKJPM (neither says a load is precise), predicted outcome inside a decision; C3 lines vs trigger,
  existing line-157 RMW + barriers 158/159; C4 g_cfsr redefined (line 76), BFARVALID "bit 7" tested on a
  CFSR word (BFSR = CFSR 15:8, D1.2.11 p.1477), no park/return decision, no lines; C5 vector.S →
  startup_m33.S, slot 5 line 22, .set vs .thumb_set, no lines/header; C6 SCB_SHCSR/SCB_CFSR exist
  (67/69), block prefix, BFARVALID mask position, no lines; C7 DEMCR 0xE000EDFC (not ED20/ED2C), bp on a
  source line, halts not ordered; C8 not per halt, no pc/sp/lr/xpsr/frame, no precise proof, print of
  uint8_t; C9 "or", Terminate ≠ halt.
Bench Map v123.

### 3.4 composition round 2 — session 20, 2026-11-07 22:26
Accepted: C0 delete .c line 161; C3 SCB_SHCSR_BUSFAULTENA_Msk ORed into the line-157 RMW, dsb/isb
  158/159 serve it; C4 logic (reuse g_cfsr; BFARVALID = CFSR bit 15 since BFSR = CFSR 15:8; BFAR only
  when set, else g_bfar 0; park for(;;)); C5 slot 5 line 22 ".word BusFault_Handler /* 0x14 5 BusFault */"
  + .thumb_set form; C6 SCB_BFAR 0xE000ED38u (p.1466), SCB_CFSR_BFARVALID_Msk (1u << 15); C7 DEMCR
  0xE000EDFC (D1.2.37 p.1522), bp addresses from objdump.
Fix in round 3: C1 STAT = RW in the LPUART map (RM p.1862; R-only: VERID 0h, PARAM 4h, DATARO 30h) → RO
  sentence n/a; "can cause" hedge; clock lines 115–121 → 134–137; C2 "global or raw" (2 options),
  trigger line not placed; C4 g_bfar_valid writer missing, lines ignore own inserts; C5 weak pair
  115–116 = inside Default_Handler (113–118), .S header text/line missing; C6 BUSFAULTENA mask undefined
  (used in C3), header/*/ lines missing; C7 bp at handler entry (redundant with VC_BUSERR, before the
  work), store bp missing from halt order, stop-line types; C8 not per halt, HFSR dropped, $sp frame only
  at entry, precise proof = RA slot +0x18 vs objdump; C9 Terminate = IDE button at the park bp.
Bench Map v124.

### 3.4 composition round 3 — session 20, 2026-11-07 22:44
Accepted: C0; C1 target LPUART4 DATARO 0x500B_4030 (map 30h "R", RM 50.5.5.1 p.1862), hedge + no-fault
  meaning, clocks 134–137; C2 store 0xFFFFFFFF, no local, after the barriers; C3; C4 logic (CFSR bit 15;
  g_bfar/g_bfar_valid set in both branches; park); C5 slot 5 line 22; C6 BUSFAULTENA (1u<<17) p.1838,
  SCB_BFAR 0xE000ED38u p.1466, BFARVALID (1u<<15); C7 halt order store bp → SIGSTOP entry (VC_BUSERR) →
  park bp; C8 entry reads + RA slot +0x18 vs objdump, park reads.
Fix in round 4: C1 quoted sentence not the RM's wording (real p.1862 NOTE: "Writing to a read-only (RO)
  register or reading a write-only (WO) register can cause bus errors."), "Table 50-2" unopened, no-op =
  inference; C2 raw address in REG32 → LPUART4_DATARO macro; C4 handler source position (105–113 = LP
  handler), line numbers inconsistent → Select-String; C5 weak pair position, .S header text + */; C6
  header line 3 / */ 5 = comment closes early → compile error (3.4 line after the 3.3 line), mask comment
  + D1.2.11 p.1477, DATARO macro; C7 bp numbers #2/#3; C8 no halt-1 baseline reads, stop lines; C9 "or
  disconnect", IDE Terminate button.
Bench Map v125.

### Paused 2026-11-08 00:09 (session 20)
3.4 composition paused after round 3 (round-3 accepted items stand; round 4 owed: C1 exact RM quote +
  50.5.5.1 + no-op = inference; C2/C6 LPUART4_DATARO macro; C6 header after the 3.3 line, */ +1, mask
  comment + D1.2.11 p.1477; C4 handler source position; C5 weak-pair position + .S header text/*/;
  C7 bps #2/#3; C8 halt-1 baseline reads + stop lines; C9 IDE Terminate at bp #3).
Files not edited; image 3.3 (B) in flash; GDB-only state none.
Open: Terminate after the 21:19 launch check (halted at main) — not confirmed.
Resume: confirm Terminate + unplug → round 4 → edit .c/.S → saved files + Select-String table → diff.
Bench Map v126.

### Resumed 2026-11-09 16:36 (session 20)
Asked: Terminate after the 2026-11-07 21:19 launch check + unplug? Next: 3.4 composition round 4.
Bench Map v127.

### 3.4 composition accepted (rounds 1–4) — session 20, 2026-11-09 17:12
C0 delete .c line 161 (msr msplim). C1 target LPUART4 DATARO 0x500B_4030 (map 30h "R", RM 50.5.5.1
  p.1862); NOTE quoted exactly ("Writing to a read-only (RO) register or reading a write-only (WO)
  register can cause bus errors."); no-fault run = no-op (inference); clocks/resets before the trigger.
C2 REG32(LPUART4_DATARO) = 0xFFFFFFFF right after the isb; precise per TRM §1.4.1 PDF p.13. C3
  SCB_SHCSR_BUSFAULTENA_Msk ORed into the line-157 RMW; dsb/isb 158/159. C4 BusFault_Handler after
  UsageFault_Handler, before main: g_cfsr = CFSR; if CFSR bit 15 (BFARVALID; BFSR = CFSR 15:8, D1.2.11
  p.1477) → g_bfar = BFAR, g_bfar_valid 1, else 0 / 0; park for(;;). C5 slot 5 line 22; weak pair +
  .thumb_set. C6 header after the 3.3 line, */ +1; LPUART4_DATARO, SCB_SHCSR_BUSFAULTENA_Msk (1u<<17)
  p.1838, SCB_BFAR 0xE000ED38u p.1466, SCB_CFSR_BFARVALID_Msk (1u<<15). C7 bp #2 store → SIGSTOP at
  entry (VC_BUSERR) → bp #3 park. C8 halt-1 baseline (pc, CFSR, HFSR, SHCSR, MMFAR, BFAR); halt 2 pc sp lr
  xpsr + x/8xw $sp, RA +0x18 vs objdump; halt 3 SCS regs + globals + pc. C9 IDE Terminate at bp #3.
Carried into the file edit: g_bfar_valid writes restored (dropped in round 4); globals after g_cfsr;
  weak pair after the UsageFault pair + comment; .S header new " * 3.4 …" line, */ moved from line 6;
  DATARO comment "RM Table 50-2" → RM 50.5.5.1 p.1862 (2nd time); stop lines from the log ("Breakpoint
  N, …", "Program received signal SIGSTOP, Stopped (signal)."); halt-3 print/x $pc.
Errors fixed across rounds: wrong file names (main.c, vector.S); citation outside its scope (LPUART
  sentence for 0x400F_F000); RW STAT taken as RO; paraphrase in quotes; C local; "or" (4×); BFARVALID bit
  7 on a CFSR word; duplicate g_cfsr/SCB macros; weak pair inside Default_Handler; header at line 3 (comment
  closes early); DEMCR 0xE000ED20/2C; bp at handler entry; bp numbers; no halt-1 baseline.
Bench Map v128.

### 3.4 saved-file check (rounds 1–4) — session 20, 2026-11-09 17:17–19:16
Terminate after the 2026-11-07 21:19 launch check + unplug: CONFIRMED (user, 17:17).
Round 1 byte-identical to the session-start 3.3 (B) sources (md5 f0643f53… / 24f4bcbe…) → no edit
  applied (first send unchanged: 3.1c, 3.3 B, now 3.4). Round 2: SCB_BFAR = 0xE000ED28u (CFSR address),
  handler/trigger/RMW absent, msr commented out instead of deleted, slot 5 "Bus_Handler" (undefined),
  UsageFault weak pair duplicated, .S header unchanged. Round 3: RMW without BUSFAULTENA (2nd), handler =
  UsageFault copy, duplicate pair (2nd), BusFault pair comment "UsageFault" + .set, mask comment (2nd).
Round 4 ACCEPTED (.c 212 / .S 137; diff vs 3.3 (B) = agreed edits only): .c header 20 / */ 21; DATARO
  49; BUSFAULTENA_Msk 71, SCB_BFAR 0xE000ED38u 72, BFARVALID_Msk (1u<<15) 73 (D1.2.11 p.1477); g_cfsr 80,
  g_bfar 81, g_bfar_valid uint8_t 82; UsageFault_Handler 117–122; BusFault_Handler 125–140 (test 128,
  park 138–139); main 142; RMW 181; dsb 182, isb 183; trigger 185; SYST_RVR 189; Hello 193; for(;;) 210.
  .S header 7; slot 5 line 23; BusFault weak pair 134–136 (.thumb_set). Cosmetic: blank lines .c
  123–124, .S 132–133. Line table by grep of the sent files (user Select-String never sent, 4×).
Bench Map v129–v132.

### 3.4 build predictions — session 20, 2026-11-09 19:26
Predictions LOCKED (P1–P16). Lock flags: no value (P3, P4, P6, P7, P8, P10, P11, P14, P15, P16);
  "unchanged" without the 3.3 (B) value (P4, P6); ranges (P2, P5); "or" (P9, P12); P1 reason not a
  source; wrong file names "_2" (P4, P6, P8).
TUTOR CORRECTION (19:50): the Lock message said "no addendum" and "range can't score" — protocol:
  addendum accepted before running; ranges/symbolic = ungraded; "or" = first value graded. The build had
  already run at 19:48 → addendum chance lost through the tutor's error (grades stand).
Tool path (19:50): nm call failed (not on PATH); the tutor's command had a "Debug\" prefix and no $od
  although the rule was logged → corrected: $tb = …\tools\bin; $od / $nm = "$tb\arm-none-eabi-….exe";
  run from Debug without a path prefix. Bench Map v133–v134.

### 3.4 build — session 20, 2026-11-09 19:48 (graded 19:59)
Result (raw): Clean 0/0; FLASH 1028 B, RAMB 28 B; text 1023 data 4 bss 24. .text 0x31F @0xE0; .data LMA
  0x400; .bss 0x18 @0x2000_8004. Symbols: uart4_putc 0xE0, SysTick 0x114, LP_FLEXCOMM4 0x14C, UsageFault
  0x168, BusFault_Handler 0x180 (0x50 B), main 0x1D0, Reset 0x34C, Default 0x3A0, uart4_puts 0x3A4;
  g_cfsr 0x2000_800C, g_bfar 0x8010, tick.0 0x8014, g_bfar_valid 0x8018, __bss_end__ 0x801C.
  .isr_vector: word1 0x34D, Default 0x3A1, slot 5 0x181, 6 0x169, 15 0x115, 55 0x14D. BusFault_Handler:
  push {r7} 0x180; ldr CFSR 0x184–0x186 → str g_cfsr 0x18A; reload g_cfsr 0x18C; and.w r3,#0x8000 0x190,
  cmp 0x194, beq.n 0x196; BFAR ldr 0x198–0x19A → str g_bfar 0x19E; strb 1 0x1A4; else str 0 0x1AC, strb
  0 0x1B2; park nop 0x1B6 / b.n 0x1B8; literals 0x1BC–0x1CC. main: SHCSR orr.w #0x60000 0x26C, str 0x270;
  dsb 0x272; isb 0x276; ldr r3 ← 0x320 (0x500B_4030) 0x27A; mov.w r2,#0xFFFFFFFF 0x27C; str r2,[r3] 0x280;
  SYST_RVR 0x282; bl uart4_puts 0x292; join nop 0x2CC; for(;;) 0x2CE / b.n 0x2D0; pad nop 0x2D2; literals
  0x2D4–0x348. msr MSPLIM absent.
Learned: bss +8 (1 B global + ALIGN(4) end); --sort-section=alignment puts tick.0 between g_bfar and
  g_bfar_valid (inference); main +4 net + pool pad nop → code after main +0x54; 0xFFFFFFFF = mov.w
  modified immediate (4 B); masks merged to orr.w #0x60000; -O0 volatile re-read of g_cfsr.
Grades: P1 ✅ P2 ❌ P3 ½ P4 ✅ P5 ½ P6 ❌ P7 ❌ P8 — P9 ❌ P10 — P11 ½ P12 ❌ P13 ✅ P14 — P15 — P16 —.
  Score 4.5/11 graded. Board: bp #2 0x280 (trigger str), bp #3 0x1B6 (park nop). Bench Map v135.

### 3.4 board predictions — session 20, 2026-11-09 22:29–22:34
Predictions LOCKED (B1–B14), addendum 22:31 (B1/B3/B6 $pc 0x0000xxxx; B2 SHCSR 0x0002_0000; B4 $sp
  0x2000_FFCC; B5 {0,0,0,0,0, 0x0000xxxx, 0x0000xxxx, 0x0100_0000}; B9 SHCSR 0x000A_0000; B11 MMFAR 0;
  B14 0x10), B2 confirmed 22:34. Lock flags: 0x0000xxxx symbolic → ungraded (B1, B3, B6 pc; B5 LR + RA);
  B2 value vs own reason (both enables named, one bit in value).

### 3.4 board run 1 — session 20, 2026-11-09 22:46–22:50
Launch: "Temporary breakpoint 1, main () at ../source/frdmmcxn236_m33_scratch.c:144", pc 0x1D6; bp #2
  0x280 (line 185), bp #3 0x1B6 (line 138), "automatically using hardware breakpoints".
SLIP: reads shifted one halt. At main (0x1D6, before continue): CFSR 0, HFSR 0, SHCSR 0, MMFAR
  0x2000_FFFC, BFAR 0x2000_FFFC. At bp #2 "Breakpoint 2, 0x00000280 in main () at …:185": pc 0x280, sp
  0x2000_FFF0, lr 0x387, xpsr 0x2900_0000, x/8xw $sp: 0, 0x4040_0000, 0, 0x387, then 0x2001_0000+ 0.
  At "Program received signal SIGSTOP, Stopped (signal). BusFault_Handler () at …:126": pc 0x180, sp
  0x2000_FF88, SHCSR 0x0006_0002, CFSR 0x0000_8200, HFSR 0, MMFAR 0x500B_4030, BFAR 0x500B_4030, g_cfsr
  0, g_bfar 0, g_bfar_valid 0 (handler body not yet run); then (22:50, same halt) lr 0xFFFF_FFE9; xpsr
  0x2900_0005; x/8xw 0x2000_FF88: 0x404, 0x2000_801C, 0xFFFF_FFFF, 0x500B_4030, 0x4, 0x387, 0x280,
  0x2910_0000. continue → "Breakpoint 3, 0x000001b6 in BusFault_Handler () at …:138": pc 0x1B6, sp
  0x2000_FF84, SHCSR 0x0006_0002, CFSR 0x0000_8200, HFSR 0, MMFAR 0x500B_4030, BFAR 0x500B_4030, g_cfsr
  0x8200, g_bfar 0x500B_4030, g_bfar_valid 0x1. UART: no "Hello".
Learned: RO write to LPUART4 DATARO → precise BusFault (RM "can cause" happened); PRECISERR + BFARVALID;
  RA 0x280 = the str; handler took the BFARVALID branch; BUSFAULTENA → no escalation (HFSR 0); SHCSR
  BUSFAULTACT bit 1 (p.1842); 26-word frame (0xFFF0 − 0xFF88 = 0x68, FPCA 1), EXC_RETURN 0xFFFF_FFE9,
  IPSR 5; stacked R2/R3 = trigger value/address; R0 0x404 / R1 0x2000_801C = Reset_Handler leftovers
  (inference); stacked xPSR bit 20 = SFPA (RETPSR D1.2.221 p.1818, p.101). 3.0 open question SETTLED:
  MMFAR reads = BFAR in both states (0x2000_FFFC before, 0x500B_4030 after) with MMFARVALID 0 → shared
  (RXPWN/RGBKY p.89 IMP DEF; inference). Handler prologue push {r7} = 4 B.

### 3.4 board run 2 — session 20, 2026-11-09 22:55
Launch "Temporary breakpoint 3, main () at …:144": run-1 bps kept by the IDE (#1 0x1B6, #2 0x280;
  Breakpoints view not emptied). At "Breakpoint 2, 0x00000280 in main () at …:185": CFSR 0, HFSR 0,
  SHCSR 0x0006_0000, MMFAR 0x0000_0320, BFAR 0x0000_0320.
Learned: both enables set before the trigger (C3 confirmed). With BFARVALID 0 the shared FAR reads
  the address of a recent data access (0x320 = literal of ldr at 0x27A; 0x2000_FFFC at main) — D1.2.6
  UNKNOWN → inference, NEW OPEN QUESTION. Shows why BFAR is used only with BFARVALID.
Grades: B1 ❌ B2 ❌ (CFSR, HFSR right; SHCSR, MMFAR, BFAR wrong) B3 ❌ B4 ❌ B5 ❌ (RA ungraded) B6 ❌
  B7 ✅ B8 ✅ B9 ❌ B10 ✅ B11 ❌ B12 ✅ B13 ✅ B14 ❌. Final 3.4 board 5/14.
Board: Terminate (halted at 0x280, run 2) + unplug CONFIRMED (user, 23:32). 3.4 CLOSED. Bench Map v138.

### Session 20 end (2026-11-09 23:32)
Session ended at the user's request after 3.4; continuation in a new chat (session 21). Image 3.4 in
  flash (.c 212 / .S 137 / .ld 45): faults on every run before "Hello" (RO store to LPUART4 DATARO at
  0x280 → precise BusFault → BusFault_Handler records CFSR/BFAR → parks at 0x1B6). GDB-only state none.
  Breakpoints view may still hold 0x1B6 / 0x280 — Remove All at the next launch. Next: session-21
  intake → launch check (pc 0x1D6, x/3i 0x27A) → 3.5 MemManage via MPU (proposed).

### Session 20 summary (2026-11-07 / 2026-11-09)
Scores: 3.4 build 4.5/11 (5 ungraded), board 5/14. Compositions: 4 rounds + 4 saved-file rounds.
Closed: real precise BusFault (RO write to LPUART4 DATARO, RA = faulting str 0x280); handler checks
  BFARVALID (CFSR bit 15) before using BFAR; 3.0 open question MMFAR = BFAR (shared; inference);
  Terminate after the 11-07 launch check and after the 3.4 runs (confirmed).
New facts: BUSFAULTACT bit 1 (p.1842); stacked xPSR bit 20 = SFPA (D1.2.221 p.1818); bss +8 for a
  1-byte global (ALIGN(4)); 0xFFFFFFFF via mov.w modified immediate; LPUART map: STAT RW, DATARO R (30h).
Tutor corrections: 19:26 Lock "no addendum"/"range can't score" (protocol: addendum before run; ranges
  ungraded); 19:50 tool command ignored the logged $od rule.
Open: FAR content with VALID 0 tracks a recent access address; TRM Table 3-3 "UNALIGN_TRP fixed at 1"
  vs measured.
Weak points: paraphrase in quotes (C1); citation outside its scope; RW register taken as RO; saved files
  ≠ agreed edits (4 sends; byte-identical first send again); Select-String never sent; reads shifted one
  halt; stop-line texts invented ("Thread 1 hit", SIGTRAP); handler prologue/stack size guessed; frame
  size (8 vs 26) and EXC_RETURN (FFF9 vs FFE9) not carried from 3.3; SHCSR value vs own reason.
Strengths: BFARVALID = CFSR bit 15 decode; CFSR 0x8200, BFAR, globals, HFSR 0, no "Hello" all right;
  RA-slot precise-proof method; P4 placement (functions before the insert unchanged).

### Session 21 intake (2026-11-10 09:37)
learning_log.md (3,103 lines, 198 "###" headings, no duplicates) ends with the session-20 summary
  exactly once (line 3087). session20_log_blocks.md (209 lines, 16 blocks): each heading once in the
  log; whole file present verbatim once, contiguous at the end (from line 2895) → nothing missing or
  duplicated. Saved sources = continuation prompt: .c 212 (header 3.4 20 / */ 21; DATARO 49; macros
  67–73; g_cfsr 80, g_bfar 81, g_bfar_valid 82; UsageFault_Handler 117–122; BusFault_Handler 125–140;
  CCR 180, SHCSR RMW 181, dsb 182, isb 183, trigger 185, SYST_RVR 189, Hello 193); .S 137 (header 1–7;
  slot 5 line 23, slot 6 24; Default_Handler 114–119; BusFault pair 134–136); .ld 45 (wc). md5 .c
  4f9d9bcb…, .S 96a5989c…, .ld 83f60f7f…. Bench Map read in full (footer v139 vs prompt v138 —
  numbering only) → v140; 3.4 row added to Phase 3 (missing). Asked: Breakpoints view EMPTY?
  Next: launch check ("Temporary breakpoint 1", pc 0x1D6, x/3i 0x27A) → 3.5 citations → composition.

### Session 21 launch check + 3.5 issued — 2026-11-10 10:12
Launch: "Temporary breakpoint 1, main () at ../source/frdmmcxn236_m33_scratch.c:144"; info breakpoints
  none; $2 = 0x1d6; x/3i 0x27A = ldr r3,[pc,#164] (0x320) / mov.w r2,#4294967295 / str r2,[r3,#0]
  → image 3.4, no rebuild.
3.5 sources opened (pdftotext): MMFAR D1.2.166 p.1739 (0xE000ED34, banked); MMFSR D1.2.167 p.1740–1742
  (MMFARVALID 7, MLSPERR 5, MSTKERR 4, MUNSTKERR 3, DACCVIOL 1 + MMFAR update, IACCVIOL 0); MPU_CTRL
  D1.2.168 p.1743–1744 (0xE000ED94; PRIVDEFENA 2, HFNMIENA 1, ENABLE 0); MPU_MAIR0 D1.2.169 p.1745
  (0xE000EDC0); MAIR_ATTR D1.2.165 p.1737–1738; MPU_RBAR D1.2.171 p.1747–1748 (0xE000ED9C); MPU_RLAR
  D1.2.173 p.1751–1752 (0xE000EDA0); MPU_RNR D1.2.175 p.1755 (0xE000ED98); MPU_TYPE D1.2.176 p.1756
  (0xE000ED90, DREGION 15:8); B10.1 p.281–283 (RJVCN, IGLXD, RZLHD, RDNXT, RHBBJ, RVCTC, RLLLP, RRHSF,
  RKMTF, RMKJC); B3.12 RXMRH p.88; B3.11 p.85 (MemManage banked); RVTTN p.317 (DAP not MPU-checked);
  MEMFAULTENA 16 p.1838, MEMFAULTACT 0 p.1842; VC_MMERR 4 p.1528; TRM §4.1 PDF p.44 + Table 4-3
  PDF p.45–46; RM 2.5.1 Table 4 p.23 ("eight unified protection regions").
New doc discrepancy: RM Table 4 p.23 "MPU mismatches and permission violations invoke the HardFault
  handler" vs TRM §4.1 PDF p.44 "invoke the MemManage handler" (D1.2.233 p.1838: HardFault only when
  MEMFAULTENA 0).
Composition C0–C10 issued. Asked: Terminate after the launch check? Bench Map v141.

### 3.5 composition round 1 — session 21, 2026-11-10 10:52
Accepted: C0 delete .c line 185, keep BusFault_Handler / BUSFAULTENA / .S slot 5 + pair; C2 32-bit
  store, privileged (log 1b.4b CONTROL_S 0xC nPRIV 0), RLLLP bullet 2; C3 RNR 0, BASE 0x0100_0400,
  SH 00, AP 0b10, XN 1; C6 MEMFAULTENA (1u<<16) p.1838 in the line-181 RMW; C7 .c header text;
  C9 slot 4 line 22 text.
Fix in round 2: C1/C3/C8 block 0x2000_8000–801F also holds g_init, irq count, g_zero, g_cfsr,
  g_bfar, g_bfar_valid → handler's g_cfsr store lands in the RO region (RLLLP p.282 → RGNVS p.143);
  new globals move the .bss map (3.4 address stale); tick.0 not 32-byte aligned; C3 LIMIT 0x0100_0401
  = 0x2000_803F, PXN RES0 on v8.0-M (p.1752), pages 1747/1748/1751/1752, RBAR/RLAR words missing;
  C4 0x44 = Non-cacheable vs "Write-Back" label, p.1745; C5 PRIVDEFENA 0 → stack/RAM/peripherals in
  no region (RLLLP bullet 3), "or", region 1 inside C5 with 2 AP values, HFNMIENA unjustified, no
  PRIVDEFENA row/CTRL word; C2 raw pointer (decision A), p.281 → 282; C7 MPU_* prefix, SCB_MMFAR
  missing, pages, mask needs D1.2.11 p.1477; lines 266/48/184/86–92/99–106 wrong (addresses vs
  lines); DATARO "Table 50-2" (3rd time); C8 g_cfsr store + else branch; C9 pair position + .S
  header; C10 bp numbers, stop lines, park read list, Terminate method.
Bench Map v142.

### 3.5 composition round 2 — session 21, 2026-11-10 11:32
Terminate after the 10:12 launch check: CONFIRMED (user).
Accepted: C0 (RMW line 181, DATARO line 49 RM 50.5.5.1 p.1863); C3 RBAR 0x2000_8045 / RLAR 0x2000_8041
  (BASE = LIMIT field 0x0100_0402, AP 0b10, XN 1, PXN 0 RES0, AttrIndx 0, EN 1) — conditional on a
  0x2000_8040–805F target block; C4 Attr0 0x44 Normal Non-cacheable; C6; C7 MPU_CTRL/RNR/RBAR/RLAR/
  MAIR0 + SCB_MMFAR + MMFARVALID mask (D1.2.11 p.1477), header after line 20; C10 read lists +
  IDE Terminate at bp #3.
Fix in round 3: C1 4-byte .bss global cannot be assumed at 0x2000_8040 (alignment-sort + source
  order → next to g_cfsr/g_bfar) — mechanism with source, or an address no object owns; C5 unchanged
  (PRIVDEFENA 0 with only region 0, regions named that don't exist, HFNMIENA reason, no PRIVDEFENA
  row / CTRL word); asm ("dsb" : : "memory") = clobber in the input slot (file: :::); C2 "via macro"
  vs pointer cast; C7 LPUART4_DATARO duplicate (line 49); C8 g_cfsr duplicate (line 80), store not
  stated, post-edit g_cfsr address, "= 0 → .bss" = inference; C9 line 7 must lose */, " * " prefix,
  pair comment line; C10 stop lines (2nd), trigger bp #2.
Bench Map v143.

### 3.5 composition round 3 — session 21, 2026-11-10 11:54
Accepted: C0; C1 raw unowned RAMB address 0x2000_8040 via MPU_TARGET_ADDR (RAMB .ld line 10; above
  .bss, below stack min 0x2000_FF84 measured 3.4); C2 REG32(MPU_TARGET_ADDR) = 0xDEADBEEFu after the
  MPU barriers, privileged, RLLLP bullet 2 p.282; C3 RNR 0, RBAR 0x2000_8045, RLAR 0x2000_8041,
  after lines 181–183; C4 Attr0 0x44; C5 PRIVDEFENA 1 + access classes (background / RVCTC / RHBBJ),
  order RNR→RBAR→RLAR→MAIR0→CTRL, dsb/isb ::: form; C6; C7; C8 (g_cfsr reused, both branches,
  g_mmfar / g_mmfar_valid; .bss = inference); C9 slot 4 line 22 + pair with comment; C10 halts,
  stop lines, read lists, IDE Terminate at bp #3.
Owed (round 4): C1 post-edit __bss_end__ as a number; C5 HFNMIENA reason tied to the step (3rd,
  RRHSF p.282) + composed MPU_CTRL word.
Carried into the file edit: C3 LIMIT p.1511 → p.1751 (fields p.1752); C7 MPU_TARGET_ADDR line +
  comment (IGLXD p.281); C9 .S header " * " prefix, one */ position.
Bench Map v144.

### 3.5 composition round 4 — session 21, 2026-11-10 12:14
C1 ACCEPTED: post-edit __bss_end__ ≤ 0x2000_8024 < 0x2000_8040 in any placement → target unowned.
  Flag: arithmetic added onto 0x801C, which includes the 3 pad bytes after g_bfar_valid (0x8018) —
  redo per object for the build predictions.
C5: MPU_CTRL 0x0000_0007 back-decodes to the stated bits. HFNMIENA still open (4th): reason
  (handlers at priority 0 stay MPU-checked) is true for any HFNMIENA value — the bit acts only at
  negative requested priority (RRHSF p.282; D1.2.168 p.1744); back-decode "MPU checking during
  exception handlers" wrong. Owed: one value + reason naming its effect in 3.5, and the CTRL word.
Bench Map v145.

### 3.5 composition accepted (rounds 1–5) — session 21, 2026-11-10 13:59
C0 delete .c line 185 (3.4 trigger); keep BusFault_Handler, BUSFAULTENA, DATARO line 49 (RM 50.5.5.1
  p.1863), .S slot 5 + pair. C1 MPU_TARGET_ADDR 0x2000_8040 (unowned RAMB: post-edit __bss_end__ ≤
  0x2000_8024, stack min 0x2000_FF84). C2 REG32(MPU_TARGET_ADDR) = 0xDEADBEEFu after the MPU
  barriers; privileged (1b.4b); RLLLP bullet 2 p.282. C3 RNR 0, RBAR 0x2000_8045 (BASE 0x0100_0402,
  SH 00, AP 0b10, XN 1), RLAR 0x2000_8041 (LIMIT → 0x2000_805F, PXN 0 RES0, AttrIndx 0, EN 1). C4
  MAIR0 Attr0 0x44 Normal Non-cacheable. C5 MPU_CTRL 0x0000_0005 (PRIVDEFENA 1, HFNMIENA 0 — no
  effect at priority 0 without FAULTMASK, RRHSF p.282 / D1.2.168 p.1744; ENABLE 1); order
  RNR→RBAR→RLAR→MAIR0→CTRL, then dsb/isb (::: form). C6 MEMFAULTENA (1u<<16) in the line-181 RMW.
  C7 MPU_* / SCB_MMFAR / masks / MPU_TARGET_ADDR; .c header after line 20. C8 MemManage_Handler
  after BusFault_Handler: g_cfsr = CFSR; MMFARVALID (CFSR bit 7) → g_mmfar/g_mmfar_valid, else 0/0;
  park. C9 .S header line 8 + */, slot 4 MemManage_Handler, pair after BusFault pair. C10 #1 temp
  → bp #2 trigger → SIGSTOP (VC_MMERR) → bp #3 park; read lists; IDE Terminate at #3.
Errors fixed across rounds: target block shared with g_cfsr/g_bfar/g_init (handler self-fault);
  PRIVDEFENA 0 with one region; LIMIT 0x0100_0401 (= 803F); PXN on v8.0-M; 0x44 mislabelled
  Write-Back; SCB_MPU_* prefix; wrong pages (1477/1478/1511/1512/1475); "or" (2×); asm clobber in
  the input slot; duplicate DATARO / g_cfsr; "Table 50-2" (3rd); HFNMIENA reason (4 rounds);
  addresses used as line numbers.
Bench Map v146.

### 3.5 saved-file check rounds 1–2 — session 21, 2026-11-10 14:05–14:22
TUTOR CORRECTION (14:05): the 13:59 Select-String commands used .\source\… and .\startup\… paths;
  both files are in source\ (log: "../source/startup_m33.S:118") → run from source\ with
  .\frdmmcxn236_m33_scratch.c and .\startup_m33.S.
Round 1 (14:06, user Select-String, first ever paste): .c no match for 3\.5:/MPU_/SCB_MMFAR/
  MEMFAULTENA/MMFARVALID/g_mmfar/MemManage_Handler/DEADBEEF; trigger DATARO still line 185; main 142;
  .S line 7 ends */, slot 4 line 22 Default_Handler, BusFault pair last → files on disk = 3.4
  sources, no edit applied (same pattern as session 20 first send). .S pattern '\*/' matched every
  vector comment → replaced by '^ \* 3\.' / '^\s*\*/'.
Round 2 (14:22; files; diff vs 3.4): .S correct — header line 7 / 8 "3.5: MemManage via the MPU*/",
  slot 4 line 23 MemManage_Handler, pair 139–141 with comment; no newline at EOF. .c correct: macros
  76–84, g_mmfar 94 / g_mmfar_valid 95 (g_cfsr not redefined), MemManage_Handler 155–170, MPU writes
  215–219 (RNR 0, RBAR 0x2000_8045, RLAR 0x2000_8041, MAIR0 0x44, CTRL 0x5), DATARO 50.
  Wrong: header 21 "… after line 20." (instruction text copied); RMW 211 lacks MEMFAULTENA (edit 5);
  no dsb/isb after MPU_CTRL (edit 7); line 222 stray "." → compile error; line 224 trigger
  commented out instead of deleted (2nd session); MPU writes without decode comments (edit 6).
Bench Map v147.

### 3.5 saved-file check round 3 — session 21, 2026-11-10 14:29 (ACCEPTED)
User Select-String: header 21 "3.5: MemManage via the MPU"; DATARO macro 50 only (3.4 trigger deleted);
  MPU_* 76–80, MEMFAULTENA_Msk 82, MPU_TARGET_ADDR 84; MemManage_Handler 155; main 172; RMW 211 with
  USGFAULTENA|BUSFAULTENA|MEMFAULTENA; dsb 212 / isb 213; MPU RNR 215, RBAR 216, RLAR 217, MAIR0 218,
  CTRL 219; dsb 221 / isb 222; trigger 224 REG32(MPU_TARGET_ADDR) = 0xDEADBEEFu.
Cosmetic (not blocking): MPU writes without decode comments; .S EOF newline unconfirmed. Lines outside
  the pattern verified by the 14:22 diff.
First session-20/21 send where the user's own Select-String paste = agreed edits.
Build predictions P1–P14 issued. Bench Map v148.

### 3.5 sources frozen — session 21, 2026-11-10 14:49
Files sent; diff vs 14:22 = header 21 fixed, RMW 211 + MEMFAULTENA, decode comments 215–219, dsb 221 /
  isb 222, stray "." gone (224), commented 3.4 trigger deleted. Diff vs 3.4 (B) = agreed edits only.
  .c 252 lines md5 2a3ecfb0…; .S 142 lines md5 7ecc766b… (line 141 now ends CRLF; line 142 = 4 spaces,
  no EOF newline). Sources frozen for 3.5. Awaiting predictions P1–P14.

### 3.5 build predictions — session 21, 2026-11-10 14:58 (lock check)
P1 0/0; P2 0x44B / 0x4 / 0x20; P3 0x2000_8020; P4 g_cfsr 0x8008, g_bfar 0x800C, g_mmfar 0x8010, tick.0
  0x8014, g_bfar_valid 0x8018, g_mmfar_valid 0x8019; P5 0xE0 / 0x114 / 0x14C / 0x168 / 0x180; P6
  0x1D0, 0x50 B; P7 0x220; P8 0x39C / 0x3F0; P9 0x1D1; P10 and.w r3,#0x80; P11 #0x70000; P12 ldr, ldr,
  movs, movs; P13 mov.w, ldr, str; P14 bp #2 0x2A8, bp #3 0x210.
Lock flags: P2 bss vs P3 end inconsistent; P2 text +0x4C < P6 handler 0x50 + MPU code; P7/P8 shift
  ignores main growth claimed in P14; P4 no per-object derivation; P1/P12/P13 reasons not sources;
  P13 operand mapping unclear; file names "_3" (session 20: "_2") and lines 180/184 (= 211/224).
Addendum allowed before the build. Build commands issued (size, -h, nm -n -S, -s .isr_vector,
  --disassemble MemManage_Handler / main); $sz = "$tb\arm-none-eabi-size.exe" added.

### 3.5 build predictions LOCKED — session 21, 2026-11-10 15:12 (addendum accepted)
P1 0/0; P2 text 1139 / data 4 / bss 32; P3 __bss_end__ 0x2000_8024; P4 g_cfsr 0x800C, g_bfar 0x8010,
  tick.0 0x8014, g_bfar_valid 0x8018, g_mmfar 0x801C, g_mmfar_valid 0x8020; P5 0xE0 / 0x114 / 0x14C /
  0x168 / 0x180 (3.4); P6 MemManage_Handler 0x1D0, 0x50 B; P7 main 0x220; P8 Reset 0x3AC, Default
  0x400; P9 slot 4 0x1D1; P10 and.w r3,#0x80; P11 orr.w #0x70000; P12 ldr literal (RBAR, RLAR),
  mov.w (0x44, 0x5; "or movs" ungraded); P13 mov.w ← 0xDEADBEEF, ldr ← 0x2000_8040, str; P14 bp #2
  0x2A8, bp #3 0x210.
Lock flags (recorded, grades stand): P4 value vs own rule (1-byte before 4-byte); P2 +0x74 vs P8
  +0x60; P12 "or" → mov.w graded; P9 line 22 → 23; P14 "estimated" = graded.

### 3.5 build — session 21, 2026-11-10 15:12 (graded 15:13)
Result: 0/0; FLASH 1176 B, RAMB 32 B; text 1171 data 4 bss 28. .text 0x3B3 @0xE0; .data LMA 0x494;
  .bss 0x1C @0x2000_8004. Symbols: uart4_putc 0xE0, SysTick 0x114, LP_FLEXCOMM4 0x14C, UsageFault
  0x168, BusFault 0x180 (0x50), MemManage_Handler 0x1D0 (0x50), main 0x220 (0x1C0), Reset 0x3E0,
  Default 0x434, uart4_puts 0x438; g_cfsr 0x800C, g_bfar 0x8010, g_mmfar 0x8014, tick.0 0x8018,
  g_bfar_valid 0x801C, g_mmfar_valid 0x801D, __bss_end__ 0x8020. .isr_vector slot 4 0x1D1, 5 0x181,
  6 0x169, Default 0x435. MemManage_Handler: push {r7}; CFSR → g_cfsr 0x1DA; and.w r3,#0x80 0x1E0;
  MMFAR ldr 0x1E8–0x1EA → str g_mmfar 0x1EE; strb 1 0x1F4; else 0x1F8–0x202; park nop 0x206 / b.n
  0x208. main: orr.w #0x70000 0x2BC; dsb 0x2C2 isb 0x2C6; RNR movs #0 0x2CC; RBAR ldr ← 0x2000_8045
  0x2D2; RLAR ldr ← 0x2000_8041 0x2D8; MAIR0 movs #0x44 0x2DE; CTRL movs #5 0x2E4; dsb 0x2E8 isb
  0x2EC; ldr r3 ← 0x2000_8040 0x2F0; ldr r2 ← 0xDEADBEEF 0x2F2; str r2,[r3] 0x2F4.
Learned: 4-aligned group in source order (g_mmfar before tick.0), 1-byte objects after, ALIGN(4) →
  bss 0x1C (3.4 pad absorbed g_mmfar_valid); 0xDEADBEEF not a modified immediate → literal;
  small immediates → movs; main +0x44 (5×6 + 8 − 2 + 32); text +0x94 → code after main +0x94.
Grades: P1 ✅ P2 ❌ P3 ❌ P4 ❌ P5 ✅ P6 ✅ P7 ✅ P8 ❌ P9 ✅ P10 ✅ P11 ✅ P12 ½ P13 ½ P14 ❌ → 8/14.
  Board: bp #2 0x2F4 (trigger str), bp #3 0x206 (park nop). Board predictions B1–B15 issued.
  Bench Map v149.

### 3.5 board predictions — session 21, 2026-11-10 15:45 (lock check)
B1 pc 0x220; B2 MPU_TYPE 0x800, MPU_CTRL 0, CFSR 0, HFSR 0, SHCSR 0x0001_0000, MMFAR 0x2000_FFFC,
  BFAR 0x2000_FFFC; B3 r2 0x2000_8040, r3 0xDEADBEEF; B4 0x1D0 MemManage_Handler; B5 sp 0x2000_FF88;
  B6 lr 0xFFFF_FFE9; B7 xpsr 0x0900_0034; B8 {0xDEADBEEF, 0x2000_8018, 0x500B_4030, 0x1, 0x4, 0x387,
  0x2F4, 0x0100_0000}; B9 CFSR 0x82; B10 MMFAR 0x2000_8040, BFAR 0x2000_8040; B11 SHCSR 0x0001_0001,
  HFSR 0; B12 g_cfsr 0x82, g_mmfar 0x2000_8040, g_mmfar_valid 1; B13 MPU_CTRL 5, sp 0x2000_FF84;
  B14 x/xw 0x20008040 = 0; B15 no "Hello".
Lock flags: B2 reason vs bp #2 after CTRL 0x2E4 + barriers; B2/B11 SHCSR vs own P11 #0x70000; B2
  source D1.2.8 = BPIALL (HFSR D1.2.123 p.1671); B3 vs objdump 0x2F0/0x2F2; B6 reason "non-float"
  vs own B5 FP frame; B7 IPSR 0x34 vs reason "4"; B8 R2/R3 vs own B3 + stacked xPSR vs 3.4 SFPA
  bit 20; B1 symbol vs stop pc (3.4: 0x1D0 / 0x1D6); file names "_3" (2nd this session).
Addendum allowed before Debug. Bench Map v150.

### 3.5 board predictions LOCKED — session 21, 2026-11-10 16:15 (addendum accepted)
B1 pc 0x1D6; B2 MPU_TYPE 0x800, MPU_CTRL 5, CFSR 0, HFSR 0, SHCSR 0x0007_0001, MMFAR 0x2000_FFFC, BFAR
  0x2000_FFFC; B3 r2 0xDEADBEEF, r3 0x2000_8040; B4 0x1D0 MemManage_Handler; B5 sp 0x2000_FF88; B6
  lr 0xFFFF_FFE9; B7 xpsr 0x2910_0004; B8 {0xDEADBEEF, 0x2000_8040, 0x2F4, 0, 0x4, 0x387, 0x2F4,
  0x2910_0000}; B9 CFSR 0x82; B10 MMFAR 0x2000_8040, BFAR 0x2000_8040; B11 SHCSR 0x0007_0001, HFSR 0;
  B12 g_cfsr 0x82, g_mmfar 0x2000_8040, g_mmfar_valid 1; B13 MPU_CTRL 5, sp 0x2000_FF84 (15:45); B14
  0x20008040 = 0 (15:45); B15 no "Hello" (15:45).
Lock flags (recorded): B1 3.4 stop pc vs 3.5 main 0x220; B2 MEMFAULTACT set before trigger vs reason;
  B6 reason self-contradictory (FType 0 = extended); B7 bit 20 in live xPSR vs 3.4 entry 0x2900_0005;
  B8 value/address in R0/R1 slots vs own reason R2/R3.

### 3.5 board run — session 21, 2026-11-10 16:23 (graded)
Launch: "Temporary breakpoint 1, main () at ../source/frdmmcxn236_m33_scratch.c:174", pc 0x226;
  bp #2 0x2F4 (line 224), bp #3 0x206 (line 168), "automatically using hardware breakpoints".
At "Breakpoint 2, 0x000002f4 in main () at …:224": MPU_TYPE 0x0000_0800, MPU_CTRL 5, CFSR 0, HFSR 0,
  SHCSR 0x0007_0000, MMFAR 0x0000_03B4, BFAR 0x0000_03B4, r2 0xDEADBEEF, r3 0x2000_8040.
At "Program received signal SIGSTOP, Stopped (signal). MemManage_Handler () at …:156": pc 0x1D0, sp
  0x2000_FF88, lr 0xFFFF_FFE9, xpsr 0x2900_0004; x/8xw $sp: 0x498, 0x2000_8020, 0xDEADBEEF,
  0x2000_8040, 0x4, 0x41B, 0x2F4, 0x2910_0000; CFSR 0x82, MMFAR 0x2000_8040, BFAR 0x2000_8040, SHCSR
  0x0007_0001, HFSR 0.
At "Breakpoint 3, 0x00000206 in MemManage_Handler () at …:168": g_cfsr 0x82, g_mmfar 0x2000_8040,
  g_mmfar_valid 0x1, MPU_CTRL 5, sp 0x2000_FF84, [0x2000_8040] 0. UART empty.
Learned: privileged store to an AP=RO region → precise MemManage (DACCVIOL + MMFARVALID; RA 0x2F4 =
  the str; RLLLP p.282); MEMFAULTENA → HFSR 0; MEMFAULTACT bit 0 at entry; MPU_TYPE DREGION 8 (Secure,
  measured; RM Table 4 "eight" confirmed); MMFAR = BFAR with BFARVALID 0 → shared (2nd sample;
  inference); VALID-0 FAR = last literal address (0x3B4 ← ldr 0x2F2; 3.4 0x320 ← ldr 0x27A; 2
  samples, inference); stacked R0/R1 = Reset_Handler leftovers (__data_load__ end 0x498,
  __bss_end__ 0x2000_8020; 3.4 0x404 / 0x801C) — inference strengthened; stacked LR 0x41B = Reset
  bl-main return (+0x3A, as 3.4); live xPSR no bit 20, stacked RETPSR SFPA bit 20 (as 3.4); frame
  size and EXC_RETURN carried from 3.4 (session-20 weak point fixed).
Open: [0x2000_8040] = 0 does not prove suppression (unowned RAM, no pre-trigger read).
Grades: B1 ❌ B2 ½ B3 ✅ B4 ✅ B5 ✅ B6 ✅ B7 ❌ B8 ❌ B9 ✅ B10 ✅ B11 ✅ B12 ✅ B13 ✅ B14 ✅ B15 ✅
  → 11.5/15. Asked: Terminate at bp #3 + unplug? Next: (a) suppression re-check / (b)–(d) optional /
  (e) Phase 4. Bench Map v151.

### 3.5a issued — session 21, 2026-11-10 16:42
User choice: (a) suppression re-check, then (e) Phase 4.
3.5a (same image, no rebuild): at bp #2 GDB writes 0x12345678 to 0x2000_8040 (DAP not MPU-checked,
  RVTTN p.317), continue → bp #3 reads the target: pattern = store suppressed, 0xDEADBEEF = written.
  GDB-only state: one write. Predictions A1–A4 asked (A1 pre-write value, A2 readback, A3 at bp #3,
  A4 CFSR/MMFAR at bp #3).
Terminate at bp #3 after the 16:23 run CONFIRMED (user, 16:44). Breakpoints → Remove All before
  relaunch; unplug before A1 (power-off premise).

### 3.5a predictions — session 21, 2026-11-10 16:48 (lock check)
A1 0x0000_0000; A2 0xCAFEBABE; A3 0xCAFEBABE; A4 CFSR 0x82, MMFAR 0x2000_8040.
Lock flags: A2/A3 pattern 0xCAFEBABE vs issued set command 0x12345678 → user to pick (i) CAFEBABE
  command or (ii) addendum 0x12345678; A1 reason circular (assumes suppression under test) and
  "zeroed upon launch" unsourced (SRAM power-up content not in any opened doc).

### 3.5a predictions LOCKED — session 21, 2026-11-10 16:49 (addendum accepted)
A1 0x0000_0000 ("or undefined" ungraded; SRAM power-up content not in any opened doc); A2 0x1234_5678
  (DAP write, RVTTN p.317); A3 0x1234_5678 (store suppressed); A4 CFSR 0x82, MMFAR 0x2000_8040.

### 3.5a store-suppression check — session 21, 2026-11-10 16:52 (graded)
Same 3.5 image, no rebuild. Launch "Temporary breakpoint 1, main () at …:174"; bp #2 0x2F4 (224),
  bp #3 0x206 (168). At "Breakpoint 2, 0x000002f4 in main () at …:224": x/xw 0x20008040 = 0;
  set {unsigned int}0x20008040 = 0x12345678; readback 0x1234_5678. "Program received signal SIGSTOP,
  Stopped (signal). MemManage_Handler () at …:156" → continue. At "Breakpoint 3, 0x00000206 in
  MemManage_Handler () at …:168": [0x2000_8040] 0x1234_5678, CFSR 0x82, MMFAR 0x2000_8040.
SETTLED: MPU-faulting store did not write memory (pattern survived; same precise DACCVIOL fault).
  DAP write with region 0 AP=RO succeeded (RVTTN p.317). A1 = 0 after unplug (2nd sample; power-up
  content still undefined by docs). GDB-only state: one write (lost at power-off).
Grades: A1 ✅ A2 ✅ A3 ✅ A4 ✅ → 4/4. Phase 3 core complete (optional (b)–(d) remain listed).
Bench Map v152.

### Session 21 end (2026-11-10 16:54)
Session ended at the user's request after 3.5a; continuation in a new chat (session 22). Terminate at
  bp #3 after the 3.5a run + board unplugged: CONFIRMED (user, 16:54). Image 3.5 in flash (.c 252 /
  .S 142 / .ld 45): faults on every run before "Hello" (privileged store to MPU region 0 AP=RO at
  0x2F4 → precise MemManage → MemManage_Handler records CFSR/MMFAR → parks at 0x206). GDB-only state
  none (the 3.5a write is lost at power-off). Breakpoints view may still hold 0x2F4 / 0x206 — Remove
  All at the next launch. User choice for next: Phase 4 — per the phase plan, Phase 3 closes first
  with the 5-question self-test + short GDB check + Field Guide Phase 3 sections, then 4.0
  read-only audit (CONTROL_S/NS D1.2.13 p.1480, MSP/PSP/limits, IPSR D1.2.142 p.1707, SHPR2 D1.2.235
  p.1846). Bench Map v153.

### Session 21 summary (2026-11-10)
Scores: 3.5 build 8/14, board 11.5/15, 3.5a 4/4. Compositions: 5 rounds + 3 saved-file rounds.
Closed: real precise MemManage via the Secure MPU (region 0 0x2000_8040–805F privileged RO; RA = the
  faulting str 0x2F4); handler checks MMFARVALID (CFSR bit 7) before using MMFAR; MPU_TYPE DREGION 8
  (measured); MMFAR = BFAR shared (2nd sample, BFARVALID 0); the MPU-faulting store does not write
  memory (3.5a); DAP write passes an RO MPU region (RVTTN p.317 confirmed on hardware); Terminate
  after every run of the session (confirmed). Phase 3 core goals met.
New facts: MemManage vector catch (VC_MMERR → SIGSTOP at entry); MEMFAULTACT bit 0; HFNMIENA acts
  only at negative requested priority (RRHSF p.282); PRIVDEFENA 1 gives privileged code the
  background map; PXN RES0 on Armv8.0-M; MAIR 0x44 = Normal Non-cacheable; 0xDEADBEEF needs a
  literal (not a modified immediate); small immediates → movs; 1-byte .bss objects fill the old pad.
Tutor corrections: 14:05 Select-String paths (.\source\, .\startup\) wrong — both files in source\.
Open: VALID-0 FAR content = last literal address (2 samples, inference); RM Table 4 p.23 "HardFault
  handler" vs TRM p.44 "MemManage handler" (doc discrepancy); TRM Table 3-3 UNALIGN_TRP "fixed at 1"
  vs measured (still open).
Weak points: block of the MPU region shared with live globals (round 1); PRIVDEFENA 0 unanalysed (2
  rounds); HFNMIENA reason 4 rounds; first saved-file send = unedited sources again (3rd session in a
  row with a first-send problem); "_3" file names (2×); copied stop pc from 3.4 (B1); live vs stacked
  SFPA (B7); frame slot order (B8); P4 value vs own rule.
Strengths: own Select-String paste matched the edits (first time); register words RBAR/RLAR
  correct from round 2; MPU_TYPE / CFSR / MMFAR / globals / sp / EXC_RETURN all right on the board;
  frame size and EXC_RETURN carried from 3.4; 3.5a 4/4; addenda fixed flagged inconsistencies.

### Session 22 intake (2026-11-10 18:44)
learning_log.md (3,396 lines, 220 "###" headings, no duplicates) ends with the session-21 summary
  exactly once (line 3375). session21_log_blocks.md (292 lines, 22 blocks): each heading once in the
  log; whole file present verbatim once, contiguous at the end (from line 3105) → nothing missing or
  duplicated (checked by script over the whole log; tail read directly). Saved sources = continuation
  prompt: .c 252 (header 3.5 21 / */ 22; DATARO 50; MPU macros 76–84; g_mmfar 94, g_mmfar_valid 95;
  MemManage_Handler 155–170; CCR 210, RMW 211, dsb 212, isb 213, MPU 215–219, dsb 221, isb 222,
  trigger 224, SYST_RVR 229, Hello 233); .S 142 (slot 4 23 / 5 24 / 6 25; Default_Handler 115–120;
  MemManage pair 139–141; line 142 4 spaces, no EOF newline); .ld 45. md5 .c 2a3ecfb0…, .S 7ecc766b…,
  .ld 83f60f7f… (= session 21). Bench Map read in full (footer v153 = prompt) → v154; 3.5 bld 8/14,
  3.5 11.5/15, 3.5a 4/4 rows added to the score table (missing). Asked: Breakpoints view EMPTY?
  Next: Phase 3 self-test (paper) → GDB check (pc 0x226, x/3i 0x2F0) → Field Guide Phase 3 → 4.0.
### Phase 3 self-test (paper) — session 22, 2026-11-10 19:16 (graded 2.55/5)
Q1 0.6: 5 CFSR decodes right (no D1.2.11 p.1477); BFSR byte 0x82 right, reason "lower byte of
  0x8200" wrong (BFSR = byte 1, 0xE000ED29; MMFSR/BFSR share layout: bit 7 xFARVALID, bit 1 precise
  data error). Q2 0.4: IPSR/HFSR/USGFAULTENA right; SHCSR lost the active bits (3.1b 0x4, 3.1c
  0x0004_0008 measured); MEMFAULTENA-0 case: slot 3 Default_Handler, IPSR 3, HFSR 0x4000_0000 right;
  CFSR 0x0004_0000 vs own text (0x82); SHCSR 0 vs 0x0006_0004; RTKCW/RDQRR p.143 not cited; RM
  Table 4 right. Q3 0.25: slot symbolic ($sp+0x18 → 0x2000_FFA0), frame cited to D1.2.96 (B3.19
  p.107); precise rule RZLHD / TRM bus-error sentence out of scope → RLLLP p.282 + RQLHB p.105; 2.5d-2
  RA 0x226 (isb) not given, RKLWH ≠ return-address rule. Q4 0.6: 3.4 0x320 / 3.5 0x3B4 replaced by
  0x2000_FFFC; UNKNOWN pages + RXPWN/RGBKY right; c right. Q5 0.7: decode right; LIMIT p.1511 (3rd,
  → p.1751/1752); b "or" in every row, first values IACCVIOL / MMFARVALID 0 right, rule RXMRH p.88
  (not RFLDT); c SP = L (RZLZG p.115), no frame → RA method fails, right.
Weak points: "or"/"e.g."/symbolic values back; logged values replaced by remembered ones; rule scope.
Tutor check: RFLDT, RXMRH p.88, RLLLP p.282, RZLHD p.281, RQLHB p.105, RZLZG p.115, RTKCW/RDQRR p.143,
  RKLWH p.104 opened with pdftotext. GDB check issued (image 3.5). Bench Map v156.
### Phase 3 GDB check — session 22, 2026-11-10 19:26
Launch "Temporary breakpoint 1, main () at …:174"; info breakpoints none; $2 = 0x226; x/3i 0x2F0 =
  ldr r3,[pc,#188] (0x3B0) / ldr r2,[pc,#192] (0x3B4) / str r2,[r3,#0] → image 3.5, no rebuild.
  bp #2 0x2F4 (224), bp #3 0x206 (168), hardware.
At "Breakpoint 2, 0x000002f4 in main () at …:224": MMFAR = BFAR = 0x0000_03B4 (= session 21; Q4a).
At "Program received signal SIGSTOP, Stopped (signal). MemManage_Handler () at …:156": sp 0x2000_FF88;
  $sp+0x18 = 0x2000_FFA0 → 0x0000_02F4 = the str (Q3a); x/1bx 0xE000ED28 0x82 (MMFSR), 0xE000ED29
  0x00 (BFSR), CFSR word 0x82 (Q1b); MMFAR = BFAR = 0x2000_8040, BFARVALID 0 (Q4c); SHCSR 0x0007_0001
  (MEMFAULTACT at entry; Q2a).
At "Breakpoint 3, 0x00000206 in MemManage_Handler () at …:168": MPU_RNR 0, RBAR 0x2000_8045, RLAR
  0x2000_8041 (Q5a).
All self-test corrections confirmed; score stays 2.55/5. Q2b (MEMFAULTENA 0) paper only — optional
  extra. Asked: Terminate at bp #3 + unplug? Next: Field Guide Phase 3 → Phase 3 DONE. Bench Map v157.
### Phase 3 closed — session 22, 2026-11-10
Terminate at bp #3 after the GDB check + unplug CONFIRMED (user, 19:29).
Field Guide (doc 8724fa96, rev 31) extended: 7 sections after the CMSIS section — Faults: the alarm
  panel (CFSR map table, HFSR, SHCSR enables/escalation RTKCW/RDQRR p.143, 3.1b vs 3.1c, vector
  catch DEMCR 0x011007F0); Traps DIV_0_TRP / UNALIGN_TRP (3.1–3.2, RCWNS p.228, TRM Table 3-3
  discrepancy); STKOF via MSPLIM (IKDPG/RZLZG p.115, RGNVS p.143); Precise faults (RQLHB p.105,
  RLLLP p.282, TRM §1.4.1; RA table 3.5/3.4/3.2b/3.1b vs 2.5d-2); BusFault via DATARO (3.4); MemManage
  via the MPU (region decode table, MAIR 0x44, PRIVDEFENA RMCCL p.281, HFNMIENA RRHSF p.282, target
  choice, 3.5a RVTTN p.317, RM Table 4 vs TRM §4.1); Fault addresses (shared FAR table, RXMRH p.88,
  RXPWN/RGBKY p.89). Lessons: 9 Phase 3 checklist items + self-test line. Quick reference: Phase 3
  numbers (image 3.5). Still open: Z/C closed (session 16); added VALID-0 FAR content, RM Table 4 vs
  TRM, TRM UNALIGN_TRP, 3.3 no lockup, MSPLIM without ISB, power-up RAM; Next = Phase 4. Intro lines →
  Phase 3 (sessions 1–22). Title still "Phases 0–2" — rename asked.
Phase 3 DONE on the Bench Map (chip "done · self-test 2.55/5", test row added). Bench Map v158.
Next: 4.0 read-only audit (CONTROL, IPSR, MSP/PSP banking, MSPLIM/PSPLIM, SHPR2 — citations opened
  first).  

### 4.0 issued — session 22, 2026-11-10 22:14
learning_log.md re-checked (3,453 lines): old log = exact prefix; 4 session-22 blocks present once;
  line 3410 " ### Phase 3 self-test …" has a leading space → not a heading (fix). Field Guide renamed
  "M33 Field Guide: Phases 0–3 in Plain Words" (doc name + H1, rev 32).
4.0 = read-only audit at stop-at-main, image 3.5, no code change. Reads: control_s, control_ns, msp_s,
  psp_s, msp_ns, psp_ns, msplim_s, psplim_s, msplim_ns, psplim_ns, $sp, $xpsr (IPSR), SHPR2
  0xE000ED1C, SHPR2_NS 0xE002ED1C.
Sources opened (pdftotext): RCNMS/RRPKP/RCMQP p.69; RWVRK/IWCFH p.70; RGKVQ/RHVGB/INMBL p.73; RDTBJ
  p.75; RBHDK p.77; RRDLR/RTXRW/IDMLS/RMDXK/ILVWN p.78; CONTROL D1.2.13 p.1480–1482 (UPAC_EN 7,
  PAC_EN 6, UBTI_EN 5, BTI_EN 4 banked, RES0 without PACBTI; SFPA 3 not banked, RAZ/WI from NS; FPCA 2
  not banked; SPSEL 1 banked; nPRIV 0 banked; all Warm reset 0); IPSR D1.2.142 p.1707 (8:0, not
  banked); MSPLIM D1.2.177 p.1757 / PSPLIM D1.2.218 p.1814 (LIMIT 31:3, banked, Warm reset 0); SHPR2
  D1.2.235 p.1846 (0xE000ED1C; PRI_11 SVCall 31:24 banked, reset 0; 23:0 RES0; SHPR2_NS 0xE002ED1C
  RES0 to NS software and the debugger). GDB names known from the log: control_s/control_ns (1b.4b),
  msp_s/msp_ns (1a.5a), msplim_s/psplim_s (3.0). Predictions G1–G14 asked. Bench Map v159.
  
### 4.0 predictions — session 22, 2026-11-11 09:04 (lock check)
G1 control_s 0; G2 control_ns 0; G3 msp_s 0x2000_FFF0; G4 psp_s 0; G5 msp_ns 0; G6 psp_ns 0; G7
  msplim_s 0; G8 psplim_s 0; G9 msplim_ns 0; G10 psplim_ns 0; G11 $sp 0x2000_FFF0 = msp_s; G12 IPSR 0,
  Thread; G13 SHPR2 0; G14 SHPR2_NS 0.
Lock flags: G3 reason vs own files (Reset_Handler push, "sub sp,#16 (or similar)", 0x2001_0000 − 8 −
  16 ≠ 0x2000_FFF0, log entry/halt unnamed); G7 "3.3 MSPLIM code runs after stop-at-main" vs .c 3.5;
  G1/G2 independent 0s vs D1.2.13 banking (FPCA/SFPA not banked; SFPA RAZ/WI from NS), reset value
  alone; G5/G6 "uninitialized (0)"; G4/G9/G10/G11 no source or log entry; G12 unsourced clause.
Addendum allowed before the run. Bench Map v160.

### 4.0 predictions LOCKED — session 22, 2026-11-11 09:22 (addendum accepted)
G1 control_s 0x0000_000C; G2 control_ns 0x0000_0008; G3 msp_s 0x2000_FFE8; G4 psp_s 0; G5 msp_ns 0;
  G6 psp_ns 0; G7 msplim_s 0; G8 psplim_s 0; G9 msplim_ns 0; G10 psplim_ns 0; G11 $sp 0x2000_FFE8 =
  msp_s; G12 IPSR 0, Thread; G13 SHPR2 0; G14 SHPR2_NS 0.
Lock flags (recorded, grades stand): G1 float lines 175–176 vs stop line 174 (1b.4b read at
  for(;;)); G3 "sub sp,#16" not from the 3.5 objdump, "log 3.1a/3.5" unnamed; G4–G6 RRDLR p.78 states
  no reset value; G11 RGKVQ/RHVGB not about stack selection (IDMLS p.78). Run issued. Bench Map v161.
  
### 4.0 read-only audit — session 22, 2026-11-11 09:29 (graded)
Same image 3.5, no rebuild. "Temporary breakpoint 1, main () at …:174"; info breakpoints none; $2 =
  0x226; x/3i 0x2F0 = ldr r3 (0x3B0) / ldr r2 (0x3B4) / str r2,[r3,#0].
info registers: control_s 0, control_ns 0, msp_s 0x2000_FFF0, psp_s 0, msp_ns 0, psp_ns 0xFFFF_FFFC,
  msplim_s 0, psplim_s 0, msplim_ns 0, psplim_ns 0 (all names accepted). $sp 0x2000_FFF0 (= msp_s);
  $xpsr 0x6900_0000 (IPSR 0 → Thread; Z C Q T). SHPR2 0xE000ED1C 0 (SVCall level 0); SHPR2_NS
  0xE002ED1C 0 (RES0 to debugger, D1.2.235 p.1846).
Learned: FPCA 0 at main — first FP instruction is line 175, after the halt; CPACR enable doesn't set
  FPCA (D1.2.13 p.1482); 1a.5a control_s 0 at main, 1b.4b 0xC at for(;;). sp = 0x2001_0000 − 8 (push
  {r7,lr}) − 8 (sub sp,#8); bl main pushes nothing. SFPA RAZ/WI from NS (p.1481). PSP_NS 0xFFFF_FFFC
  with no writer in our image; bits 1:0 = 0 (RXPWM p.79); reset value not in opened docs → open
  (test: read at Reset_Handler after Restart). Unbanked FPCA in the NS view untested (FPCA 0 here).
Grades: G1 ❌ G2 ❌ G3 ❌ G4 ✅ G5 ✅ G6 ❌ G7 ✅ G8 ½ G9 ½ G10 ½ G11 ❌ G12 ✅ G13 ✅ G14 ✅ → 8.5/14.
Weak point: addendum moved G1/G2/G3 from right values to wrong ones (flags asked for reasons, not
  values). Asked: Terminate at main + unplug? Next (proposed): 4.1 Thread on PSP_S. Bench Map v162.
  
### 4.1 issued — session 22, 2026-11-11 09:33
Terminate at main after 4.0 + board unplugged: CONFIRMED (user).
4.1 = Thread mode on PSP_S, observed through the existing 3.5 MemManage (store at the trigger;
  handler unchanged). Composition C1–C9 asked: C1 PSP area (not .bss ≤0x2000_8020, not MPU region
  0x8040–805F, not MSP min 0x2000_FF84; alignment + rule); C2 placement vs float lines 175–176 and
  trigger; C3 msr psp inline asm (::: style), RBRLT p.116; C4 CONTROL RMW (MRS/OR/MSR), before/after
  values, why not a constant (MSR CONTROL writes nPRIV/SPSEL/FPCA/SFPA, C2.4.126 p.763 lines 108–117);
  C5 ISB (RSNGJ p.73) + DSB question; C6 -O0 r7 frame pointer vs new SP; C7 handler stack (RTXRW
  p.78), no handler/.S change; C8 GDB halts + read lists (addresses after objdump); C9 header after
  line 21 + edit positions.
Sources opened (pdftotext): RSNGJ/RHVGB/INMBL p.73; RTXRW/IDMLS p.78; RXPWM/RJXJM p.79; RPBVF p.109;
  RBRLT p.116; MSR C2.4.126 p.760–763; MRS C2.4.125 p.755 (located); EXC_RETURN D1.2.96 p.1628–1629.
  Bench Map v163.
  
### 4.1 composition round 1 — session 22, 2026-11-11 10:13
Accepted: C1 PSP_S top 0x2000_B000 (8-aligned; RXPWM/RJXJM p.79, RPBVF p.109 to cite); C4 RMW
  mrs → |= (1u<<1) → msr, CONTROL_S 0xC → 0xE (p.763 lines 108–117 to cite); C7 handler on MSP, no
  handler/.S change (RTXRW p.78 to cite); C9 header "4.1: Thread mode on the process stack (PSP_S)"
  after line 21.
Fix in round 2: C2 no line number; reason (prologue/f corruption) contradicts own C6 (locals via r7);
  C9 "after line 184" = between LED comment 184 and PDDR 185; C3 ": )" and C4 no clobber → ":::"
  style + "memory"; RBRLT p.116 uncited; C5 DSB not answered from the Phase 2 barrier rule
  (memory-mapped store vs special-register write); C6 MSP_S after switch + handler push vs main's r7
  frame; C8 stop-at-main "*0x186" (temp bp #1; not from any image), SIGSTOP "*0x1D0" as a bp
  (vector catch), GDB names exc_return/ipsr/cfsr/mmfar → $lr/$xpsr/x/1wx; missing msp_s, psp_s,
  control_s, frame x/8xw at entry; 0x2000B000u literal → own macro PSP_S_TOP with line + comment.
Bench Map v164.

### 4.1 composition round 2 — session 22, 2026-11-11 10:21
Accepted: C1 0x2000_B000 (RXPWM/RJXJM p.79, RPBVF p.109); C3 msr psp "r"(PSP_S_TOP) + "memory",
  RBRLT p.116; C4 RMW mrs/|=(1u<<1)/msr + "memory", CONTROL_S 0xC → 0xE, p.763 lines 108–117; C5 ISB
  only, no DSB (special-register write, not memory-mapped; RSNGJ p.73); C7 RTXRW/IDMLS p.78; macro
  PSP_S_TOP after line 84.
Fix in round 3: C2/C9 "before line 215 (REG32(MPU_TARGET_ADDR) = 0xDEADBEEFu)" — line 215 = MPU_RNR,
  trigger = 224; floats "170–171" → 175–176; C6 MSP_S 0x2000_FFE8 vs 4.0 measured 0x2000_FFF0
  (ctrl_val effect via objdump), "main's frame on PSP_S" (r7 frame on MSP); C8 SIGSTOP "sp = psp_s"
  and frame "at msp_s" contradict the design, SPSEL claim = prediction; trigger reads msp_s/psp_s;
  macro comment without source. Bench Map v165.
  
### 4.1 composition accepted — session 22, 2026-11-11 10:27 (rounds 1–3)
C1 PSP_S_TOP 0x2000_B000 (8-aligned; RXPWM/RJXJM p.79, RPBVF p.109; clear of .bss ≤0x2000_8020, MPU
  0x8040–805F, MSP min 0x2000_FF84). C2/C9 after line 222 (isb), before 224 (trigger). C3 msr psp
  "r"(PSP_S_TOP) "memory", not limit-checked (RBRLT p.116). C4 mrs → ctrl_val |= (1u<<1) → msr
  control, "memory"; CONTROL_S 0xC → 0xE; constant would clear FPCA/SFPA (C2.4.126 p.763 lines
  108–117). C5 ISB only (RSNGJ p.73; special-register write, no DSB). C6 MSP_S after switch from
  objdump. C7 handler on MSP (RTXRW/IDMLS p.78), no handler/.S change. C8 temp bp #1 → bp after isb →
  bp trigger str → SIGSTOP (vector catch) → bp park; read lists incl. msp_s/psp_s/control_s/$lr/
  CFSR/MMFAR/frame. Macro after line 84 with source comment; header after line 21.
Corrections carried (recorded): C6 "hardware stacking on MSP_S" → frame on the preempted context's
  stack = PSP_S (RWYWW p.104, RPBVF p.109), only the handler push on MSP; C8 frame at psp_s read at the
  SIGSTOP, not the pre-entry value.
Errors fixed across rounds: line 184 (LED block); line 215 mislabelled as the trigger; floats
  "170–171"; ": )" clobber; stop-at-main "*0x186"; SIGSTOP as a bp; exc_return/ipsr/cfsr/mmfar GDB
  names; MSP_S 0x2000_FFE8 from old G3; main's frame "on PSP_S"; frame "at msp_s".
File edit issued; expected header 22 / */ 23, macro 86, block 225–231, trigger 233, 261 lines.
  Bench Map v166.
  
### 4.1 saved-file check round 1 — session 22, 2026-11-11 11:23
User Select-String (from source\): 3.5 header 21, 4.1 header 22, */ 23; MPU_TARGET_ADDR 85; PSP_S_TOP
  87 (no comment — agreed comment missing); main 175; isb 216 (SHCSR pair) / 225 (MPU pair); block
  msr psp 228, ctrl_val 229, mrs 230, |= 231, msr control 232, isb 233 (RSNGJ); trigger 235; isb 253
  (ISPR pair); Count 263 (expected 261 → lines 86, 226–227, 234 unseen; likely blanks + block
  comment). .c not attached.
Asked: add the macro comment on line 87; show lines 84–88 and 224–236 with numbers + Count; attach .c.
  Bench Map v167.
  
### 4.1 saved-file check round 2 — sources frozen — session 22, 2026-11-11 11:29
File attached; diff vs 3.5 (.c 252) = agreed edits only: header 22 "4.1: Thread mode on the process
  stack (PSP_S)" (*/ 23); PSP_S_TOP 87 with source comment (RAMB .ld line 10, RJXJM p.79) + blank
  88; blank 226 + block 227–233 (comment, msr psp "r"(PSP_S_TOP) "memory", uint32_t ctrl_val, mrs
  control, |= (1u<<1), msr control, isb RSNGJ p.73); trigger 235. .c 263 lines, md5 97e4a1b1…, CRLF;
  .S 142 / .ld 45 unchanged. Sources frozen for 4.1.
Build predictions P1–P12 asked (warnings; text/data/bss; main + stop pc; block start; PSP_S_TOP load;
  block size; ctrl_val location; bp after isb; trigger str; MemManage + park; Reset/Default; slot 4).
  Bench Map v168.
  
### 4.1 build predictions — session 22, 2026-11-11 12:10 (lock check)
P1 0/0; P2 1195 / 4 / 28; P3 main 0x168, stop 0x16E; P4 0x220; P5 ldr literal "4 B"; P6 24 B; P7 r3;
  P8 0x238; P9 0x2FA; P10 0x1D0 / 0x206; P11 Reset 0x3E0, Default 0x434; P12 0x1D1.
Lock flags: P3 = image 2.5d values (3.5 build main 0x220; 4.0 pc 0x226); P4 = 3.5 main start (block
  after line 225 isb); P8 vs P9 (trigger = ldr, ldr, str); P11 unchanged vs P2/P6 main growth (Reset
  follows main); P5 mixed sizes; P6 not itemised, "orr/mov"; P7 hedge + reason vs -O0 named locals
  (f in 3.5 objdump); P1 reason not evidence. Addendum allowed. Bench Map v169.
  
### 4.1 build predictions LOCKED — session 22, 2026-11-11 12:12 (addendum accepted)
P1 0/0; P2 text 1219 / data 4 / bss 28; P3 main 0x220, stop 0x226; P4 symbolic (ungraded); P5 ldr
  literal 2 B + 4 B word; P6 24 B (ldr 2 + lit 4 + msr psp 4 + mrs 4 + orr 4 + msr control 4 + isb
  2); P7 [r7, #n] (n not given); P8 / P9 symbolic (ungraded); P10 MemManage 0x1D0, park 0x206; P11
  Reset 0x3F8 (Default not given); P12 0x1D1.
Lock flags (recorded, grades stand): P2 base 1195 = own earlier prediction (3.5 text 1171); P6 isb 2 B
  vs size rule 4 B; P6 no ctrl_val str/ldr vs own P7; P4/P8/P9 no value. Build commands issued
  ($tb/$od/$nm/$sz from Debug\). Bench Map v170.
  
### 4.1 build — session 22, 2026-11-11 12:23 (graded 12:24)
Result: 0/0; FLASH 1208 B, RAMB 32 B; text 1203 data 4 bss 28. .text 0x3D3 @0xE0; .data LMA 0x4B4;
  .bss 0x1C @0x2000_8004 (globals = 3.5). Symbols: uart4_putc 0xE0, SysTick 0x114, LP_FLEXCOMM4 0x14C,
  UsageFault 0x168, BusFault 0x180, MemManage_Handler 0x1D0 (0x50, park nop 0x206), main 0x220
  (0x1E0), Reset 0x400, Default 0x454, uart4_puts 0x458. .isr_vector slot 4 0x1D1, 5 0x181, 6 0x169,
  Default 0x455. main: prologue push {r7,lr} / sub sp,#8 / add r7,sp,#0 (stop pc 0x226); f at
  [r7,#0]; MPU dsb 0x2E8 isb 0x2EC; 4.1 block 0x2F0 ldr r3 ← 0x2000_B000 (lit 0x3CC), 0x2F2 msr PSP,r3
  (f383 8809), 0x2F6 mrs r3,CONTROL (f3ef 8314), 0x2FA str [r7,#4], 0x2FC ldr, 0x2FE orr.w #2, 0x302
  str, 0x304 ldr, 0x306 msr CONTROL,r3 (f383 8814), 0x30A isb; trigger ldr 0x30E (← 0x2000_8040),
  ldr 0x310 (← 0xDEADBEEF), str 0x312.
Learned: -O0 named local stored/reloaded around each statement (+4 × 2 B); isb = 4 B; ctrl_val at
  [r7,#4] → frame stays 8 B → MSP_S at main 0x2000_FFF0; SYSm in encodings = C2.4.126 table (PSP 0x09,
  CONTROL 0x14); main +0x20 = 30 code + 4 literal − 2 pad.
Grades: P1 ✅ P2 ❌ P3 ✅ P4 – P5 ✅ P6 ❌ P7 ½ P8 – P9 – P10 ✅ P11 ❌ P12 ✅ → 5.5/12 (P4/P8/P9
  ungraded). Board: bp #2 0x30E, bp #3 0x312, SIGSTOP 0x1D0, bp #4 0x206. Board predictions B1–B15
  asked. Bench Map v171.
  
### 4.1 board predictions — session 22, 2026-11-11 13:02 (lock check)
B1 0xE; B2 0x2000_FFE8; B3 0x2000_FFE8; B4 0x2000_B000; B5 0x2000_B000; B6 0x1D0; B7 0x2000_FFE8; B8
  0x2000_FFE8; B9 0x2000_B000; B10 0xFFFF_FFE9; B11 0xC; B12 0x0900_0034; B13 {0x498, 0x2000_8020,
  0xDEADBEEF, 0x2000_8040, 0x4, 0x41B, 0x31E, 0x2910_0004}; B14a 0x82; B14b 0x2000_8040; B15
  0x2000_FFE8.
Lock flags: FFE8 vs 4.1 build (frame 8 B) + 4.0 msp_s at main; B2 value MSP vs own reason (Thread on
  PSP); B9 "retains" vs own B13 (frame on PSP; RJXJM p.79 / RPBVF p.109), read address = stack top;
  B10 hex vs own SPSEL = 1 decode, "FType 0/1"; B11 vs Field Guide entry rule (FPCA/SFPA); B12 IPSR
  0x34 vs own "4" (2nd session, 3.5 B7); B13 LR/RA/R0 = 3.5 values vs 4.1 build (Reset 0x400, str
  0x312, __data_load__ 0x4B4), RETPSR IPSR = interrupted mode; B15 "remains" vs push {r7} at 0x1D0;
  B14a name, B5 label. Addendum allowed. Bench Map v172.

### 4.1 board predictions LOCKED — session 22, 2026-11-11 13:16 (addendum accepted)
B1 0xE; B2 0x2000_B000; B3 0x2000_FFF0; B4 0x2000_B000; B5 0x2000_B000; B6 0x1D0; B7 0x2000_FFF0;
  B8 0x2000_FFF0; B9 0x2000_AFA8; B10 0xFFFF_FFE9; B11 0xC; B12 0x0100_0004; B13 {0x4B8, 0x2000_8020,
  0xDEADBEEF, 0x2000_8040, 0x4, 0x43B, 0x322, 0x2910_0000}; B14a 0x82; B14b 0x2000_8040; B15
  0x2000_FFEC.
Lock flags (recorded, grades stand): B9 arithmetic (0x2000_B000 − 0x68); B10 hex vs own bit list;
  B11 reason self-contradictory; B12 "Q/APSR flags" vs hex; B13 RA 0x322 vs own B5 / build str 0x312,
  "__bss_end__ shifted" vs build. Run issued: temp bp #1 (:177, pc 0x226), bp #2 0x30E, bp #3 0x312,
  SIGSTOP, bp #4 0x206; Terminate at bp #4. Bench Map v173.

### 4.1 board run — session 22, 2026-11-11 13:30 (graded)
Launch "Temporary breakpoint 1, main () at …:177", pc 0x226; x/3i 0x30E = ldr r3 (0x3D0) / ldr r2
  (0x3D4) / str r2,[r3,#0] → image 4.1. bp #2 0x30E (235), bp #3 0x312 (235), bp #4 0x206 (171).
At "Breakpoint 2, main () at …:235" (pc 0x30E): control_s 0xE, msp_s 0x2000_FFF0, psp_s 0x2000_B000,
  $sp 0x2000_B000. At "Breakpoint 3, 0x00000312": $sp 0x2000_B000.
At "Program received signal SIGSTOP, Stopped (signal). MemManage_Handler () at …:159": pc 0x1D0, $sp
  0x2000_FFF0 = msp_s, psp_s 0x2000_AF98, control_s 0, $lr 0xFFFF_FFED, $xpsr 0x2900_0004; x/8xw
  0x2000_AF98: 0x4B8, 0x2000_8020, 0xDEADBEEF, 0x2000_8040, 0x4, 0x43B, 0x312, 0x2910_0000; CFSR
  0x82, MMFAR 0x2000_8040.
At "Breakpoint 4, 0x00000206 in MemManage_Handler () at …:171": $sp 0x2000_FFEC.
Learned: CONTROL RMW + ISB → Thread on PSP_S (RSNGJ p.73); MSP_S untouched (0x2000_FFF0). Exception
  from Thread/PSP stacks on PSP (RWYWW p.104, RPBVF p.109): 26 words (FPCA 1), 0x2000_B000 − 0x68 =
  0x2000_AF98, no pad (RETPSR bit 9 0). EXC_RETURN 0xFFFF_FFED (S1 DCRS1 FType0 Mode1 SPSEL1 ES1,
  D1.2.96). Handler on MSP (RTXRW p.78), push {r7} → 0x2000_FFEC; CONTROL_S 0 in handler (SPSEL by
  RHVGB p.73; FPCA/SFPA cleared on entry). R0 = __data_load__ end 0x4B8, R1 = __bss_end__ (3rd
  sample, inference); LR 0x43B = Reset bl-main return (0x400 + 0x3A + 1); RA 0x312 = the str
  (precise). Live xPSR no SFPA, RETPSR bit 20 set (as 3.4/3.5).
Grades: B1–B8 ✅ B9 ❌ B10 ❌ B11 ❌ B12 ❌ B13 ½ B14a ✅ B14b ✅ B15 ✅ → 11.5/16.
Weak points: right reason, wrong number (B9 arithmetic, B10 hex); B11 Field Guide entry rule unused;
  B12 dropped C/Q. Asked: Terminate at bp #4 + unplug? Next (proposed): 4.2 unprivileged Thread
  (nPRIV) + privileged-only access; 4.3 SVC back. Bench Map v174.

### Plan check — session 22, 2026-11-11 13:37
Pace measured: 22 sessions / ~29 days (≈0.75/day); phases 1–3 = 8 / 6 / ~6.5 sessions. Phase 4 at
  4.1 done. Remaining estimate (tutor, extrapolated): P4 rest 4–5, P5 7–9, P6 6–8, P7 3–4, P8 4–5,
  P9 3–4, P10 4–5, P11 3–4, P12 8–10, P13 6–10 → ~48–64 sessions → late Jan–mid Feb 2027 at
  current pace (March if slower). Speed levers: addendum values only from traces, back-decode every
  hex, Select-String before every file send; optional lighter format (merged build+board
  predictions) from Phase 7 — decision deferred to Phase 6 close.  


### Time box + GitHub — session 22, 2026-11-11 13:47–14:39
Decision (user, 13:47): finish by 2026-12-06 (job search; interview-ready proof sooner). Plan: wk 1
  (11–17 Nov) GitHub → 4.2 unprivileged Thread → 4.3 SVC → short P4 test; wk 2 (18–24 Nov) Phase 5
  two-task PendSV switcher on PSP stacks; wk 3 (25 Nov–1 Dec) minimal Phase 6 FreeRTOS CM33_NTZ port
  on our startup/.ld, 2 tasks + a queue; 2–5 Dec packaging (READMEs, Field Guide to P6, interview
  drill bank, resume bullets); 6 Dec travel buffer. After: revision drills; P7–P12 optional. Lighter
  format until then: 6–8 prediction rows, build + board predictions in one message, tutor drafts
  routine code; lock / hardware evidence / log unchanged.
4.1 Terminate + Remove All Terminated: CONFIRMED (user, 14:05).
Git (user, 14:05): option C + public. Workspace repo: Count 0 tracked files from the scratch folder;
  "frdmmcxn236_m33_scratch/" added to .git/info/exclude → gone from status (hello_world changes left
  alone). New repo: git init -b main in frdmmcxn236_m33_scratch\. First git add staged Debug/,
  .launch, .settings/ — .gitignore had been saved as "gitignore" → git rm -r --cached, renamed,
  re-added → 8 files (.cproject, .gitignore, .project, README.md, learning_log.md, m33_scratch.ld,
  source .c, .S). Author Mohammed Hafiz, GitHub noreply email. Root commit 4b89487 "Phases 0-4.1:
  bare-metal Cortex-M33 on FRDM-MCXN236". Password push refused (GitHub needs browser or token) →
  credential.helper manager, browser sign-in → pushed to
  github.com/hafeezmhd/cortex-m33-bare-metal-mcxn236 (public), main tracks origin/main.
Next: 4.2 unprivileged Thread mode (lighter format). Bench Map v178.