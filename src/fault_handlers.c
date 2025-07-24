#include "stm32h7xx_hal.h"
#include "fault_handlers.h"

/**
 * @brief Trigger software reset using NVIC
 */
static void trigger_software_reset(void) {
    /* Disable all interrupts */
    __disable_irq();
    
    /* Only clean/invalidate caches if they are enabled */
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        /* D-Cache is enabled, clean and invalidate it */
        SCB_CleanDCache();
        SCB_InvalidateDCache();
        SCB_DisableDCache();
    }
    
    if (SCB->CCR & SCB_CCR_IC_Msk) {
        /* I-Cache is enabled, invalidate it */
        SCB_InvalidateICache();
        SCB_DisableICache();
    }
    
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