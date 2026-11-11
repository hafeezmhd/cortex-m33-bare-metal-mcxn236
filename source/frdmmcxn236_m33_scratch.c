/*
1b.1  g_init (.data) / g_zero (.bss) -> copied/zeroed by Reset_Handler (1b.3)
1b.4  float ops -> FPU enabled by Reset_Handler (CPACR)
1c    Thumb-bit fault experiment done and removed (see log 1c)
1d.3: light the LED from C code
1d.4: blink the red LED with PTOR and a busy-wait loop
1e.3e: UART gates (bus clocks, resets, FC4 clock, pin mux) in C
1e.4e-1 UART init + polled putc in C
1e.4e-2: print a string
2.3b: SysTick polled (COUNTFLAG), processor clock, 1 ms period.
2.4: the SysTick interrupt.
2.5a table to vector 55 + own IRQ 39 handler (static)
2.5d: firmware-initiated IRQ 39 pend and verification
3.1a: integer division by zero with trap OFF (DIV_0_TRP = 0)
3.1c: UsageFault handler and vector slot 6 override
3.1d: Self-setup of DIV_0_TRP and USGFAULTENA in C with CFSR logging
3.2a: Unaligned word access with UNALIGN_TRP = 0
3.2b: Unaligned word access with UNALIGN_TRP = 1
3.3: Stack limit checking with MSPLIM
3.4: a real BusFault, and a handler that checks BFARVALID before using BFAR
3.5: MemManage via the MPU
4.1: Thread mode on the process stack (PSP_S)
*/

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define REG8(addr)  (*(volatile uint8_t *)(addr))

#define SYSCON_AHBCLKCTRLSET0 0x50000220u
#define GPIO4_PCOR            0x5009E048u
#define GPIO4_PDDR            0x5009E054u
#define GPIO4_PTOR            0x5009E04Cu

/*UART*/
#define SYSCON_AHBCLKCTRLSET1 0x50000224u
#define SYSCON_PRESETCTRLCLR0 0x50000140u
#define SYSCON_PRESETCTRLCLR1 0x50000144u
#define SYSCON_FROHFDIV		  0x50000388u
#define SYSCON_FCCLKSEL4      0x500002C0u
#define SYSCON_FLEXCOMM4CLKDIV 0x50000860u
#define PORT1_PCR8			  0x501170A0u
#define PORT1_PCR9			  0x501170A4u

/* LPUART4 register addresses */
#define LP_FLEXCOMM4_PSELID   0x500B4FF8u
#define LPUART4_BAUD     0x500B4010u
#define LPUART4_STAT     0x500B4014u
#define LPUART4_CTRL     0x500B4018u
#define LPUART4_DATA     0x500B401Cu
#define LPUART4_DATARO	 0x500B4030u	 // RM 50.5.5.1 p.1863

/*LPUART4_STAT*/
#define LPUART4_STAT_TDRE   (1u << 23)


#define SYST_CSR              0xE000E010u /* DDI0553 D1.2.241 */
#define SYST_RVR              0xE000E014u  /* D1.2.243 */
#define SYST_CVR              0xE000E018u /* D1.2.242 */
#define SYST_CSR_COUNTFLAG    (1u << 16) /* bit 16, D1.2.241 */

#define SCB_SHPR3_PRI15 	  0xE000ED23u /* DDI0553 D1.2.236 p.1847: SHPR3 = PRI_15 = bits 31:24 */

#define NVIC_ISER1              0xE000E104u /* DDI0553 D1.2.186 p.1770 */
#define NVIC_ISPR1              0xE000E204u /* DDI0553 D1.2.187 p.1771-1772 */
#define NVIC_ISER1_IRQ39_SET    (1u << 7)   /* DDI0553 D1.2.186 p.1770 */
#define NVIC_ISPR1_IRQ39_SET    (1u << 7)   /* DDI0553 D1.2.187 p.1771-1772 */

#define SCB_CCR 				0xE000ED14u /* DDI0553 D1.2.9 p.1471 */
#define SCB_SHCSR 				0xE000ED24u /* DDI0553 D1.2.233 p.1836 */
#define SCB_SHCSR_USGFAULTENA_Msk (1u << 18) /* DDI0553 D1.2.233 p.1837 */
#define SCB_CFSR 				0xE000ED28u /* DDI0553 D1.2.11 p.1477 */
#define SCB_SHCSR_BUSFAULTENA_Msk (1u << 17) /* DDI0553 D1.2.233 p.1838 */
#define SCB_BFAR				0xE000ED38u /* DDI0553 D1.2.6 p.1466 */
#define SCB_CFSR_BFARVALID_Msk	(1u << 15) 	/* DDI0553 D1.2.11 p.1477 (BFSR = CFSR[15:8]) */

