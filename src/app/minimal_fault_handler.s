/**
 * @file minimal_fault_handler.s
 * @brief Minimal fault handlers that save crash info to Backup SRAM and reset
 * 
 * These handlers run with interrupts disabled and do minimal work:
 * 1. Save critical registers to Backup SRAM
 * 2. Trigger immediate system reset
 * 
 * The bootloader will detect the crash and send the full dump.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .section .text

/**
 * HardFault Handler - Save minimal info and reset
 */
    .global HardFault_Handler
    .type HardFault_Handler, %function
    .thumb_func
HardFault_Handler:
    /* Disable interrupts */
    cpsid   i
    
    /* Load Backup SRAM base address */
    ldr     r0, =0x38800000
    
    /* Save magic number */
    ldr     r1, =0xDEADC0DE
    str     r1, [r0, #0x00]
    
    /* Save crash type (1 = HardFault) */
    mov     r1, #1
    str     r1, [r0, #0x04]
    
    /* Check which stack was used (MSP or PSP) */
    mov     r1, lr
    ldr     r2, =0x4
    tst     r1, r2
    beq     1f
    
    /* PSP was used */
    mrs     r1, psp
    b       2f
1:
    /* MSP was used */
    mrs     r1, msp
2:
    /* r1 now contains the stack pointer before exception */
    
    /* Save stacked registers from exception frame */
    ldr     r2, [r1, #0]    /* r0 */
    str     r2, [r0, #0x10]
    ldr     r2, [r1, #4]    /* r1 */
    str     r2, [r0, #0x14]
    ldr     r2, [r1, #8]    /* r2 */
    str     r2, [r0, #0x18]
    ldr     r2, [r1, #12]   /* r3 */
    str     r2, [r0, #0x1C]
    ldr     r2, [r1, #16]   /* r12 */
    str     r2, [r0, #0x40]
    ldr     r2, [r1, #20]   /* lr */
    str     r2, [r0, #0x48]
    ldr     r2, [r1, #24]   /* pc - THIS IS WHERE IT CRASHED */
    str     r2, [r0, #0x4C]
    ldr     r2, [r1, #28]   /* psr */
    str     r2, [r0, #0x50]
    
    /* Save current stack pointer */
    str     r1, [r0, #0x44]
    
    /* Save MSP and PSP */
    mrs     r2, msp
    str     r2, [r0, #0x54]
    mrs     r2, psp
    str     r2, [r0, #0x58]
    
    /* Save fault status registers */
    ldr     r1, =0xE000ED28     /* CFSR */
    ldr     r2, [r1]
    str     r2, [r0, #0x5C]
    
    ldr     r1, =0xE000ED2C     /* HFSR */
    ldr     r2, [r1]
    str     r2, [r0, #0x60]
    
    ldr     r1, =0xE000ED30     /* DFSR */
    ldr     r2, [r1]
    str     r2, [r0, #0x64]
    
    ldr     r1, =0xE000ED34     /* MMFAR */
    ldr     r2, [r1]
    str     r2, [r0, #0x68]
    
    ldr     r1, =0xE000ED38     /* BFAR */
    ldr     r2, [r1]
    str     r2, [r0, #0x6C]
    
    /* Save LR at fault entry */
    mov     r1, lr
    str     r1, [r0, #0x74]
    
    /* Trigger system reset */
    ldr     r0, =0xE000ED0C     /* AIRCR */
    ldr     r1, =0x05FA0004     /* VECTKEY | SYSRESETREQ */
    str     r1, [r0]
    dsb                         /* Ensure write completes */
    
    /* Wait for reset */
3:  b       3b

    .size HardFault_Handler, .-HardFault_Handler

/**
 * MemManage Handler
 */
    .global MemManage_Handler
    .type MemManage_Handler, %function
    .thumb_func
MemManage_Handler:
    /* Disable interrupts */
    cpsid   i
    
    /* Load Backup SRAM base address */
    ldr     r0, =0x38800000
    
    /* Save magic and crash type (2 = MemManage) */
    ldr     r1, =0xDEADC0DE
    str     r1, [r0, #0x00]
    mov     r1, #2
    str     r1, [r0, #0x04]
    
    /* Jump to common handler */
    b       HardFault_Handler.L2

/**
 * BusFault Handler
 */
    .global BusFault_Handler
    .type BusFault_Handler, %function
    .thumb_func
BusFault_Handler:
    /* Disable interrupts */
    cpsid   i
    
    /* Load Backup SRAM base address */
    ldr     r0, =0x38800000
    
    /* Save magic and crash type (3 = BusFault) */
    ldr     r1, =0xDEADC0DE
    str     r1, [r0, #0x00]
    mov     r1, #3
    str     r1, [r0, #0x04]
    
    /* Jump to common handler */
    b       HardFault_Handler.L2

/**
 * UsageFault Handler
 */
    .global UsageFault_Handler
    .type UsageFault_Handler, %function
    .thumb_func
UsageFault_Handler:
    /* Disable interrupts */
    cpsid   i
    
    /* Load Backup SRAM base address */
    ldr     r0, =0x38800000
    
    /* Save magic and crash type (4 = UsageFault) */
    ldr     r1, =0xDEADC0DE
    str     r1, [r0, #0x00]
    mov     r1, #4
    str     r1, [r0, #0x04]
    
    /* Jump to common handler */
    b       HardFault_Handler.L2

/* Label for common fault handling code */
HardFault_Handler.L2:
    b       2b

    .end