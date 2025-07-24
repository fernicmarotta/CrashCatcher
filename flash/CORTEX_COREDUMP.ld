/* Entry Point */
ENTRY(Reset_Handler);

/* Highest address of the user mode stack - ONLY IN ITCM! */
_estack = ORIGIN(ITCMRAM) + LENGTH(ITCMRAM);

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size =  0x2000; /* 8KB heap */
_Min_Stack_Size = 0x2000; /* 8KB stack */

MEMORY
{
    /* Bootloader flash region - starts at 0x08000000 */
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 128K
    
    /* ONLY use ITCM RAM - 64KB */
    ITCMRAM (rwx)   : ORIGIN = 0x00000000, LENGTH = 64K
    
    /* Define but DO NOT USE these regions - they contain crash data! */
    DTCMRAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K   /* PRESERVED */
    AXI_SRAM (rwx)  : ORIGIN = 0x24000000, LENGTH = 512K   /* PRESERVED */
    SRAM1 (rwx)     : ORIGIN = 0x30000000, LENGTH = 128K   /* PRESERVED */
    SRAM2 (rwx)     : ORIGIN = 0x30020000, LENGTH = 128K   /* PRESERVED */
    SRAM3 (rwx)     : ORIGIN = 0x30040000, LENGTH = 32K    /* PRESERVED */
    SRAM4 (rwx)     : ORIGIN = 0x38000000, LENGTH = 64K    /* PRESERVED */
    BACKUP_SRAM (rwx) : ORIGIN = 0x38800000, LENGTH = 4K   /* PRESERVED */
}

SECTIONS
{
    /* ISR Vector table */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ARM exception handling */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* C++ constructors */
    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > FLASH

    /* Used by startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data - goes to ITCM ONLY */
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
    } > ITCMRAM AT> FLASH

    /* Uninitialized data - in ITCM ONLY */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > ITCMRAM

    /* User heap and stack - in ITCM ONLY */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > ITCMRAM
    
    /* Ethernet DMA descriptors in SRAM3 */
    .RxDecripSection 0x30040000 (NOLOAD) :
    {
        . = ALIGN(4);
        *(.RxDecripSection)
        . = ALIGN(4);
    } > SRAM3
    
    .TxDecripSection 0x30040060 (NOLOAD) :
    {
        . = ALIGN(4);
        *(.TxDecripSection)
        . = ALIGN(4);
    } > SRAM3
    
    .RxArraySection 0x30040200 (NOLOAD) :
    {
        . = ALIGN(4);
        *(.RxArraySection)
        . = ALIGN(4);
    } > SRAM3
    
    .TxArraySection 0x30042000 (NOLOAD) :
    {
        . = ALIGN(4);
        *(.TxArraySection)
        . = ALIGN(4);
    } > SRAM3

    /* Remove information from the standard libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}