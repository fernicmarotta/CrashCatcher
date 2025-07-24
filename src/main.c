#include "stm32h7xx_hal.h"
#include "crash_info.h"
#include "network_init.h"
#include "tcp_crash_dump_uip.h"
#include <string.h>

/* Application start address */
#define APPLICATION_ADDRESS     0x08020000

/* NVIC parameters */
#define NVIC_NUM_INTERRUPTS    240
#define NVIC_PRIO_REGISTERS    8

/* System clock configuration - minimal for bootloader */
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* Configure power supply */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    
    /* Configure HSE and PLL for 400MHz */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;   // HSE = 25MHz, so 25/5 = 5MHz
    RCC_OscInitStruct.PLL.PLLN = 160; // 5MHz * 160 = 800MHz VCO
    RCC_OscInitStruct.PLL.PLLP = 2;   // 800MHz / 2 = 400MHz SYSCLK
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        while (1);
    }
    
    /* Configure clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        while (1);
    }
}

/* Jump to application */
static void jump_to_application(void) {
    uint32_t app_address = APPLICATION_ADDRESS;
    
    /* Disable all interrupts */
    __disable_irq();
    
    /* Disable all NVIC interrupts and clear pending */
    for (int i = 0; i < NVIC_PRIO_REGISTERS; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  /* Disable all interrupts */
        NVIC->ICPR[i] = 0xFFFFFFFF;  /* Clear all pending interrupts */
    }
    
    /* Clear all interrupt priorities */
    for (int i = 0; i < NVIC_NUM_INTERRUPTS; i++) {
        NVIC->IP[i] = 0;
    }
    
    /* Disable SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* Clear any pending SysTick interrupt */
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
    
    /* Clear all other pending interrupts in ICSR */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    
    /* Reset CONTROL register - switch to MSP, Thread mode, no FP active */
    __set_CONTROL(0);
    
    /* Clear BASEPRI to enable all interrupts (after we jump) */
    __set_BASEPRI(0);
    
    /* Clear FAULTMASK */
    __set_FAULTMASK(0);
    
    /* Clear PRIMASK - this will be set again by __disable_irq() but we clear it for clean state */
    __set_PRIMASK(0);
    
    /* Reset peripherals */
    HAL_RCC_DeInit();
    HAL_DeInit();
    
    /* Set vector table to application */
    SCB->VTOR = app_address;
    
    /* Ensure all memory accesses are completed */
    __DSB();
    __ISB();
    
    /* Set MSP and jump using assembly */
    extern void jump_to_application_asm(uint32_t address);
    jump_to_application_asm(app_address);
}

int main(void) {
    crash_info_t *crash_info = (crash_info_t *)BACKUP_SRAM_BASE;
    uint32_t timeout = 30000; /* 30 second timeout */
    uint32_t start_time;
    
    /* Initialize HAL */
    HAL_Init();
    
    /* Configure system clock */
    SystemClock_Config();
    
    /* Check if we have a valid crash dump */
    if (crash_info->magic == CRASH_MAGIC_VALID) {
        /* Initialize network */
        if (network_init() == 0) {
            /* Start crash dump transmission */
            if (tcp_crash_dump_start(crash_info) == 0) {
                /* Wait for dump to complete or timeout */
                start_time = HAL_GetTick();
                
                while (!tcp_crash_dump_is_complete()) {
                    /* Process network packets */
                    network_process();
                    
                    /* Check timeout */
                    if ((HAL_GetTick() - start_time) > timeout) {
                        break;
                    }
                }
            }
        }
        
        /* Clear crash info */
        crash_info->magic = 0;
    }
    
    /* Jump to application */
    jump_to_application();
    
    /* Should never reach here */
    while (1);
}