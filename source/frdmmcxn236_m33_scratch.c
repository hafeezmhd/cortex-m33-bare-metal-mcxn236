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
4.2: Thread mode unprivileged (CONTROL.nPRIV) under the 3.5 MPU
4.3: SVC back to privileged (MPU off, SVC_Handler slot 11)
5.1: two-task scheduler data (TCBs, initial frames in C), PendSV_Handler in .S slot 14, PendSV level 7
5.2: first task start (main pends PendSV once via ICSR.PENDSVSET)
5.3: SysTick pends PendSV every SLICE_TICKS ticks; task A LED, task B UART
6.1: FreeRTOS V11.2.0 ARM_CM33_NTZ port (SECURE_ONLY) on our startup/.ld; one LED task.
     Phase 5 switcher, SysTick_Handler, SVC_Handler removed (port owns slots 11/14/15).
6.2: queue A -> B. A sends its count, then spins 3 ticks; B blocks on the queue and prints.
     Run 1 TASK_B_PRIO 2 (above A), run 2 TASK_B_PRIO 1 (equal): event switch vs tick switch.
7.1: Modbus RTU slave 1 on LPUART4 (VCOM COM3), 19200 8E1, decisions 7.0 D1-D12.
     RX + idle-line interrupt (IRQ 39 at 0x20) -> frame length queue -> modbus task (replaces
     task_b); CRC-16, FC 03/06/16, exceptions 01/02/03; polled TX. All UART prints, the IRQ 39
     test pend, uart4_puts/uart4_put_u32 and the 6.2 queue/globals removed; task_a only counts.
     Unused macros removed: GPIO4_PTOR, LPUART4_DATARO, SYST_*, SCB_SHPR3_*, SCB_ICSR*, NVIC_ISPR1*, MPU_*.
*/

#include <stdint.h>
#include <stddef.h>      /* size_t for memset/memcpy (6.0 D5 = N) */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define REG8(addr)  (*(volatile uint8_t *)(addr))

#define SYSCON_AHBCLKCTRLSET0 0x50000220u
#define GPIO4_PSOR            0x5009E044u /* 7.1: LED off (P4_18 high, active-low LED) */
#define GPIO4_PCOR            0x5009E048u
#define GPIO4_PDOR            0x5009E040u /* 7.1: read back the LED pin level for register 1 */
#define GPIO4_PDDR            0x5009E054u

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

/*LPUART4_STAT (RM 50.5.5.7 p.1872-1877)*/
#define LPUART4_STAT_TDRE   (1u << 23)
#define LPUART4_STAT_RDRF   (1u << 21)  /* 7.1: cleared by reading DATA (FIFO off) */
#define LPUART4_STAT_IDLE   (1u << 20)  /* 7.1: W1C */
#define LPUART4_STAT_OR     (1u << 19)  /* 7.1: W1C */
#define LPUART4_STAT_NF     (1u << 18)  /* 7.1: W1C */
#define LPUART4_STAT_FE     (1u << 17)  /* 7.1: W1C */
#define LPUART4_STAT_PF     (1u << 16)  /* 7.1: W1C */
#define LPUART4_STAT_ERR    (LPUART4_STAT_OR | LPUART4_STAT_NF | LPUART4_STAT_FE | LPUART4_STAT_PF)

/* 7.0 Q3: 19200 baud = 48 MHz / ((15 + 1) x 156) = 19,230.77 (+0.16 %); OSR 28:24, SBR 12:0 */
#define LPUART4_BAUD_19200    0x0F00009Cu
/* 7.0 D2/D3 (RM 50.5.5.8 p.1879-1885): RIE 21, ILIE 20, TE 19, RE 18, IDLECFG 010 (4 idle chars)
 * bits 10:8, M 4 = 9-bit (8 data + parity), ILT 2 = count after the stop bit, PE 1, PT 0 = even */
#define LPUART4_CTRL_8E1_RX   ((1u << 21) | (1u << 20) | (1u << 19) | (1u << 18) | \
                               (2u << 8) | (1u << 4) | (1u << 2) | (1u << 1))

