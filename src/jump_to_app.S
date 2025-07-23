.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

.text
.align 2
.global jump_to_application_asm
.thumb
.thumb_func
.type jump_to_application_asm, %function

jump_to_application_asm:
    /* r0 contains the application address */
    ldr sp, [r0]        /* Load stack pointer from vector table */
    ldr pc, [r0, #04]   /* Load PC with reset handler - exactly like working bootloader */

.size jump_to_application_asm, .-jump_to_application_asm