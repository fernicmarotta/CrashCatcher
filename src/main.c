#include "stm32h7xx_hal.h"
#include "crash_info.h"
#include "network_init.h"
#include "tcp_crash_dump.h"
#include "timer.h"
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

/* State machine states */
typedef enum {
    STATE_INIT,
    STATE_ARP_ANNOUNCE,
    STATE_ARP_WAIT,
    STATE_TCP_START,
    STATE_TCP_SENDING,
    STATE_TCP_RETRY_WAIT,
    STATE_DONE
} bootloader_state_t;

int main(void) {
    crash_info_t *crash_info = (crash_info_t *)BACKUP_SRAM_BASE;
    bootloader_state_t state = STATE_INIT;
    struct timer dump_timer;
    struct timer retry_timer;
    struct timer arp_timer;
    int retry_count = 0;
    const int max_retries = 3;
    const uint32_t dump_timeout_ms = 30000;  /* 30 seconds per attempt */
    const uint32_t retry_delay_ms = 2000;    /* 2 seconds between retries */
    const uint32_t arp_delay_ms = 100;       /* 100ms ARP propagation */
    int network_initialized = 0;
    
    /* Initialize HAL */
    HAL_Init();
    
    /* Configure system clock */
    SystemClock_Config();
    
    /* Main polling loop */
    while (1) {
        switch (state) {
            case STATE_INIT:
                /* Check if we have a valid crash dump */
                if (crash_info->magic == CRASH_MAGIC_VALID) {
                    /* Initialize network */
                    if (network_init() == 0) {
                        network_initialized = 1;
                        state = STATE_ARP_ANNOUNCE;
                    } else {
                        /* Network init failed, jump to app */
                        state = STATE_DONE;
                    }
                } else {
                    /* No crash dump, jump to app */
                    state = STATE_DONE;
                }
                break;
                
            case STATE_ARP_ANNOUNCE:
                /* Send gratuitous ARP */
                network_send_gratuitous_arp();
                
                /* Start ARP wait timer */
                timer_set(&arp_timer, arp_delay_ms);
                state = STATE_ARP_WAIT;
                break;
                
            case STATE_ARP_WAIT:
                /* Process network while waiting */
                if (network_initialized) {
                    network_process();
                }
                
                /* Check if ARP wait time expired */
                if (timer_expired(&arp_timer)) {
                    state = STATE_TCP_START;
                }
                break;
                
            case STATE_TCP_START:
                /* Try to start TCP connection */
                if (tcp_crash_dump_start(crash_info) == 0) {
                    /* Started successfully, set timeout */
                    timer_set(&dump_timer, dump_timeout_ms);
                    state = STATE_TCP_SENDING;
                } else {
                    /* Failed to start */
                    retry_count++;
                    if (retry_count < max_retries) {
                        timer_set(&retry_timer, retry_delay_ms);
                        state = STATE_TCP_RETRY_WAIT;
                    } else {
                        state = STATE_DONE;
                    }
                }
                break;
                
            case STATE_TCP_SENDING:
                /* Process network and TCP */
                if (network_initialized) {
                    network_process();
                    tcp_crash_dump_poll();
                }
                
                /* Check completion */
                if (tcp_crash_dump_is_complete()) {
                    if (!tcp_crash_dump_has_error()) {
                        /* Success */
                        state = STATE_DONE;
                    } else {
                        /* Error occurred */
                        retry_count++;
                        if (retry_count < max_retries) {
                            timer_set(&retry_timer, retry_delay_ms);
                            state = STATE_TCP_RETRY_WAIT;
                        } else {
                            state = STATE_DONE;
                        }
                    }
                } else if (timer_expired(&dump_timer)) {
                    /* Timeout */
                    retry_count++;
                    if (retry_count < max_retries) {
                        timer_set(&retry_timer, retry_delay_ms);
                        state = STATE_TCP_RETRY_WAIT;
                    } else {
                        state = STATE_DONE;
                    }
                }
                break;
                
            case STATE_TCP_RETRY_WAIT:
                /* Process network while waiting */
                if (network_initialized) {
                    network_process();
                }
                
                /* Check if retry delay expired */
                if (timer_expired(&retry_timer)) {
                    /* Reinitialize for retry */
                    tcp_crash_dump_init();
                    state = STATE_TCP_START;
                }
                break;
                
            case STATE_DONE:
                /* Clear crash info */
                if (crash_info->magic == CRASH_MAGIC_VALID) {
                    crash_info->magic = 0;
                }
                
                /* Jump to application */
                jump_to_application();
                
                /* Should never reach here */
                while (1);
                break;
        }
    }
}