#define MPU_CTRL 				0xE000ED94u // (D1.2.168 p.1743)
#define MPU_RNR 				0xE000ED98u // (D1.2.175 p.1755)
#define MPU_RBAR 				0xE000ED9Cu // (D1.2.171 p.1747)
#define MPU_RLAR 				0xE000EDA0u // (D1.2.173 p.1751)
#define MPU_MAIR0 				0xE000EDC0u // (D1.2.169 p.1745)
#define SCB_MMFAR 				0xE000ED34u // (D1.2.166 p.1739)
#define SCB_SHCSR_MEMFAULTENA_Msk (1u << 16) // (D1.2.233 p.1838)
#define SCB_CFSR_MMFARVALID_Msk (1u << 7)   // (D1.2.167 p.1740, D1.2.11 p.1477)
#define MPU_TARGET_ADDR 		0x20008040u // un-owned RAMB, 32-byte aligned, IGLXD p.281

#define PSP_S_TOP 0x2000B000u 				/* RAMB .ld line 10, 8-aligned (RJXJM p.79), above .bss end and the MPU region, below MSP minimum 0x2000_FF84 */

volatile uint32_t g_lpflexcomm4_irq_count;   /* .bss (zeroed by Reset_Handler .S lines 100–107); written by the IRQ 39 handler, read in main from 2.5c/d onward → volatile: main's reads must not be kept in a register. */

unsigned int g_init = 0x12345678;   /* .data */
unsigned int g_zero;                /* .bss  */

volatile uint32_t g_cfsr;
volatile uint32_t g_bfar;
volatile uint8_t g_bfar_valid;
volatile uint32_t g_mmfar = 0;
volatile uint8_t g_mmfar_valid = 0;

static void uart4_putc(char c)
{
	while ((REG32(LPUART4_STAT) & LPUART4_STAT_TDRE) == 0u)
	{
		/* wait for TX data register empty */
	}
	REG32(LPUART4_DATA) = (uint32_t)(unsigned char)c;
}

static void uart4_puts(const char *s)
{
    while (*s != '\0') {     /* stop at the null terminator */
        uart4_putc(*s);      /* emit the current character   */
        s++;                 /* advance to the next one      */
    }
}

void SysTick_Handler(void) {
    static uint32_t tick = 0;
    if (++tick >= 500u) {          // 500 ms half-period, 10 ON / 10 s
        tick = 0;
        REG32(GPIO4_PTOR) = (1u << 18);   // toggle LED
    }
}

/* 2.5a: IRQ39, vector 55 (Table 15 p.33).
 * Weak alias in startup_m33.S overridden by this strong definition;
 * no FP, no peripheral access — main and GDB reads the counter after the run. */
void LP_FLEXCOMM4_IRQHandler(void)
{
    g_lpflexcomm4_irq_count++;
}

void UsageFault_Handler(void)
{
	g_cfsr = REG32(SCB_CFSR);
	for(;;)
	{}
}


void BusFault_Handler(void)
{
	g_cfsr = REG32(SCB_CFSR);
	if(g_cfsr & SCB_CFSR_BFARVALID_Msk)
	{
		g_bfar = REG32(SCB_BFAR);
		g_bfar_valid = 1;
	}
	else
	{
		g_bfar = 0;
		g_bfar_valid = 0;
	}
	for(;;)
	{}
}

void MemManage_Handler(void)
{
	g_cfsr = REG32(SCB_CFSR);
	if(g_cfsr & SCB_CFSR_MMFARVALID_Msk)
	{
		g_mmfar = REG32(SCB_MMFAR);
		g_mmfar_valid  = 1;
	}
	else
	{
		g_mmfar = 0;
		g_mmfar_valid  = 0;
	}
	for(;;)
	{}
}

