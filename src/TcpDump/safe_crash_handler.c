/**
 * @file safe_crash_handler.c
 * @brief Safe crash handler that disables interrupts and DMA before dumping
 */

#include "stm32h7xx.h"
#include "CrashCatcher.h"

/**
 * @brief Safe entry point that disables all interrupts and DMA
 * 
 * This function should be called BEFORE CrashCatcher_Entry to ensure
 * that no DMA or interrupt can corrupt the RAM during dump.
 */
void Safe_CrashCatcher_Entry(void) {
    /* 1. Disable ALL interrupts immediately */
    __disable_irq();
    
    /* 2. Disable all DMA controllers */
    /* DMA1 and DMA2 in D2 domain */
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN);
    
    /* MDMA in D1 domain */
    RCC->AHB3ENR &= ~RCC_AHB3ENR_MDMAEN;
    
    /* BDMA in D3 domain */
    RCC->AHB4ENR &= ~RCC_AHB4ENR_BDMAEN;
    
    /* 3. Disable Ethernet DMA if it was enabled */
    if (RCC->AHB1ENR & RCC_AHB1ENR_ETH1MACEN) {
        /* Stop Ethernet DMA */
        ETH->DMAMR |= ETH_DMAMR_SWR;  /* Software reset */
        
        /* Wait for reset to complete */
        while (ETH->DMAMR & ETH_DMAMR_SWR) {
            __NOP();
        }
        
        /* Disable Ethernet clocks */
        RCC->AHB1ENR &= ~(RCC_AHB1ENR_ETH1MACEN | 
                          RCC_AHB1ENR_ETH1TXEN | 
                          RCC_AHB1ENR_ETH1RXEN);
    }
    
    /* 4. Disable other dangerous peripherals */
    /* USB OTG - can do DMA */
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_USB1OTGHSEN | 
                      RCC_AHB1ENR_USB1OTGHSULPIEN |
                      RCC_AHB1ENR_USB2OTGHSEN |
                      RCC_AHB1ENR_USB2OTGHSULPIEN);
    
    /* SDMMC - has internal DMA */
    RCC->AHB3ENR &= ~(RCC_AHB3ENR_SDMMC1EN);
    RCC->APB4ENR &= ~(RCC_APB4ENR_SDMMC2EN);
    
    /* 5. Optional: Save critical RAM to safe location before network init */
    /* This ensures we have a clean copy even if network init corrupts something */
    #ifdef SAVE_CRITICAL_RAM_FIRST
    extern uint32_t _sdata, _edata;  /* Data section */
    extern uint32_t _sbss, _ebss;    /* BSS section */
    static uint8_t critical_ram_backup[32*1024] __attribute__((section(".backup_ram")));
    
    /* Save data section */
    uint32_t data_size = (uint32_t)&_edata - (uint32_t)&_sdata;
    if (data_size <= sizeof(critical_ram_backup)) {
        memcpy(critical_ram_backup, &_sdata, data_size);
    }
    #endif
    
    /* 6. Now safe to call original CrashCatcher */
    /* Cast to void to call the original function */
    ((void (*)(void))CrashCatcher_Entry)();
}

/**
 * @brief Modified HardFault handler that calls safe entry
 * 
 * Add this to your assembly file or replace the existing handler
 */
__attribute__((naked))
void Safe_HardFault_Handler(void) {
    __asm volatile(
        /* Disable interrupts IMMEDIATELY */
        "cpsid i                                        \n"
        
        /* Save registers as CrashCatcher expects */
        "mrs     r3, xpsr                               \n"
        "mrs     r2, psp                                \n"
        "mrs     r1, msp                                \n"
        "ldr     sp, =(g_crashCatcherStack + 4 * 64)    \n"  /* CRASH_CATCHER_STACK_WORD_COUNT = 64 */
        "push.w  {r1-r11,lr}                            \n"
        
        /* Call safe entry point */
        "mov     r0, sp                                 \n"
        "bl      Safe_CrashCatcher_Entry                \n"
        
        /* Should never return, but just in case */
        "1: b    1b                                     \n"
    );
}