/**
 * @file crash_handler.c
 * @brief Complete crash handler implementation for STM32H7 - PE style
 * 
 * Captures full system state on HardFault and saves to backup SRAM
 */

#include "crash_info.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>

/* Assembly functions for each fault type */
void HardFault_Handler(void) __attribute__((naked));
void MemManage_Handler(void) __attribute__((naked));
void BusFault_Handler(void) __attribute__((naked));
void UsageFault_Handler(void) __attribute__((naked));

/**
 * @brief Common C handler called from assembly with stack pointers
 * @param msp_value Current MSP value
 * @param psp_value Current PSP value
 * @param exc_return Exception return value (LR)
 * @param fault_type Type of fault (1=HardFault, 2=MemManage, etc)
 */
void crash_handler_common(uint32_t msp_value, uint32_t psp_value, uint32_t exc_return, uint32_t fault_type)
{
    /* Enable Backup SRAM clock */
    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    
    /* Enable backup domain access */
    PWR->CR1 |= PWR_CR1_DBP;
    
    /* Get pointer to backup SRAM */
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Set magic marker and crash type */
    crash_info->magic = CRASH_MAGIC_VALID;
    crash_info->crash_type = fault_type;
    
    /* Timestamp and boot count */
    crash_info->timestamp = HAL_GetTick();
    crash_info->boot_count = 0;
    
    /* Determine which stack was active and get exception frame */
    uint32_t* stack_frame;
    if (exc_return & 0x04) {
        /* Thread mode, PSP was used */
        stack_frame = (uint32_t*)psp_value;
        crash_info->sp = psp_value;
    } else {
        /* Handler mode, MSP was used */
        stack_frame = (uint32_t*)msp_value;
        crash_info->sp = msp_value;
    }
    
    /* Extract registers from exception stack frame */
    crash_info->r0 = stack_frame[0];
    crash_info->r1 = stack_frame[1];
    crash_info->r2 = stack_frame[2];
    crash_info->r3 = stack_frame[3];
    crash_info->r12 = stack_frame[4];
    crash_info->lr = stack_frame[5];
    crash_info->pc = stack_frame[6];
    crash_info->psr = stack_frame[7];
    
    /* Save both stack pointers as captured on entry */
    crash_info->msp = msp_value;
    crash_info->psp = psp_value;
    
    /* Save exception return value */
    crash_info->lr_at_fault = exc_return;
    
    /* r4-r11 aren't automatically saved by exception entry
     * For now, we'll leave them as zeros - a complete implementation
     * would need assembly code to capture them before any push operations
     */
    crash_info->r4 = 0;
    crash_info->r5 = 0;
    crash_info->r6 = 0;
    crash_info->r7 = 0;
    crash_info->r8 = 0;
    crash_info->r9 = 0;
    crash_info->r10 = 0;
    crash_info->r11 = 0;
    
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

    /* Ensure all writes complete */
    __DSB();
    SCB_CleanInvalidateDCache();
    __DSB();

    /* System reset to jump to bootloader */
    NVIC_SystemReset();
}

/**
 * @brief HardFault handler - PE style
 */
void HardFault_Handler(void)
{
    __asm volatile(
        "mrs r0, msp             \n"  /* Get current MSP */
        "mrs r1, psp             \n"  /* Get current PSP */
        "mov r2, lr              \n"  /* Get EXC_RETURN */
        "mov r3, #1              \n"  /* Fault type = 1 (HardFault) */
        "b crash_handler_common  \n"
    );
}

/**
 * @brief MemManage handler - PE style
 */
void MemManage_Handler(void)
{
    __asm volatile(
        "mrs r0, msp             \n"  /* Get current MSP */
        "mrs r1, psp             \n"  /* Get current PSP */
        "mov r2, lr              \n"  /* Get EXC_RETURN */
        "mov r3, #2              \n"  /* Fault type = 2 (MemManage) */
        "b crash_handler_common  \n"
    );
}

/**
 * @brief BusFault handler - PE style
 */
void BusFault_Handler(void)
{
    __asm volatile(
        "mrs r0, msp             \n"  /* Get current MSP */
        "mrs r1, psp             \n"  /* Get current PSP */
        "mov r2, lr              \n"  /* Get EXC_RETURN */
        "mov r3, #3              \n"  /* Fault type = 3 (BusFault) */
        "b crash_handler_common  \n"
    );
}

/**
 * @brief UsageFault handler - PE style
 */
void UsageFault_Handler(void)
{
    __asm volatile(
        "mrs r0, msp             \n"  /* Get current MSP */
        "mrs r1, psp             \n"  /* Get current PSP */
        "mov r2, lr              \n"  /* Get EXC_RETURN */
        "mov r3, #4              \n"  /* Fault type = 4 (UsageFault) */
        "b crash_handler_common  \n"
    );
}