int main(void)
{
	g_zero = g_init;
	volatile float f = 1.5f;
    f = f * 2.0f;

    /* 1. Clock SET first: enable PORT4 and GPIO4: bit 17 PORT4, bit 23 GPIO4 */
    REG32(SYSCON_AHBCLKCTRLSET0) = (1u << 17) | (1u << 23);

    /* 2. Set level before direction: P4_18 low */
    REG32(GPIO4_PCOR) = (1u << 18);

    /* 3. Configure P4_18 as output */
    REG32(GPIO4_PDDR) = (1u << 18);

	/*UART*/
	REG32(SYSCON_AHBCLKCTRLSET0) = (1u << 14); // bus clock
	REG32(SYSCON_AHBCLKCTRLSET1) = (1u << 15); // bus clock
	REG32(SYSCON_PRESETCTRLCLR0) = (1u << 14); // reset
	REG32(SYSCON_PRESETCTRLCLR1) = (1u << 15); // reset
	REG32(SYSCON_FROHFDIV)       = 0;  // functional clock
	REG32(SYSCON_FCCLKSEL4)      = 3u; /* functional clock - SEL 011b = fro_hf_div */
	REG32(SYSCON_FLEXCOMM4CLKDIV) = 0;  // functional clock
	REG32(PORT1_PCR8)           = (2u << 8) | (1u << 12); // pin mux
	REG32(PORT1_PCR9)			  = (2u << 8); // pin mux

	REG32(LP_FLEXCOMM4_PSELID) = (1u << 0);

	/*
	 * RM 50.5.5.6 p.1871: BAUD[SBR12:SBR0] may only be changed while both
	 * CTRL[RE] and CTRL[TE] are 0 — so program BAUD before enabling CTRL
	 */

	REG32(LPUART4_BAUD)   = (1u << 1)  | (1u << 3)  | (1u << 4)  |
	                        (1u << 24) | (1u << 25) | (1u << 26) | (1u << 27);

	REG32(LPUART4_CTRL)   = (1u << 19);

	REG32(SCB_CCR) = 0x00000201u; /* D1.2.9 p.1474: 3.0 measured 0x201; UNALIGN_TRP = 0 */
	REG32(SCB_SHCSR) = REG32(SCB_SHCSR) | SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk; /* p. 1836: D1.2.233 p. 1836 explicitly recommends using a read-modify-write sequence for SHCSR to preserve active and pending status bits while modifying enable flags. */
	__asm__ volatile ("dsb":::"memory"); // RSJQJ p.222 / RBMLS p.268
	__asm__ volatile ("isb":::"memory"); // RNPDJ / RHMNM p.222, RWQQB p.268

	REG32(MPU_RNR) = 0u; 				// region 0
	REG32(MPU_RBAR) = 0x20008045u;		// BASE 0x2000_8040, SH 00, AP 10 priv RO, XN 1
	REG32(MPU_RLAR) = 0x20008041u;		// LIMIT 0x2000_805F, AttrIndx 0, EN 1
	REG32(MPU_MAIR0) = 0x44u;			// Attr0 Normal Non-cacheable
	REG32(MPU_CTRL) = 0x00000005u;		// PRIVDEFENA 1, HFNMIENA 0, ENABLE 1

	__asm__ volatile ("dsb":::"memory"); // RSJQJ p.222 / RBMLS p.268
	__asm__ volatile ("isb":::"memory"); // RNPDJ / RHMNM p.222, RWQQB p.268

	/* 4.1: switch Thread mode to PSP_S */
	__asm__ volatile ("msr psp, %0" : : "r" (PSP_S_TOP) : "memory");
	uint32_t ctrl_val;
	__asm__ volatile ("mrs %0, control" : "=r" (ctrl_val) : : "memory");
	ctrl_val |= (1u << 1);
	__asm__ volatile ("msr control, %0" : : "r" (ctrl_val) : "memory");
	__asm__ volatile ("isb" : : : "memory"); /* RSNGJ p.73 */

	REG32(MPU_TARGET_ADDR) = 0xDEADBEEFu;


	/* IPPGV order: RVR, then CVR, then CSR.
	 * (IVHDT: period = RELOAD+1; 48 MHz log 1e.2a) */
	REG32(SYST_RVR) = 47999u;      /* 48 MHz: 48000 ticks - 1 = 1 ms */
	REG32(SYST_CVR) = 0u;          /* clear CVR and COUNTFLAG; reload next edge */


	uart4_puts("Hello\r\n");

	REG8(SCB_SHPR3_PRI15) = 0x80u; // bits [7:5] = level 4 (log 2.2a); 3 priority bits implemented

	REG32(SYST_CSR) = (1u << 2) | (1u << 1) | (1u << 0); // ENABLE, TICKINT and CLKSOURCE

	REG32(NVIC_ISER1) = NVIC_ISER1_IRQ39_SET; // W1S write (DDI0553 D1.2.186 p.1770)
	REG32(NVIC_ISPR1) = NVIC_ISPR1_IRQ39_SET; // W1S write (DDI0553 D1.2.187 p.1771-1772)
	__asm__ volatile ("dsb" : : : "memory"); // Ensure SCS write completion (RSJQJ/RNPDJ p.222)
	__asm__ volatile ("isb" : : : "memory"); // Context synchronization event for visibility (RWQQB p.268)

	if (g_lpflexcomm4_irq_count > 0) {
		uart4_puts("IRQ 39 taken\r\n");
	} else {
		uart4_puts("IRQ 39 not taken yet\r\n");
	}

	for (;;) {
    }
}
