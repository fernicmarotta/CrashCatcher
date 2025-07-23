/**
 * @file bootloader_config.h
 * @brief Bootloader configuration structure
 */

#ifndef BOOTLOADER_CONFIG_H
#define BOOTLOADER_CONFIG_H

#include <stdint.h>

/**
 * @brief Network configuration
 */
typedef struct {
    /* Device IP configuration */
    uint8_t device_ip[4];
    uint8_t device_netmask[4];
    uint8_t device_gateway[4];
    
    /* Crash dump server */
    uint8_t server_ip[4];
    uint16_t server_port;
    
    /* MAC address */
    uint8_t mac_addr[6];
} network_config_t;

/**
 * @brief LED configuration
 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} led_config_t;

/**
 * @brief Bootloader configuration
 */
typedef struct {
    /* Application address */
    uint32_t app_address;
    
    /* Timeouts in milliseconds */
    uint32_t dump_timeout_ms;
    uint32_t connection_timeout_ms;
    
    /* Network configuration */
    network_config_t network;
    
    /* Retry configuration */
    uint8_t max_retries;
    uint8_t retry_delay_ms;
    
    /* LED configuration */
    led_config_t led_life;
    led_config_t led_fault;
    uint32_t led_blink_period_ms;
    
} bootloader_config_t;

/* LED pin definitions */
#define LED_LIFE_uP_Pin         GPIO_PIN_12
#define LED_LIFE_uP_GPIO_Port   GPIOG
#define LED_FAULT_uP_Pin        GPIO_PIN_13
#define LED_FAULT_uP_GPIO_Port  GPIOG

/**
 * @brief Default bootloader configuration
 */
static const bootloader_config_t DEFAULT_CONFIG = {
    .app_address = 0x08020000,
    .dump_timeout_ms = 30000,    /* 30 seconds */
    .connection_timeout_ms = 5000, /* 5 seconds */
    
    .network = {
        .device_ip = {192, 168, 1, 200},
        .device_netmask = {255, 255, 255, 0},
        .device_gateway = {192, 168, 1, 1},
        
        .server_ip = {192, 168, 1, 10},
        .server_port = 9999,
        
        .mac_addr = {0x00, 0x80, 0xE1, 0x00, 0x00, 0x01}
    },
    
    .max_retries = 3,
    .retry_delay_ms = 100,
    
    .led_life = {
        .port = LED_LIFE_uP_GPIO_Port,
        .pin = LED_LIFE_uP_Pin
    },
    .led_fault = {
        .port = LED_FAULT_uP_GPIO_Port,
        .pin = LED_FAULT_uP_Pin
    },
    .led_blink_period_ms = 500  /* Blink every 500ms */
};

#endif /* BOOTLOADER_CONFIG_H */