#define SCB_CCR 				0xE000ED14u /* DDI0553 D1.2.9 p.1471 */
#define SCB_SHCSR 				0xE000ED24u /* DDI0553 D1.2.233 p.1836 */
#define SCB_SHCSR_USGFAULTENA_Msk (1u << 18) /* DDI0553 D1.2.233 p.1837 */
#define SCB_CFSR 				0xE000ED28u /* DDI0553 D1.2.11 p.1477 */
#define SCB_SHCSR_BUSFAULTENA_Msk (1u << 17) /* DDI0553 D1.2.233 p.1838 */
#define SCB_BFAR				0xE000ED38u /* DDI0553 D1.2.6 p.1466 */
#define SCB_CFSR_BFARVALID_Msk	(1u << 15) 	/* DDI0553 D1.2.11 p.1477 (BFSR = CFSR[15:8]) */
#define SCB_MMFAR 				0xE000ED34u // (D1.2.166 p.1739)
#define SCB_SHCSR_MEMFAULTENA_Msk (1u << 16) // (D1.2.233 p.1838)
#define SCB_CFSR_MMFARVALID_Msk	(1u << 7)  	// (D1.2.167 p.1740, D1.2.11 p.1477)

#define NVIC_ISER1              0xE000E104u /* DDI0553 D1.2.186 p.1770 */
#define NVIC_ISER1_IRQ39_SET    (1u << 7)   /* DDI0553 D1.2.186 p.1770 */
#define NVIC_IPR9_PRI_N3        0xE000E427u /* 7.0 D9: IRQ 39 priority byte (D1.2.185 p.1769, RM Table 15) */
#define IRQ39_PRIO              0x20u       /* 7.0 Q1: level 1 = MAX_SYSCALL; >= 0x20 passes port.c 2132 */

#define DEMCR                   0xE000EDFCu /* 7.1: D1.2.37 p.1522, TRCENA bit 24 */
#define DWT_CTRL                0xE0001000u /* 7.1: CYCCNTENA bit 0 */
#define DWT_CYCCNT              0xE0001004u /* D1.2.59 p.1566 */

#define TASK_A_STACK_WORDS 		256u        /* 6.1: 1 KB, static in .bss */
#define TASK_MB_STACK_WORDS 	256u        /* 7.1: modbus task, 1 KB */
#define TASK_A_PRIO 			1u          /* 6.1: counter */
#define TASK_MB_PRIO 			2u          /* 7.1: above A, so a frame is answered at once */
#define MB_FRAME_Q_LEN 			1u          /* 7.1: one frame in flight (D10) */
#define MB_BUF_SIZE 			256u        /* 7.1: RTU frame max 256 B (Serial Line p.13) */
#define MB_SLAVE_ADDR 			1u          /* 7.0 D5 */
#define MB_NUM_REGS 			8u          /* 7.0 D5: holding registers 0-7 */
#define MB_FRAME_BAD 			0x80000000u /* 7.1: queue item bit 31 = OR/NF/FE/PF or overflow seen */

volatile uint32_t g_lpflexcomm4_irq_count;   /* .bss; IRQ 39 entries (7.1: every RX/idle interrupt) */

unsigned int g_init = 0x12345678;   /* .data */
unsigned int g_zero;                /* .bss  */

volatile uint32_t g_cfsr;
volatile uint32_t g_bfar;
volatile uint8_t g_bfar_valid;
volatile uint32_t g_mmfar = 0;
volatile uint8_t g_mmfar_valid = 0;

volatile uint32_t g_cnt_a;                 /* .bss; task_a loop count = holding register 0 */

volatile uint32_t g_assert_line;           /* 6.1: set by configASSERT (FreeRTOSConfig.h) */
const char * volatile g_assert_file;       /* 6.1: set by configASSERT */

