/**
 * @file main.c
 * @brief Main entry point for CrashCatcher with TCP dump
 */

#include "stm32h7xx_hal.h"
#include <string.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "CrashCatcher.h"
#include "core_cm7.h"
#include "crash_info.h"
#include "bootloader_config.h"

/* Network interface */
struct netif gnetif;

/* Bootloader configuration */
static bootloader_config_t g_config;

/* Using SysTick instead of TIM6 */

/**
 * @brief Initialize GPIO for LEDs
 */
static void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIOG clock */
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* Configure LED pins as outputs */
    GPIO_InitStruct.Pin = g_config.led_life.pin | g_config.led_fault.pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    HAL_GPIO_Init(g_config.led_life.port, &GPIO_InitStruct);
    
    /* Turn off LEDs initially */
    HAL_GPIO_WritePin(g_config.led_life.port, g_config.led_life.pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(g_config.led_fault.port, g_config.led_fault.pin, GPIO_PIN_RESET);
}

/**
 * @brief Toggle both LEDs
 */
static void LED_Toggle_Both(void) {
    HAL_GPIO_TogglePin(g_config.led_life.port, g_config.led_life.pin);
    HAL_GPIO_TogglePin(g_config.led_fault.port, g_config.led_fault.pin);
}

/**
 * @brief Turn on both LEDs
 */
