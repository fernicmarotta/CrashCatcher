/**
 * @file tcp_crash_dump.c
 * @brief TCP-based crash dump transmission for CrashCatcher
 * 
 * Minimal implementation optimized for low memory footprint
 */

#include "tcp_crash_dump.h"
#include "CrashCatcher.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "ethif_config.h"
#include <string.h>

/* TCP connection state */
typedef enum {
    TCP_STATE_IDLE,
    TCP_STATE_CONNECTING,
    TCP_STATE_CONNECTED,
    TCP_STATE_SENDING,
    TCP_STATE_ERROR
} tcp_state_t;

/* Crash dump context */
static struct {
    struct tcp_pcb *pcb;
    tcp_state_t state;
    const uint8_t *data_ptr;
    uint32_t data_remaining;
    uint32_t retry_count;
    uint8_t buffer[TCP_MSS];  /* Reuse single buffer */
    uint16_t buffer_used;
} g_tcp_dump;

/* Network interface */
static struct netif g_netif;

/* Function prototypes */
static err_t tcp_dump_connected(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t tcp_dump_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
static void tcp_dump_error(void *arg, err_t err);
static err_t tcp_dump_poll(void *arg, struct tcp_pcb *pcb);
static void tcp_send_buffered_data(void);

/**
 * Initialize TCP crash dump module
 */
void tcp_crash_dump_init(void) {
    ip4_addr_t ipaddr, netmask, gateway, server_ip;
    
    /* Initialize lwIP */
    lwip_init();
    
    /* Set up IP addresses */
    IP4_ADDR(&ipaddr, CRASH_DUMP_IP_ADDR0, CRASH_DUMP_IP_ADDR1, 
             CRASH_DUMP_IP_ADDR2, CRASH_DUMP_IP_ADDR3);
    IP4_ADDR(&netmask, CRASH_DUMP_NETMASK0, CRASH_DUMP_NETMASK1,
             CRASH_DUMP_NETMASK2, CRASH_DUMP_NETMASK3);
    IP4_ADDR(&gateway, CRASH_DUMP_GATEWAY0, CRASH_DUMP_GATEWAY1,
             CRASH_DUMP_GATEWAY2, CRASH_DUMP_GATEWAY3);
    
    /* Initialize network interface */
    netif_add(&g_netif, &ipaddr, &netmask, &gateway, NULL, 
              ethernetif_init, ethernet_input);
    netif_set_default(&g_netif);
    
    /* Initialize dump context */
    memset(&g_tcp_dump, 0, sizeof(g_tcp_dump));
    g_tcp_dump.state = TCP_STATE_IDLE;
}

/**
 * CrashCatcher callback - Start dump transmission
 */
void CrashCatcher_DumpStart(const CrashCatcherInfo* pInfo) {
    ip4_addr_t server_ip;
    err_t err;
    
    (void)pInfo;  /* Unused */
    
    /* Bring up network interface if needed */
    if (!netif_is_up(&g_netif)) {
        netif_set_up(&g_netif);
        
        /* Wait for link */
        uint32_t timeout = ETH_LINK_TIMEOUT;
        while (!netif_is_link_up(&g_netif) && timeout > 0) {
            sys_check_timeouts();
            timeout--;
            /* Add delay function here if available */
        }
        
        if (!netif_is_link_up(&g_netif)) {
            return;  /* No network, give up */
        }
    }
    
    /* Create TCP PCB */
    g_tcp_dump.pcb = tcp_new();
    if (g_tcp_dump.pcb == NULL) {
        return;
    }
    
    /* Set up callbacks */
    tcp_arg(g_tcp_dump.pcb, NULL);
    tcp_err(g_tcp_dump.pcb, tcp_dump_error);
    tcp_poll(g_tcp_dump.pcb, tcp_dump_poll, 10);  /* Poll every 5 seconds */
    tcp_sent(g_tcp_dump.pcb, tcp_dump_sent);
    
    /* Connect to server using configured address */
    uint8_t ip0, ip1, ip2, ip3;
    uint16_t port;
    
    /* Get configured server address */
    extern void tcp_crash_dump_get_server_ip(uint8_t *ip0, uint8_t *ip1, uint8_t *ip2, uint8_t *ip3);
    extern uint16_t tcp_crash_dump_get_server_port(void);
    
    tcp_crash_dump_get_server_ip(&ip0, &ip1, &ip2, &ip3);
    port = tcp_crash_dump_get_server_port();
    
    IP4_ADDR(&server_ip, ip0, ip1, ip2, ip3);
    
    err = tcp_connect(g_tcp_dump.pcb, &server_ip, port, tcp_dump_connected);
    
    if (err != ERR_OK) {
        tcp_close(g_tcp_dump.pcb);
        g_tcp_dump.pcb = NULL;
        return;
    }
    
    g_tcp_dump.state = TCP_STATE_CONNECTING;
    g_tcp_dump.buffer_used = 0;
    g_tcp_dump.retry_count = 0;
    
    /* Wait for connection */
    while (g_tcp_dump.state == TCP_STATE_CONNECTING) {
        sys_check_timeouts();
        ethernetif_input(&g_netif);
    }
    
    if (g_tcp_dump.state != TCP_STATE_CONNECTED) {
        return;
    }
    
    /* Send dump header */
    const char* header = "CRASH_DUMP_START\n";
    tcp_write(g_tcp_dump.pcb, header, strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_output(g_tcp_dump.pcb);
}

/**
 * CrashCatcher callback - Send memory region
 */
void CrashCatcher_DumpMemory(const void* pvMemory, CrashCatcherElementSizes elementSize, 
                             size_t elementCount) {
    const uint8_t* pData = (const uint8_t*)pvMemory;
    size_t totalBytes = elementCount * elementSize;
    
    /* Buffer data and send when buffer is full */
    while (totalBytes > 0) {
        size_t space = sizeof(g_tcp_dump.buffer) - g_tcp_dump.buffer_used;
        size_t toCopy = (totalBytes < space) ? totalBytes : space;
        
        memcpy(&g_tcp_dump.buffer[g_tcp_dump.buffer_used], pData, toCopy);
        g_tcp_dump.buffer_used += toCopy;
        pData += toCopy;
        totalBytes -= toCopy;
        
        /* Send if buffer is full */
        if (g_tcp_dump.buffer_used >= sizeof(g_tcp_dump.buffer)) {
            tcp_send_buffered_data();
        }
    }
}

/**
 * CrashCatcher callback - End dump transmission
 */
CrashCatcherReturnCodes CrashCatcher_DumpEnd(void) {
    const char* footer = "\nCRASH_DUMP_END\n";
    
    /* Send any remaining buffered data */
    if (g_tcp_dump.buffer_used > 0) {
        tcp_send_buffered_data();
    }
    
    /* Send footer */
    if (g_tcp_dump.pcb && g_tcp_dump.state == TCP_STATE_CONNECTED) {
        tcp_write(g_tcp_dump.pcb, footer, strlen(footer), TCP_WRITE_FLAG_COPY);
        tcp_output(g_tcp_dump.pcb);
        
        /* Wait for data to be sent */
        while (tcp_sndbuf(g_tcp_dump.pcb) < TCP_SND_BUF) {
            sys_check_timeouts();
            ethernetif_input(&g_netif);
        }
        
        /* Close connection */
        tcp_close(g_tcp_dump.pcb);
        g_tcp_dump.pcb = NULL;
    }
    
    g_tcp_dump.state = TCP_STATE_IDLE;
    
    /* Mark dump as complete */
    extern void tcp_crash_dump_set_complete(void);
    tcp_crash_dump_set_complete();
    
    return CRASH_CATCHER_EXIT;  /* Done with dump */
}

/**
 * TCP connected callback
 */
static err_t tcp_dump_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    (void)pcb;
    
    if (err == ERR_OK) {
        g_tcp_dump.state = TCP_STATE_CONNECTED;
    } else {
        g_tcp_dump.state = TCP_STATE_ERROR;
    }
    
    return ERR_OK;
}

/**
 * TCP sent callback
 */
static err_t tcp_dump_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)arg;
    (void)pcb;
    (void)len;
    
    /* Data acknowledged, can send more */
    g_tcp_dump.state = TCP_STATE_CONNECTED;
    
    return ERR_OK;
}

