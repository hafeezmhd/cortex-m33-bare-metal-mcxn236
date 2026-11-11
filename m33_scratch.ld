/* m33_scratch.ld: hand-written linker script, step 1a.3
 * Sources: RM Rev.4 Table 16/17 (log 0.3), guide 1.4, 1.5 
 * step 1a.3, + .data/.bss rules step 1b.2*/

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1M   /* NS alias, 2 x 512 KB banks */
  RAMB  (rwx) : ORIGIN = 0x20008000, LENGTH = 32K  /* one physical RAM block */
}

/* Initial stack pointer: top of RAMB = 0x2001_0000 (grows down, stays in RAMB) */
__stack_top = ORIGIN(RAMB) + LENGTH(RAMB);

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : ALIGN(4)
  {
    __data_start__ = .;
    *(.data*)
    . = ALIGN(4);
    __data_end__ = .;
  } > RAMB AT > FLASH
  __data_load__ = LOADADDR(.data);

  .bss : ALIGN(4)
  {
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAMB
}