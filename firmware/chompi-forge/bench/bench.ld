/* Emulator-only layout for CPU benchmarks (never flashed). Regions mirror the
   STM32H750 address map so DSY_SDRAM_BSS / DTCM attributes land correctly. */
MEMORY
{
    CODE (rwx)  : ORIGIN = 0x24000000, LENGTH = 4M
    DTCM (rw)   : ORIGIN = 0x20000000, LENGTH = 112K
    SDRAM (rw)  : ORIGIN = 0xC0000000, LENGTH = 64M
}
SECTIONS
{
    .text : { *(.text*) *(.rodata*) *(.glue_7*) *(.init*) *(.fini*) } > CODE
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > CODE
    .data : { *(.data*) } > CODE
    .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) __bss_end__ = .; } > CODE
    .dtcm (NOLOAD) : { *(.dtcmram_bss*) *(.dtcm_bss*) } > DTCM
    .sdram (NOLOAD) : { *(.sdram_bss*) *(.sdram_data*) } > SDRAM
    end = .;
}