/* 7.1: Modbus counters and timing (read with GDB) */
volatile uint32_t g_mb_rx_bytes;           /* bytes stored into s_rx_buf */
volatile uint32_t g_mb_dropped;            /* bytes read while the task owned the buffer */
volatile uint32_t g_mb_err_frames;         /* frames with OR/NF/FE/PF or > 256 B */
volatile uint32_t g_mb_crc_bad;            /* frames with a wrong CRC */
volatile uint32_t g_mb_other_addr;         /* good CRC, address not 0 or 1 */
volatile uint32_t g_mb_ok;                 /* normal replies sent */
volatile uint32_t g_mb_exc;                /* exception replies sent */
volatile uint32_t g_mb_last_stat;          /* STAT at the last error */
volatile uint32_t g_mb_cyc_last_byte;      /* CYCCNT when the last byte of a frame was read */
volatile uint32_t g_mb_cyc_idle;           /* CYCCNT when IDLE was seen */
volatile uint32_t g_mb_cyc_reply;          /* CYCCNT when the first reply byte was written */
volatile uint16_t g_mb_crc_rx;             /* CRC field of the last good-CRC request (low byte first) */
volatile uint16_t g_mb_regs_rw[4];         /* holding registers 4-7, scratch */

static StaticTask_t s_task_a_tcb;                       /* 6.1: task A TCB, .bss (D3 = T) */
static StackType_t  s_task_a_stack[TASK_A_STACK_WORDS]; /* 6.1: task A stack, .bss */
static StaticTask_t s_task_mb_tcb;                      /* 7.1: modbus task TCB */
static StackType_t  s_task_mb_stack[TASK_MB_STACK_WORDS]; /* 7.1: modbus task stack */
static StaticQueue_t s_frame_q_struct;                  /* 7.1: frame-length queue control block */
static uint8_t      s_frame_q_storage[MB_FRAME_Q_LEN * sizeof(uint32_t)];
static QueueHandle_t s_frame_q;                         /* 7.1: ISR -> modbus task */
static StaticTask_t s_idle_tcb;                         /* 6.1: idle task TCB, .bss */
static StackType_t  s_idle_stack[configMINIMAL_STACK_SIZE]; /* 6.1: idle task stack, .bss */

static uint8_t s_rx_buf[MB_BUF_SIZE];      /* 7.1: request bytes (D10) */
static volatile uint32_t s_rx_len;         /* 7.1: bytes in s_rx_buf */
static volatile uint32_t s_rx_bad;         /* 7.1: error seen in this frame */
static volatile uint32_t s_rx_owned;       /* 7.1: 1 = modbus task owns s_rx_buf, ISR drops bytes */
static uint8_t s_tx_buf[MB_BUF_SIZE];      /* 7.1: reply bytes */

/* 6.0 D5 = N: -nostdlib kept, so the kernel's memset/memcpy (12 calls) and any
 * GCC-emitted ones resolve here. Byte loops; the attribute stops GCC turning
 * the loop back into a call to itself at higher -O levels. */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s;
	while (n-- != 0u) { *p++ = (unsigned char)c; }
	return s;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n-- != 0u) { *d++ = *s++; }
	return dst;
}

