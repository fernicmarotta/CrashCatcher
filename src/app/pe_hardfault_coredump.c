/**
 * @file pe_hardfault_coredump.c
 * @brief Integration of PE hardfault handler with CrashCatcher core dump
 * 
 * This replaces the system_reset() call with CrashCatcher dump
 */

#include "pe_hardfault_types.h"
#include "crash_info.h"
#include "CrashCatcher.h"
#include <stm32h7xx.h>

/* External reference to hardfault data structure */
extern pe_hardfault_t hf;

/**
 * @brief Save crash info to backup SRAM for bootloader
 */
static void save_crash_to_backup_sram(void)
{
    /* Enable Backup SRAM clock */
    __HAL_RCC_BKPRAM_CLK_ENABLE();
    
    /* Enable backup domain access */
    HAL_PWR_EnableBkUpAccess();
    
    /* Get pointer to backup SRAM */
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    
    /* Fill crash info from pe_hardfault data */
    crash_info->magic = CRASH_MAGIC_VALID;
    crash_info->crash_type = CRASH_TYPE_HARDFAULT;
    
    /* Set timestamp and boot count */
    crash_info->timestamp = HAL_GetTick();
    crash_info->boot_count = 0; /* Will be incremented by bootloader */
    
    /* Copy registers from pe_hardfault structure */
    crash_info->r0 = hf.info.r0;
    crash_info->r1 = hf.info.r1;
    crash_info->r2 = hf.info.r2;
    crash_info->r3 = hf.info.r3;
    crash_info->r4 = 0; /* Not captured by pe_hardfault */
    crash_info->r5 = 0;
    crash_info->r6 = 0;
    crash_info->r7 = 0;
    crash_info->r8 = 0;
    crash_info->r9 = 0;
    crash_info->r10 = 0;
    crash_info->r11 = 0;
    crash_info->r12 = hf.info.r12;
    crash_info->lr = hf.info.lr;
    crash_info->pc = hf.info.pc;
    crash_info->psr = hf.info.psr;
    
    /* Set stack pointers */
    crash_info->msp = (uint32_t)__get_MSP();
    crash_info->psp = (uint32_t)__get_PSP();
    crash_info->sp = (hf.info.lr_exc_return & 0x04) ? crash_info->psp : crash_info->msp;
    
    /* Copy fault registers */
    crash_info->cfsr = hf.info.cfsr;
    crash_info->hfsr = hf.info.hfsr;
    crash_info->dfsr = hf.info.dfsr;
    crash_info->mmfar = hf.info.memmanage_fault_address;
    crash_info->bfar = hf.info.bus_fault_address;
    crash_info->afsr = hf.info.afsr;
    
    /* Additional info */
    crash_info->lr_at_fault = hf.info.lr_exc_return;
    crash_info->app_version = 0; /* Set your app version here */
    
    /* Calculate checksum of the structure (excluding checksum field) */
    crash_info->checksum = 0; /* TODO: Implement CRC32 calculation */
    
    /* Ensure all writes complete */
    __DSB();
    __ISB();
}

/**
 * @brief Modified hardfault handler that triggers CrashCatcher
 * 
 * This function should be called instead of system_reset() in hardfault_type_handler
 */
void pe_hardfault_trigger_coredump(void)
{
    /* Save crash info to backup SRAM first */
    save_crash_to_backup_sram();
    
    /* Option 1: Trigger CrashCatcher to dump via HexDump (original mode) */
    /* CrashCatcher_Entry(); */
    
    /* Option 2: Just reset and let bootloader handle the dump via TCP */
    NVIC_SystemReset();
}

/**
 * @brief Integration point - replace system_reset() call
 * 
 * In pe_hardfault.c, replace:
 *   system_reset();
 * With:
 *   pe_hardfault_trigger_coredump();
 */