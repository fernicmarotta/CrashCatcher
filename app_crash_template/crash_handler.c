/**
 * @file crash_handler.c
 * @brief Complete crash handler implementation for STM32H7
 * 
 * Captures full system state on HardFault and saves to backup SRAM
 */

#include "crash_info.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>

/* Assembly function to capture all registers */
void HardFault_Handler(void) __attribute__((naked));

/**
 * @brief C handler called from assembly with complete register context
 * @param stack_frame Pointer to exception stack frame
 * @param exc_return Exception return value (LR)
 * @param r4_r11 Array containing r4-r11 values
 */
void crash_handler_save_context(uint32_t* stack_frame, uint32_t exc_return, uint32_t* r4_r11)
{
    /* Enable Backup SRAM clock */
    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    
    /* Enable backup domain access */
    PWR->CR1 |= PWR_CR1_DBP;
    
    /* Get pointer to backup SRAM */
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Set magic marker */
    crash_info->magic = CRASH_MAGIC_VALID;
    crash_info->crash_type = CRASH_TYPE_HARDFAULT;
    
    /* Timestamp and boot count */
    crash_info->timestamp = HAL_GetTick();
    crash_info->boot_count = 0;
    
    /* Extract registers from stack frame */
    crash_info->r0 = stack_frame[0];
    crash_info->r1 = stack_frame[1];
    crash_info->r2 = stack_frame[2];
    crash_info->r3 = stack_frame[3];
    crash_info->r12 = stack_frame[4];
    crash_info->lr = stack_frame[5];
    crash_info->pc = stack_frame[6];
    crash_info->psr = stack_frame[7];
    
    /* Save r4-r11 from array */
    crash_info->r4 = r4_r11[0];
    crash_info->r5 = r4_r11[1];
    crash_info->r6 = r4_r11[2];
    crash_info->r7 = r4_r11[3];
    crash_info->r8 = r4_r11[4];
    crash_info->r9 = r4_r11[5];
    crash_info->r10 = r4_r11[6];
    crash_info->r11 = r4_r11[7];
    
    /* Save stack pointers */
    crash_info->msp = __get_MSP();
    crash_info->psp = __get_PSP();
    
    /* Determine which stack was used */
    if (exc_return & 0x04) {
        /* Thread mode, PSP was used */
        crash_info->sp = crash_info->psp;
    } else {
        /* Handler mode, MSP was used */
        crash_info->sp = crash_info->msp;
    }
    
    /* Save exception return value */
    crash_info->lr_at_fault = exc_return;
    
    /* Read fault status registers */
    crash_info->cfsr = SCB->CFSR;
    crash_info->hfsr = SCB->HFSR;
    crash_info->dfsr = SCB->DFSR;
    crash_info->mmfar = SCB->MMFAR;
    crash_info->bfar = SCB->BFAR;
    crash_info->afsr = SCB->AFSR;
    
    /* Application version - set your version here */
    crash_info->app_version = 0x01000000; /* 1.0.0.0 */
    
    /* Simple checksum (XOR of all fields except checksum itself) */
    uint32_t checksum = 0;
    uint32_t* ptr = (uint32_t*)crash_info;
    size_t words = (sizeof(crash_info_t) - sizeof(uint32_t)) / sizeof(uint32_t);
    
    for (size_t i = 0; i < words; i++) {
        checksum ^= ptr[i];
    }
    crash_info->checksum = checksum;

    SCB_CleanInvalidateDCache();

    /* System reset to jump to bootloader */
    NVIC_SystemReset();
}

/**
 * @brief HardFault handler - captures all registers and calls C handler
 * 
 * This naked function captures the complete CPU state including
 * registers that are not automatically stacked by the exception entry
 */
void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4                \n"  /* Test bit 2 of EXC_RETURN */
        "ite eq                    \n"
        "mrseq r0, msp             \n"  /* If 0, use MSP */
        "mrsne r0, psp             \n"  /* If 1, use PSP */
        "mov r1, lr                \n"  /* Pass EXC_RETURN as second parameter */
        "push {r4-r11}             \n"  /* Save r4-r11 on stack */
        "mov r2, sp                \n"  /* Pass pointer to saved r4-r11 */
        "b crash_handler_save_context  \n"
    );
}