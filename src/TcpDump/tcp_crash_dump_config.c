/**
 * @file tcp_crash_dump_config.c
 * @brief Configurable TCP crash dump client
 */

#include "tcp_crash_dump.h"
#include "lwip/tcp.h"
#include <string.h>

/* Server configuration */
static struct {
    uint8_t ip[4];
    uint16_t port;
    uint32_t timeout_ms;
} g_server_config = {
    .ip = {192, 168, 1, 10},  /* Default */
    .port = 9999,
    .timeout_ms = 5000
};

/* Dump completion flag */
static volatile int g_dump_complete = 0;

/**
 * @brief Initialize TCP crash dump with custom server configuration
 */
void tcp_crash_dump_init_with_config(
    const uint8_t server_ip[4], 
    uint16_t server_port,
    uint32_t timeout_ms
) {
    /* Store configuration */
    memcpy(g_server_config.ip, server_ip, 4);
    g_server_config.port = server_port;
    g_server_config.timeout_ms = timeout_ms;
    
    /* Reset completion flag */
    g_dump_complete = 0;
    
    /* Initialize the original module */
    tcp_crash_dump_init();
}

/**
 * @brief Get configured server IP
 */
void tcp_crash_dump_get_server_ip(uint8_t *ip0, uint8_t *ip1, uint8_t *ip2, uint8_t *ip3) {
    *ip0 = g_server_config.ip[0];
    *ip1 = g_server_config.ip[1];
    *ip2 = g_server_config.ip[2];
    *ip3 = g_server_config.ip[3];
}

/**
 * @brief Get configured server port
 */
uint16_t tcp_crash_dump_get_server_port(void) {
    return g_server_config.port;
}

/**
 * @brief Check if dump is complete
 */
int tcp_crash_dump_is_complete(void) {
    return g_dump_complete;
}

/**
 * @brief Mark dump as complete (called from tcp_crash_dump.c)
 */
void tcp_crash_dump_set_complete(void) {
    g_dump_complete = 1;
}