/**
 * @file crash_dump.c
 * @brief Crash dump utility functions
 */

#include "crash_info.h"
#include "stm32h7xx_hal.h"

/**
 * @brief Check if there's a valid crash dump in backup SRAM
 * @return 1 if valid crash dump exists, 0 otherwise
 */
int crash_dump_is_valid(void)
{
    /* Enable Backup SRAM clock */
    __HAL_RCC_BKPRAM_CLK_ENABLE();
    
    /* Enable backup domain access */
    HAL_PWR_EnableBkUpAccess();
    
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Check magic number */
    if (crash_info->magic != CRASH_MAGIC_VALID) {
        return 0;
    }
    
    /* Verify checksum */
    uint32_t checksum = 0;
    uint32_t* ptr = (uint32_t*)crash_info;
    size_t words = (sizeof(crash_info_t) - sizeof(uint32_t)) / sizeof(uint32_t);
    
    for (size_t i = 0; i < words; i++) {
        checksum ^= ptr[i];
    }
    
    return (checksum == crash_info->checksum) ? 1 : 0;
}

/**
 * @brief Clear crash dump from backup SRAM
 */
void crash_dump_clear(void)
{
    /* Enable Backup SRAM clock */
    __HAL_RCC_BKPRAM_CLK_ENABLE();
    
    /* Enable backup domain access */
    HAL_PWR_EnableBkUpAccess();
    
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Clear magic number */
    crash_info->magic = CRASH_MAGIC_CLEAR;
    
    /* Memory barrier */
    __DSB();
}

/**
 * @brief Get crash info (if valid)
 * @param info Pointer to structure to fill
 * @return 1 if valid crash info was copied, 0 otherwise
 */
int crash_dump_get_info(crash_info_t* info)
{
    if (!crash_dump_is_valid()) {
        return 0;
    }
    
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Copy entire structure */
    *info = *crash_info;
    
    return 1;
}

/**
 * @brief Force a test crash (HardFault)
 * WARNING: This will crash the system!
 */
void crash_dump_test(void)
{
    /* Disable interrupts to ensure clean crash */
    __disable_irq();
    
    /* Force a HardFault by accessing invalid address */
    *((volatile uint32_t*)0xFFFFFFFF) = 0xDEADBEEF;
    
    /* Should never reach here */
    while(1);
}

/**
 * @brief Force a divide by zero crash (UsageFault)
 * WARNING: This will crash the system!
 */
void crash_dump_test_divzero(void)
{
    /* Enable divide by zero trap */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    
    /* Disable interrupts to ensure clean crash */
    __disable_irq();
    
    /* Force divide by zero */
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;  /* This will trigger UsageFault */
    
    /* Should never reach here */
    (void)c;
    while(1);
}

/**
 * @brief Force an unaligned access crash (UsageFault)
 * WARNING: This will crash the system!
 */
void crash_dump_test_unaligned(void)
{
    /* Enable unaligned access trap */
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    
    /* Disable interrupts to ensure clean crash */
    __disable_irq();
    
    /* Force unaligned access */
    volatile uint32_t *ptr = (uint32_t*)0x20000001;  /* Odd address */
    volatile uint32_t val = *ptr;  /* This will trigger UsageFault */
    
    /* Should never reach here */
    (void)val;
    while(1);
}

/**
 * @brief Force an undefined instruction crash (UsageFault)
 * WARNING: This will crash the system!
 */
void crash_dump_test_undefined(void)
{
    /* Disable interrupts to ensure clean crash */
    __disable_irq();
    
    /* Execute undefined instruction */
    __asm volatile (".word 0xDE00");  /* Undefined Thumb instruction */
    
    /* Should never reach here */
    while(1);
}