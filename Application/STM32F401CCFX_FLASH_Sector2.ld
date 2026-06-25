/* =============================================================================
 * STM32F401CC — Custom Linker Script for Application in Sector 2
 *
 * Flash layout (STM32F401CC, 256 KB total):
 *   Sector 0 : 0x08000000  16 KB  -> Bootloader
 *   Sector 1 : 0x08004000  16 KB  -> Bootloader
 *   Sector 2 : 0x08008000  16 KB  -> App starts HERE  <-- ORIGIN
 *   Sector 3 : 0x0800C000  16 KB  -> App
 *   Sector 4 : 0x08010000  64 KB  -> App
 *   Sector 5 : 0x08020000 128 KB  -> App
 *                          ------
 *   Available to app       224 KB  (LENGTH below)
 * =============================================================================
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x400;   /* 1 KB heap  */
_Min_Stack_Size = 0x800;   /* 2 KB stack */

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 224K  /* Sectors 2-5 */
}

SECTIONS
{
  /* ---------- Interrupt vector table (must be first) ---------- */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ---------- Code ---------- */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* ---------- Read-only data ---------- */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* ---------- Initialised data (LMA in FLASH, VMA in RAM) ---------- */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* ---------- Zero-initialised data ---------- */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* ---------- Heap / stack size check ---------- */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end  = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* ---------- Discard C library object files ---------- */
  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