/**
 * TCP error callback
 */
static void tcp_dump_error(void *arg, err_t err) {
    (void)arg;
    (void)err;
    
    g_tcp_dump.state = TCP_STATE_ERROR;
    g_tcp_dump.pcb = NULL;  /* PCB is already freed by lwIP */
}

/**
 * TCP poll callback
 */
static err_t tcp_dump_poll(void *arg, struct tcp_pcb *pcb) {
    (void)arg;
    
    /* Keep connection alive */
    if (g_tcp_dump.state == TCP_STATE_CONNECTED && g_tcp_dump.retry_count > 0) {
        g_tcp_dump.retry_count--;
        tcp_output(pcb);
    }
    
    return ERR_OK;
}

/**
 * Send buffered data over TCP
 */
static void tcp_send_buffered_data(void) {
    err_t err;
    
    if (g_tcp_dump.pcb == NULL || g_tcp_dump.state != TCP_STATE_CONNECTED) {
        return;
    }
    
    /* Try to send data */
    while (g_tcp_dump.buffer_used > 0) {
        u16_t send_len = (g_tcp_dump.buffer_used < tcp_sndbuf(g_tcp_dump.pcb)) ?
                         g_tcp_dump.buffer_used : tcp_sndbuf(g_tcp_dump.pcb);
        
        if (send_len == 0) {
            /* No space in send buffer, wait */
            sys_check_timeouts();
            ethernetif_input(&g_netif);
            continue;
        }
        
        err = tcp_write(g_tcp_dump.pcb, g_tcp_dump.buffer, send_len, TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK) {
            /* Remove sent data from buffer */
            if (send_len < g_tcp_dump.buffer_used) {
                memmove(g_tcp_dump.buffer, &g_tcp_dump.buffer[send_len], 
                        g_tcp_dump.buffer_used - send_len);
            }
            g_tcp_dump.buffer_used -= send_len;
            
            /* Force output */
            tcp_output(g_tcp_dump.pcb);
        } else {
            /* Error sending, retry later */
            g_tcp_dump.retry_count = 3;
            break;
        }
        
        /* Process incoming packets */
        sys_check_timeouts();
        ethernetif_input(&g_netif);
    }
}

