/**
 * @file pe_hardfault_integration.h
 * @brief Integration header for PE hardfault with core dump
 */

#ifndef PE_HARDFAULT_INTEGRATION_H
#define PE_HARDFAULT_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trigger core dump instead of system reset
 * 
 * This function should be called from pe_hardfault.c instead of system_reset()
 * It will:
 * 1. Save crash info to backup SRAM
 * 2. Reset the system
 * 3. Bootloader will detect the crash and send dump via TCP
 */
void pe_hardfault_trigger_coredump(void);

/**
 * @brief Check if we should override the default pe_hardfault behavior
 * 
 * Define PE_HARDFAULT_USE_COREDUMP in your project to enable core dump
 */
#ifdef PE_HARDFAULT_USE_COREDUMP
    /* Override the system_reset function */
    #define system_reset() pe_hardfault_trigger_coredump()
#endif

#ifdef __cplusplus
}
#endif

#endif /* PE_HARDFAULT_INTEGRATION_H */