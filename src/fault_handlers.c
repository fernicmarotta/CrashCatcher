#include "stm32h7xx_hal.h"
#include "fault_handlers.h"

/**
 * @brief Trigger software reset using NVIC
 */
static void trigger_software_reset(void) {
    /* Disable all interrupts */
    __disable_irq();
    
    /* Clean D-Cache to ensure all modified data is written back to RAM */
    SCB_CleanDCache();
    
    /* Invalidate D-Cache to ensure no stale data remains */
    SCB_InvalidateDCache();
    
    /* Invalidate I-Cache as well */
    SCB_InvalidateICache();
    
    /* Disable both caches before reset to ensure bootloader starts clean */
    SCB_DisableDCache();
    SCB_DisableICache();
    
    /* Final barrier to ensure all operations complete */
    __DSB();
    __ISB();
    
    /* Clear any pending RCC reset flags */
    __HAL_RCC_CLEAR_RESET_FLAGS();
    
    /* Trigger system reset using NVIC */
    NVIC_SystemReset();
    
    /* Should never reach here */
    while(1);
}

/**
 * @brief HardFault Handler - Just reset
 */
void HardFault_Handler(void) {
    trigger_software_reset();
}

/**
 * @brief MemManage Handler - Just reset
 */
void MemManage_Handler(void) {
    trigger_software_reset();
}

/**
 * @brief BusFault Handler - Just reset
 */
void BusFault_Handler(void) {
    trigger_software_reset();
}

/**
 * @brief UsageFault Handler - Just reset
 */
void UsageFault_Handler(void) {
    trigger_software_reset();
}

/**
 * @brief Debug Monitor Handler - Just reset
 */
void DebugMon_Handler(void) {
    trigger_software_reset();
}