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
    /* Do NOT enable interrupts - let the app decide when to enable them */
    
    /* Load new stack pointer */
    ldr sp, [r0]
    
    /* Load reset handler address */
    ldr r1, [r0, #4]
    
    /* Ensure THUMB bit is set */
    orr r1, r1, #1
    
    /* Jump to application reset handler */
    bx r1

.size jump_to_application_asm, .-jump_to_application_asm