#include "uip.h"
#include "uip_arp.h"
#include "netdev.h"
#include "crash_info.h"
#include <string.h>
#include <stdio.h>

#define TCP_DUMP_PORT 9999
#define SERVER_IP_ADDR0 192
#define SERVER_IP_ADDR1 168
#define SERVER_IP_ADDR2 1
#define SERVER_IP_ADDR3 10

typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_SENDING_HEADER,
    STATE_SENDING_DATA,
    STATE_CLOSING,
    STATE_CLOSED
} tcp_state_t;

typedef struct {
    tcp_state_t state;
    uint8_t *dump_ptr;
    size_t dump_remaining;
    size_t total_size;
    crash_info_t crash_info;
} tcp_dump_state_t;

static tcp_dump_state_t dump_state;
static struct uip_conn *dump_connection = NULL;

/* RAM regions to dump */
typedef struct {
    uint32_t start;
    uint32_t size;
    const char *name;
} ram_region_t;

static const ram_region_t ram_regions[] = {
    {0x20000000, 0x20000, "DTCM"},
    {0x24000000, 0x80000, "AXI_SRAM"},
    {0x30000000, 0x20000, "SRAM1"},
    {0x30020000, 0x20000, "SRAM2"},
    {0x30040000, 0x08000, "SRAM3"},
    {0x38000000, 0x10000, "SRAM4"},
    {0x38800000, 0x01000, "BACKUP_SRAM"}
};

void tcp_crash_dump_init(void) {
    dump_state.state = STATE_IDLE;
    dump_state.dump_ptr = NULL;
    dump_state.dump_remaining = 0;
    dump_state.total_size = 0;
}

int tcp_crash_dump_start(crash_info_t *crash_info) {
    if (dump_state.state != STATE_IDLE) {
        return -1;
    }
    
    /* Copy crash info */
    memcpy(&dump_state.crash_info, crash_info, sizeof(crash_info_t));
    
    /* Calculate total dump size */
    dump_state.total_size = sizeof(crash_info_t);
    for (size_t i = 0; i < sizeof(ram_regions)/sizeof(ram_regions[0]); i++) {
        dump_state.total_size += ram_regions[i].size;
    }
    
    /* Set up for sending */
    dump_state.dump_ptr = (uint8_t *)&dump_state.crash_info;
    dump_state.dump_remaining = sizeof(crash_info_t);
    dump_state.state = STATE_CONNECTING;
    
    /* Connect to server */
    uip_ipaddr_t server_ip;
    uip_ipaddr(&server_ip, SERVER_IP_ADDR0, SERVER_IP_ADDR1, SERVER_IP_ADDR2, SERVER_IP_ADDR3);
    dump_connection = uip_connect(&server_ip, HTONS(TCP_DUMP_PORT));
    
    if (dump_connection == NULL) {
        dump_state.state = STATE_IDLE;
        return -1;
    }
    
    return 0;
}

void tcp_crash_dump_appcall(void) {
    if (dump_connection == NULL || uip_conn != dump_connection) {
        return;
    }
    
    if (uip_connected()) {
        dump_state.state = STATE_CONNECTED;
    }
    
    if (uip_closed() || uip_aborted() || uip_timedout()) {
        dump_state.state = STATE_CLOSED;
        dump_connection = NULL;
        return;
    }
    
    if (uip_acked()) {
        /* Data was acknowledged, update pointers */
        size_t acked = uip_conn->len;
        if (acked > dump_state.dump_remaining) {
            acked = dump_state.dump_remaining;
        }
        
        dump_state.dump_ptr += acked;
        dump_state.dump_remaining -= acked;
        
        /* Check if we finished current stage */
        if (dump_state.dump_remaining == 0) {
            if (dump_state.state == STATE_SENDING_HEADER) {
                /* Start sending first RAM region */
                dump_state.dump_ptr = (uint8_t *)ram_regions[0].start;
                dump_state.dump_remaining = ram_regions[0].size;
                dump_state.state = STATE_SENDING_DATA;
            } else if (dump_state.state == STATE_SENDING_DATA) {
                /* Find next region to send */
                int current_region = -1;
                for (size_t i = 0; i < sizeof(ram_regions)/sizeof(ram_regions[0]); i++) {
                    if ((uint32_t)dump_state.dump_ptr == ram_regions[i].start + ram_regions[i].size) {
                        current_region = i;
                        break;
                    }
                }
                
                if (current_region >= 0 && (size_t)current_region < sizeof(ram_regions)/sizeof(ram_regions[0]) - 1) {
                    /* Move to next region */
                    current_region++;
                    dump_state.dump_ptr = (uint8_t *)ram_regions[current_region].start;
                    dump_state.dump_remaining = ram_regions[current_region].size;
                } else {
                    /* All done, close connection */
                    dump_state.state = STATE_CLOSING;
                    uip_close();
                    return;
                }
            }
        }
    }
    
    if ((uip_newdata() || uip_acked() || uip_connected()) && 
        (dump_state.state == STATE_CONNECTED || dump_state.state == STATE_SENDING_HEADER || dump_state.state == STATE_SENDING_DATA)) {
        /* Send data */
        if (dump_state.state == STATE_CONNECTED) {
            dump_state.state = STATE_SENDING_HEADER;
        }
        
        size_t to_send = dump_state.dump_remaining;
        if (to_send > uip_mss()) {
            to_send = uip_mss();
        }
        
        if (to_send > 0) {
            memcpy(uip_appdata, dump_state.dump_ptr, to_send);
            uip_send(uip_appdata, to_send);
        }
    }
}

int tcp_crash_dump_is_complete(void) {
    return (dump_state.state == STATE_CLOSED || dump_state.state == STATE_IDLE);
}