/* 6.0 D3: static allocation without kernel-provided memory -> we supply the idle task's */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *puxIdleTaskStackSize)
{
	*ppxIdleTaskTCBBuffer   = &s_idle_tcb;
	*ppxIdleTaskStackBuffer = s_idle_stack;
	*puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

static void uart4_putc(char c)
{
	while ((REG32(LPUART4_STAT) & LPUART4_STAT_TDRE) == 0u)
	{
		/* wait for TX data register empty */
	}
	REG32(LPUART4_DATA) = (uint32_t)(unsigned char)c;
}

/* 7.1: CRC-16/Modbus, Serial Line V1.02 p.39: init 0xFFFF, LSB first, poly 0xA001.
 * Returned value: low byte = first byte on the wire (p.15: low-order byte appended first). */
static uint16_t mb_crc16(const uint8_t *p, uint32_t n)
{
	uint16_t crc = 0xFFFFu;
	while (n-- != 0u)
	{
		crc ^= *p++;
		for (uint32_t b = 0u; b < 8u; b++)
		{
			if ((crc & 1u) != 0u) { crc = (uint16_t)((crc >> 1) ^ 0xA001u); }
			else                  { crc = (uint16_t)(crc >> 1); }
		}
	}
	return crc;
}

/* 7.1: D10 — RX byte(s) and end of frame by idle line. IRQ 39 at 0x20 (D9) so the FromISR
 * call passes port.c 2132. STAT is RW but its config bits (MSBF, RXINV, RWUID, BRK13, LBKDE)
 * stay 0 here, so writing only W1C flags back changes nothing else. */
void LP_FLEXCOMM4_IRQHandler(void)
{
	BaseType_t woken = pdFALSE;
	uint32_t stat = REG32(LPUART4_STAT);

	g_lpflexcomm4_irq_count++;

	if ((stat & LPUART4_STAT_ERR) != 0u)
	{
		g_mb_last_stat = stat;
		s_rx_bad = 1u;
		REG32(LPUART4_STAT) = stat & LPUART4_STAT_ERR;   /* W1C the error flags only */
	}

	while ((REG32(LPUART4_STAT) & LPUART4_STAT_RDRF) != 0u)
	{
		uint8_t byte = (uint8_t)REG32(LPUART4_DATA);     /* reading DATA clears RDRF; parity not in [7:0] */
		g_mb_cyc_last_byte = REG32(DWT_CYCCNT);
		if (s_rx_owned != 0u)
		{
			g_mb_dropped++;
		}
		else if (s_rx_len < MB_BUF_SIZE)
		{
			s_rx_buf[s_rx_len++] = byte;
			g_mb_rx_bytes++;
		}
		else
		{
			s_rx_bad = 1u;                                /* > 256 B: not a Modbus frame */
		}
	}

	if ((stat & LPUART4_STAT_IDLE) != 0u)
	{
		REG32(LPUART4_STAT) = LPUART4_STAT_IDLE;          /* W1C */
		g_mb_cyc_idle = REG32(DWT_CYCCNT);
		if ((s_rx_owned == 0u) && (s_rx_len != 0u))
		{
			uint32_t item = s_rx_len | ((s_rx_bad != 0u) ? MB_FRAME_BAD : 0u);
			s_rx_owned = 1u;
			(void)xQueueSendFromISR(s_frame_q, &item, &woken);
		}
	}

	portYIELD_FROM_ISR(woken);
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

static void task_a(void *pv)                /* 6.1: LED every 500 ms; 7.1: counter only (LED = register 1) */
{
	(void)pv;
	for (;;)
	{
		g_cnt_a++;
		vTaskDelay(pdMS_TO_TICKS(500u));
	}
}

/* 7.0 D5: 0 g_cnt_a low 16 bits RO, 1 LED P4_18 RW (1 = on), 2/3 tick low/high RO, 4-7 scratch RW */
static uint16_t mb_reg_read(uint32_t r)
{
	TickType_t t;
	switch (r)
	{
	case 0u: return (uint16_t)g_cnt_a;
	case 1u: return ((REG32(GPIO4_PDOR) & (1u << 18)) == 0u) ? 1u : 0u;   /* active-low LED */
	case 2u: t = xTaskGetTickCount(); return (uint16_t)(t & 0xFFFFu);
	case 3u: t = xTaskGetTickCount(); return (uint16_t)(t >> 16);
	default: return g_mb_regs_rw[r - 4u];
	}
}

static uint32_t mb_reg_writable(uint32_t r)  /* 7.0 D12: 0, 2, 3 read-only */
{
	return ((r == 1u) || (r >= 4u)) ? 1u : 0u;
}

static void mb_reg_write(uint32_t r, uint16_t v)
{
	if (r == 1u)
	{
		if (v != 0u) { REG32(GPIO4_PCOR) = (1u << 18); }   /* low = LED on */
		else         { REG32(GPIO4_PSOR) = (1u << 18); }   /* high = LED off */
	}
	else
	{
		g_mb_regs_rw[r - 4u] = v;
	}
}

static void mb_send(uint8_t *buf, uint32_t n)  /* appends the CRC, low byte first, polled TX */
{
	uint16_t crc = mb_crc16(buf, n);
	buf[n]      = (uint8_t)(crc & 0xFFu);
	buf[n + 1u] = (uint8_t)(crc >> 8);
	g_mb_cyc_reply = REG32(DWT_CYCCNT);
	for (uint32_t i = 0u; i < n + 2u; i++)
	{
		uart4_putc((char)buf[i]);
	}
}

static uint32_t mb_exception(uint8_t fc, uint8_t code)  /* 7.0 D6: App. Protocol p.15, p.48 */
{
	s_tx_buf[0] = MB_SLAVE_ADDR;
	s_tx_buf[1] = (uint8_t)(fc | 0x80u);
	s_tx_buf[2] = code;
	return 3u;
}

/* n = request length including the 2 CRC bytes. Returns the reply length without CRC (0 = no reply). Order of checks = App. Protocol
 * state diagrams: function code (01), quantity / value (03), address (02). */
static uint32_t mb_process(const uint8_t *rq, uint32_t n)
{
	uint8_t  fc    = rq[1];
	uint32_t start = ((uint32_t)rq[2] << 8) | rq[3];
	uint32_t qty   = ((uint32_t)rq[4] << 8) | rq[5];

	if (fc == 0x03u)                                        /* p.15 Read Holding Registers */
	{
		if (n != 8u) { return 0u; }                         /* malformed: no reply */
		if ((qty < 1u) || (qty > 125u)) { return mb_exception(fc, 0x03u); }
		if ((start + qty) > MB_NUM_REGS) { return mb_exception(fc, 0x02u); }
		s_tx_buf[0] = MB_SLAVE_ADDR;
		s_tx_buf[1] = fc;
		s_tx_buf[2] = (uint8_t)(qty * 2u);
		for (uint32_t i = 0u; i < qty; i++)
		{
			uint16_t v = mb_reg_read(start + i);
			s_tx_buf[3u + 2u * i]      = (uint8_t)(v >> 8);  /* big-endian, p.5 */
			s_tx_buf[3u + 2u * i + 1u] = (uint8_t)(v & 0xFFu);
		}
		return 3u + 2u * qty;
	}
	if (fc == 0x06u)                                        /* p.19 Write Single Register */
	{
		if (n != 8u) { return 0u; }
		if ((start >= MB_NUM_REGS) || (mb_reg_writable(start) == 0u)) { return mb_exception(fc, 0x02u); }
		mb_reg_write(start, (uint16_t)qty);                 /* bytes 4-5 = value */
		for (uint32_t i = 0u; i < 6u; i++) { s_tx_buf[i] = rq[i]; }   /* echo */
		return 6u;
	}
	if (fc == 0x10u)                                        /* p.30 Write Multiple Registers */
	{
		if (n < 9u) { return 0u; }
		uint32_t bc = rq[6];
		if ((qty < 1u) || (qty > 123u) || (bc != qty * 2u)) { return mb_exception(fc, 0x03u); }
		if (n != 9u + bc) { return 0u; }
		if ((start + qty) > MB_NUM_REGS) { return mb_exception(fc, 0x02u); }
		for (uint32_t i = 0u; i < qty; i++)
		{
			if (mb_reg_writable(start + i) == 0u) { return mb_exception(fc, 0x02u); }
		}
		for (uint32_t i = 0u; i < qty; i++)
		{
			mb_reg_write(start + i, (uint16_t)(((uint32_t)rq[7u + 2u * i] << 8) | rq[8u + 2u * i]));
		}
		for (uint32_t i = 0u; i < 6u; i++) { s_tx_buf[i] = rq[i]; }   /* addr, fc, start, qty */
		return 6u;
	}
	return mb_exception(fc, 0x01u);                         /* p.48 ILLEGAL FUNCTION */
}

static void task_modbus(void *pv)           /* 7.1: replaces 6.2 task_b */
{
	(void)pv;
	for (;;)
	{
		uint32_t item;
		if (xQueueReceive(s_frame_q, &item, portMAX_DELAY) == pdPASS)
		{
			uint32_t n = item & ~MB_FRAME_BAD;
			if ((item & MB_FRAME_BAD) != 0u)
			{
				g_mb_err_frames++;                          /* D10: no reply */
			}
			else if ((n < 4u) || (mb_crc16(s_rx_buf, n) != 0u))
			{
				g_mb_crc_bad++;                             /* CRC over data + CRC field = 0 */
			}
			else
			{
				uint8_t addr = s_rx_buf[0];
				g_mb_crc_rx = (uint16_t)(s_rx_buf[n - 2u] | ((uint16_t)s_rx_buf[n - 1u] << 8));
				if (addr == MB_SLAVE_ADDR)
				{
					uint32_t len = mb_process(s_rx_buf, n);   /* n includes the 2 CRC bytes */
					if (len != 0u)
					{
						if ((s_tx_buf[1] & 0x80u) != 0u) { g_mb_exc++; } else { g_mb_ok++; }
						mb_send(s_tx_buf, len);
					}
				}
				else if ((addr == 0u) && ((s_rx_buf[1] == 0x06u) || (s_rx_buf[1] == 0x10u)))
				{
					(void)mb_process(s_rx_buf, n);          /* D11: broadcast write, no reply */
				}
				else
				{
					g_mb_other_addr++;
				}
			}
			s_rx_len = 0u;
			s_rx_bad = 0u;
			s_rx_owned = 0u;                                /* hand the buffer back to the ISR */
		}
	}
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
	REG32(LPUART4_BAUD)   = LPUART4_BAUD_19200;   /* 7.1: was 0x0F00_001A (115200) */
	REG32(LPUART4_CTRL)   = LPUART4_CTRL_8E1_RX;  /* 7.1: was (1u << 19) = 8N1 TE only */

	REG32(SCB_CCR) = 0x00000201u; /* D1.2.9 p.1474: 3.0 measured 0x201; UNALIGN_TRP = 0 */
	REG32(SCB_SHCSR) = REG32(SCB_SHCSR) | SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk; /* p. 1836: D1.2.233 p. 1836 explicitly recommends using a read-modify-write sequence for SHCSR to preserve active and pending status bits while modifying enable flags. */
	REG32(DEMCR)    = REG32(DEMCR) | (1u << 24);   /* 7.1: TRCENA, so CYCCNT runs without a debugger */
	REG32(DWT_CTRL) = REG32(DWT_CTRL) | 1u;        /* 7.1: CYCCNTENA */
	__asm__ volatile ("dsb":::"memory"); // RSJQJ p.222 / RBMLS p.268
	__asm__ volatile ("isb":::"memory"); // RNPDJ / RHMNM p.222, RWQQB p.268

	/* 6.1: PendSV/SysTick priorities and SysTick itself are now set by the port
	 * (port.c 1818/1819, vPortSetupTimerInterrupt) inside vTaskStartScheduler. */

	/* 7.1: queue before the IRQ can use it; IRQ 39 at 0x20 (D9) before enabling it */
	s_frame_q = xQueueCreateStatic(MB_FRAME_Q_LEN, sizeof(uint32_t), s_frame_q_storage, &s_frame_q_struct);
	(void)xTaskCreateStatic(task_a, "TaskA", TASK_A_STACK_WORDS, NULL, TASK_A_PRIO,
	                        s_task_a_stack, &s_task_a_tcb);
	(void)xTaskCreateStatic(task_modbus, "Modbus", TASK_MB_STACK_WORDS, NULL, TASK_MB_PRIO,
	                        s_task_mb_stack, &s_task_mb_tcb);
	REG8(NVIC_IPR9_PRI_N3) = IRQ39_PRIO;         /* byte access allowed (D1.2.185 p.1769) */
	REG32(NVIC_ISER1) = NVIC_ISER1_IRQ39_SET;    /* W1S write (DDI0553 D1.2.186 p.1770) */
	__asm__ volatile ("dsb" : : : "memory");     /* RSJQJ p.222 */
	__asm__ volatile ("isb" : : : "memory");     /* RWQQB p.268 */
	vTaskStartScheduler();                   /* does not return: MSP reset, svc 102, first task */

	for (;;) {
    }
}