/**
 * CrashCatcher memory region callback
 */
const CrashCatcherMemoryRegion* CrashCatcher_GetMemoryRegions(void) {
    static const CrashCatcherMemoryRegion regions[] = {
        /* STM32H743 RAM regions - All except ITCM (0x00000000 - 0x00010000) */
        {0x20000000, 0x20020000, CRASH_CATCHER_BYTE},  /* DTCM RAM: 128KB */
        {0x24000000, 0x24080000, CRASH_CATCHER_BYTE},  /* AXI SRAM: 512KB (D1 domain) */
        {0x30000000, 0x30020000, CRASH_CATCHER_BYTE},  /* SRAM1: 128KB (D2 domain) */
        {0x30020000, 0x30040000, CRASH_CATCHER_BYTE},  /* SRAM2: 128KB (D2 domain) */
        {0x30040000, 0x30048000, CRASH_CATCHER_BYTE},  /* SRAM3: 32KB (D2 domain) */
        {0x38000000, 0x38010000, CRASH_CATCHER_BYTE},  /* SRAM4: 64KB (D3 domain) */
        {0x38800000, 0x38801000, CRASH_CATCHER_BYTE},  /* Backup SRAM: 4KB */
        {0xFFFFFFFF, 0xFFFFFFFF, CRASH_CATCHER_BYTE}   /* End marker */
    };
    
    return regions;
}