static void LED_On_Both(void) {
    HAL_GPIO_WritePin(g_config.led_life.port, g_config.led_life.pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(g_config.led_fault.port, g_config.led_fault.pin, GPIO_PIN_SET);
}

/**
 * @brief Turn off both LEDs
 */
static void LED_Off_Both(void) {
    HAL_GPIO_WritePin(g_config.led_life.port, g_config.led_life.pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(g_config.led_fault.port, g_config.led_fault.pin, GPIO_PIN_RESET);
}

/**
 * @brief System Clock Configuration
 */
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    
    /* Configure the main internal regulator output voltage */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    
    /* Macro to configure the PLL clock source */
    __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
    
    /* Initialize HSE Oscillator - match app config */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 12;
    RCC_OscInitStruct.PLL.PLLN = 480;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 12;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    /* Select PLL as system clock source */
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
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief Network initialization
 */
static void Network_Init(void) {
    ip4_addr_t ipaddr, netmask, gw;
    
    /* Initialize lwIP */
    lwip_init();
    
    /* Set static IP from configuration */
    IP4_ADDR(&ipaddr, 
             g_config.network.device_ip[0],
             g_config.network.device_ip[1],
             g_config.network.device_ip[2],
             g_config.network.device_ip[3]);
    
    IP4_ADDR(&netmask,
             g_config.network.device_netmask[0],
             g_config.network.device_netmask[1],
             g_config.network.device_netmask[2],
             g_config.network.device_netmask[3]);
    
    IP4_ADDR(&gw,
             g_config.network.device_gateway[0],
             g_config.network.device_gateway[1],
             g_config.network.device_gateway[2],
             g_config.network.device_gateway[3]);
    
    /* Set MAC address */
    gnetif.hwaddr_len = 6;
    memcpy(gnetif.hwaddr, g_config.network.mac_addr, 6);
    
    /* Add network interface */
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
    
    /* Register default network interface */
    netif_set_default(&gnetif);
    
    /* Set interface up */
    netif_set_up(&gnetif);
    netif_set_link_up(&gnetif);
}

/**
 * @brief Jump to application
 * @param app_address Application start address (0x08020000)
 */
/* External assembly function for clean jump */
extern void jump_to_application_asm(uint32_t app_address);

static void Jump_To_Application(uint32_t app_address) {
    uint32_t i;
    
    /* Disable all interrupts */
    __disable_irq();
    
    /* Suspend HAL tick first */
    HAL_SuspendTick();
    
    /* De-initialize peripherals - ORDER IS IMPORTANT */
    HAL_DeInit();
    HAL_RCC_DeInit();
    
    /* Disable Systick AFTER HAL_RCC_DeInit to avoid lockups */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* Disable MPU */
    HAL_MPU_Disable();
    
    /* Clean and Disable caches for Cortex-M7 */
    SCB_DisableDCache();
    SCB_DisableICache();
    
    /* Clear all NVIC interrupts - proper way */
    for (i = 0; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  /* Disable all interrupts */
        NVIC->ICPR[i] = 0xFFFFFFFF;  /* Clear all pending interrupts */
    }
    
    /* Clear any pending system exceptions */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    
    /* Clear exception priority registers */
    for (i = 0; i < (sizeof(NVIC->IP) / sizeof(NVIC->IP[0])); i++) {
        NVIC->IP[i] = 0x00;
    }
    
    /* Clear PRIMASK, FAULTMASK and BASEPRI */
    __set_PRIMASK(0);
    __set_FAULTMASK(0);
    __set_BASEPRI(0);
    
    /* Reset CONTROL register */
    __set_CONTROL(0);
    
    /* Re-enable interrupts */
    __enable_irq();
    
    /* Set Main Stack Pointer from application vector table */
    __set_MSP(*((uint32_t*)app_address));
    
    /* Set vector table to application */
    SCB->VTOR = app_address;
    
    /* Memory barriers */
    __DSB();
    __ISB();
    
    /* Jump to application reset handler */
    jump_to_application_asm(app_address);
}

/**
 * @brief Check if crash dump needs to be sent
 * @return 1 if crash detected, 0 otherwise
 */
static int Check_For_Crash(void) {
    /* Enable Backup SRAM clock */
    __HAL_RCC_BKPRAM_CLK_ENABLE();
    
    /* Enable backup domain access */
    HAL_PWR_EnableBkUpAccess();
    
    /* Check crash marker */
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    return (crash_info->magic == CRASH_MAGIC_VALID);
}

/**
 * @brief Clear crash marker
 */
static void Clear_Crash_Marker(void) {
    volatile crash_info_t* crash_info = (volatile crash_info_t*)BACKUP_SRAM_BASE;
    crash_info->magic = CRASH_MAGIC_CLEAR;
}

/**
 * @brief Main function
 */
int main(void) {
    /* Load configuration */
    g_config = DEFAULT_CONFIG;
    
    /* Disable I-Cache and D-Cache */
    /* NOTE: Commenting out temporarily to debug hang issue */
    SCB_EnableICache ();

    /* Enable D-Cache */
    SCB_EnableDCache ();
    
    /* Reset peripherals, Initialize Flash and Systick */
    HAL_Init();

    /* Configure system clock */
    SystemClock_Config();

    /* Initialize LEDs */
    LED_Init();
    
    /* Check if there was a crash */
    if (Check_For_Crash()) {
        /* Turn on both LEDs to indicate crash detected */
        LED_On_Both();
        HAL_Delay(1000);  /* Keep on for 1 second */
        
        /* Initialize network */
        Network_Init();
        
        /* Initialize TCP crash dump client with server config */
        extern void tcp_crash_dump_init_with_config(
            const uint8_t server_ip[4], 
            uint16_t server_port,
            uint32_t timeout_ms
        );
        
        tcp_crash_dump_init_with_config(
            g_config.network.server_ip,
            g_config.network.server_port,
            g_config.connection_timeout_ms
        );
        
        /* Wait for dump to complete (with timeout) */
        uint32_t timeout_start = HAL_GetTick();
        uint32_t last_blink_time = HAL_GetTick();
        
        while ((HAL_GetTick() - timeout_start) < g_config.dump_timeout_ms) {
            /* Blink LEDs during dump transmission */
            if ((HAL_GetTick() - last_blink_time) >= g_config.led_blink_period_ms) {
                LED_Toggle_Both();
                last_blink_time = HAL_GetTick();
            }
            
            /* Handle timeouts */
            sys_check_timeouts();
            
            /* Check for received frames */
            extern void ethernetif_input(struct netif *netif);
            ethernetif_input(&gnetif);
            
            /* Check if dump is complete */
            extern int tcp_crash_dump_is_complete(void);
            if (tcp_crash_dump_is_complete()) {
                /* Clear crash marker */
                Clear_Crash_Marker();
                break;
            }
            
            /* Small delay */
            HAL_Delay(1);
        }
        
        /* Turn off LEDs when done */
        LED_Off_Both();
        
        /* Clear marker even if timeout */
        Clear_Crash_Marker();
    }
    
    /* Blink LEDs briefly before jumping to indicate bootloader is working */
    for (int i = 0; i < 3; i++) {
        LED_On_Both();
        HAL_Delay(100);
        LED_Off_Both();
        HAL_Delay(100);
    }
    
    /* Jump to application */
    Jump_To_Application(g_config.app_address);
    
    /* Should never reach here */
    while (1) {
        /* If we're here, jump failed - blink fast to indicate error */
        LED_Toggle_Both();
        HAL_Delay(100);
    }
}

/* HAL uses default SysTick_Handler for time base */

/**
 * @brief Error handler
 */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}