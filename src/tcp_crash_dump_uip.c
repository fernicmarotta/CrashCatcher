#include "uip.h"
#include "uip_arp.h"
#include "netdev.h"
#include "crash_info.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdio.h>

#define TCP_DUMP_PORT 9999
#define SERVER_IP_ADDR0 192
#define SERVER_IP_ADDR1 168
#define SERVER_IP_ADDR2 204
#define SERVER_IP_ADDR3 134

/* Timeout definitions */
#define CONNECTION_TIMEOUT_MS   5000   /* 5 seconds to establish connection */
#define SEND_TIMEOUT_MS        10000   /* 10 seconds for each send operation */
#define BYTES_PER_LINE            16   /* Hex dump: 16 bytes per line */

typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_SENDING_HEADER,
    STATE_SENDING_REGISTERS,
    STATE_SENDING_NVIC_INFO,
    STATE_SENDING_MEMORY,
    STATE_SENDING_FOOTER,
    STATE_CLOSING,
    STATE_CLOSED,
    STATE_ERROR
} tcp_state_t;

typedef struct {
    tcp_state_t state;
    uint8_t *current_ptr;         /* Current memory pointer */
    size_t bytes_remaining;       /* Bytes remaining in current region */
    crash_info_t *crash_info;      /* Pointer to crash info in backup SRAM */
    uint32_t connect_start_time;  /* Time when connection started */
    uint32_t last_activity_time;  /* Last time we had network activity */
    char send_buffer[256];        /* Buffer for formatted output */
    size_t send_buffer_len;       /* Current length of data in send buffer */
    size_t send_buffer_pos;       /* Current send position */
    int current_region;           /* Current RAM region index */
    size_t line_byte_count;       /* Bytes in current hex line */
    uint32_t current_address;     /* Current dump address */
} tcp_dump_state_t;

static tcp_dump_state_t dump_state;
static struct uip_conn *dump_connection = NULL;

/* RAM regions to dump - matching CrashCatcher regions */
typedef struct {
    uint32_t start;
    uint32_t end;
    const char *name;
} ram_region_t;

static const ram_region_t ram_regions[] = {
    {0x20000000, 0x20020000, "DTCM"},
    {0x24000000, 0x24080000, "AXI_SRAM"},
    {0x30000000, 0x30020000, "SRAM1"},
    {0x30020000, 0x30040000, "SRAM2"},
    {0x30040000, 0x30048000, "SRAM3"},
    {0x38000000, 0x38010000, "SRAM4"},
    {0x38800000, 0x38801000, "BACKUP_SRAM"}
};

/* Register names for dump */
static const char* register_names[] = {
    "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
    "PSR", "MSP", "PSP", "CONTROL", "BASEPRI", "PRIMASK", 
    "FAULTMASK", "FPSCR", "HFSR", "CFSR", "MMFAR", "BFAR", "AFSR", "NVIC_ISER0",
    "LR_AT_FAULT"
};

static void format_hex_byte(char *out, uint8_t byte);
static void format_hex_word(char *out, uint32_t word);
static size_t prepare_string(const char *str);
static size_t prepare_hex_line(void);
static void check_timeouts(void);

void tcp_crash_dump_init(void) {
    memset(&dump_state, 0, sizeof(dump_state));
    dump_state.state = STATE_IDLE;
}

int tcp_crash_dump_start(crash_info_t *crash_info) {
    if (dump_state.state != STATE_IDLE) {
        return -1;
    }
    
    /* Save pointer to crash info (no copy needed) */
    dump_state.crash_info = crash_info;
    
    /* Initialize state */
    dump_state.current_region = -1;
    dump_state.send_buffer_len = 0;
    dump_state.send_buffer_pos = 0;
    dump_state.connect_start_time = HAL_GetTick();
    dump_state.last_activity_time = HAL_GetTick();
    
    /* Connect to server */
    uip_ipaddr_t server_ip;
    uip_ipaddr(&server_ip, SERVER_IP_ADDR0, SERVER_IP_ADDR1, SERVER_IP_ADDR2, SERVER_IP_ADDR3);
    dump_connection = uip_connect(&server_ip, HTONS(TCP_DUMP_PORT));
    
    if (dump_connection == NULL) {
        dump_state.state = STATE_IDLE;
        return -1;
    }
    
    dump_state.state = STATE_CONNECTING;
    return 0;
}

static void format_hex_byte(char *out, uint8_t byte) {
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[byte >> 4];
    out[1] = hex[byte & 0x0F];
}

static void format_hex_word(char *out, uint32_t word) {
    format_hex_byte(&out[0], (word >> 24) & 0xFF);
    format_hex_byte(&out[2], (word >> 16) & 0xFF);
    format_hex_byte(&out[4], (word >> 8) & 0xFF);
    format_hex_byte(&out[6], word & 0xFF);
}

static size_t prepare_string(const char *str) {
    size_t len = strlen(str);
    if (len > sizeof(dump_state.send_buffer) - 2) {
        len = sizeof(dump_state.send_buffer) - 2;
    }
    memcpy(dump_state.send_buffer, str, len);
    dump_state.send_buffer[len] = '\r';
    dump_state.send_buffer[len + 1] = '\n';
    dump_state.send_buffer_len = len + 2;
    dump_state.send_buffer_pos = 0;
    return dump_state.send_buffer_len;
}

static size_t prepare_hex_line(void) {
    size_t pos = 0;
    
    if (dump_state.bytes_remaining == 0) {
        return 0;
    }
    
    /* Format address if starting new line */
    if (dump_state.line_byte_count == 0) {
        format_hex_word(&dump_state.send_buffer[pos], dump_state.current_address);
        pos += 8;
        dump_state.send_buffer[pos++] = ':';
        dump_state.send_buffer[pos++] = ' ';
    }
    
    /* Format up to 16 bytes per line */
    while (dump_state.line_byte_count < BYTES_PER_LINE && dump_state.bytes_remaining > 0) {
        uint8_t byte = *dump_state.current_ptr++;
        format_hex_byte(&dump_state.send_buffer[pos], byte);
        pos += 2;
        dump_state.send_buffer[pos++] = ' ';
        
        dump_state.line_byte_count++;
        dump_state.bytes_remaining--;
        dump_state.current_address++;
    }
    
    /* End of line */
    if (dump_state.line_byte_count >= BYTES_PER_LINE || dump_state.bytes_remaining == 0) {
        dump_state.send_buffer[pos - 1] = '\r';  /* Replace last space */
        dump_state.send_buffer[pos++] = '\n';
        dump_state.line_byte_count = 0;
    }
    
    dump_state.send_buffer_len = pos;
    dump_state.send_buffer_pos = 0;
    return pos;
}

static void check_timeouts(void) {
    uint32_t now = HAL_GetTick();
    
    if (dump_state.state == STATE_CONNECTING) {
        if ((now - dump_state.connect_start_time) > CONNECTION_TIMEOUT_MS) {
            dump_state.state = STATE_ERROR;
            if (dump_connection) {
                uip_abort();
                dump_connection = NULL;
            }
        }
    } else if (dump_state.state >= STATE_SENDING_HEADER && dump_state.state <= STATE_SENDING_MEMORY) {
        if ((now - dump_state.last_activity_time) > SEND_TIMEOUT_MS) {
            dump_state.state = STATE_ERROR;
            if (dump_connection) {
                uip_abort();
                dump_connection = NULL;
            }
        }
    }
}

void tcp_crash_dump_appcall(void) {
    if (dump_connection == NULL || uip_conn != dump_connection) {
        return;
    }
    
    /* Update activity time on any network event */
    dump_state.last_activity_time = HAL_GetTick();
    
    /* Handle connection events */
    if (uip_connected()) {
        dump_state.state = STATE_CONNECTED;
        dump_state.send_buffer_len = 0;
        dump_state.send_buffer_pos = 0;
    }
    
    if (uip_closed() || uip_aborted() || uip_timedout()) {
        dump_state.state = (uip_closed() ? STATE_CLOSED : STATE_ERROR);
        dump_connection = NULL;
        return;
    }
    
    /* Process based on current state */
    switch (dump_state.state) {
        case STATE_CONNECTED:
            /* Prepare header */
            prepare_string("\r\n\r\nCRASH ENCOUNTERED\r\n");
            dump_state.state = STATE_SENDING_HEADER;
            break;
            
        case STATE_SENDING_HEADER:
            if (dump_state.send_buffer_pos >= dump_state.send_buffer_len) {
                /* Header sent, prepare registers */
                dump_state.state = STATE_SENDING_REGISTERS;
                dump_state.current_region = 0;  /* Use as register index */
            }
            break;
            
        case STATE_SENDING_REGISTERS:
            if (dump_state.send_buffer_pos >= dump_state.send_buffer_len) {
                /* Prepare next register */
                if (dump_state.current_region < 31) {  /* 31 registers total */
                    char line[64];
                    uint32_t value = 0;
                    
                    /* Get register value */
                    switch (dump_state.current_region) {
                        case 0: value = dump_state.crash_info->r0; break;
                        case 1: value = dump_state.crash_info->r1; break;
                        case 2: value = dump_state.crash_info->r2; break;
                        case 3: value = dump_state.crash_info->r3; break;
                        case 4: value = dump_state.crash_info->r4; break;
                        case 5: value = dump_state.crash_info->r5; break;
                        case 6: value = dump_state.crash_info->r6; break;
                        case 7: value = dump_state.crash_info->r7; break;
                        case 8: value = dump_state.crash_info->r8; break;
                        case 9: value = dump_state.crash_info->r9; break;
                        case 10: value = dump_state.crash_info->r10; break;
                        case 11: value = dump_state.crash_info->r11; break;
                        case 12: value = dump_state.crash_info->r12; break;
                        case 13: value = dump_state.crash_info->sp; break;
                        case 14: value = dump_state.crash_info->lr; break;
                        case 15: value = dump_state.crash_info->pc; break;
                        case 16: value = dump_state.crash_info->psr; break;
                        case 17: value = dump_state.crash_info->msp; break;
                        case 18: value = dump_state.crash_info->psp; break;
                        case 19: value = dump_state.crash_info->control; break;
                        case 20: value = dump_state.crash_info->basepri; break;
                        case 21: value = dump_state.crash_info->primask; break;
                        case 22: value = dump_state.crash_info->faultmask; break;
                        case 23: value = dump_state.crash_info->fpscr; break;
                        case 24: value = dump_state.crash_info->hfsr; break;
                        case 25: value = dump_state.crash_info->cfsr; break;
                        case 26: value = dump_state.crash_info->mmfar; break;
                        case 27: value = dump_state.crash_info->bfar; break;
                        case 28: value = dump_state.crash_info->afsr; break;
                        case 29: value = dump_state.crash_info->nvic_iser0; break;
                        case 30: value = dump_state.crash_info->lr_at_fault; break;
                    }
                    
                    snprintf(line, sizeof(line), "%s: ", register_names[dump_state.current_region]);
                    size_t len = strlen(line);
                    format_hex_word(&line[len], value);
                    line[len + 8] = '\0';  /* Null terminate after 8 hex chars */
                    prepare_string(line);
                    dump_state.current_region++;
                } else {
                    /* Basic registers done, send NVIC state info */
                    prepare_string("\r\nNVIC STATE:\r\n");
                    dump_state.state = STATE_SENDING_NVIC_INFO;
                    dump_state.current_region = 0;
                }
            }
            break;
            
        case STATE_SENDING_NVIC_INFO:
            if (dump_state.send_buffer_pos >= dump_state.send_buffer_len) {
                if (dump_state.current_region == 0) {
                    /* Send NVIC pending interrupts */
                    char line[80];
                    snprintf(line, sizeof(line), "NVIC_ISPR0: 0x%08X (Pending interrupts)", 
                             dump_state.crash_info->nvic_ispr0);
                    prepare_string(line);
                    dump_state.current_region++;
                } else if (dump_state.current_region == 1) {
                    /* Send NVIC active interrupts */
                    char line[80];
                    snprintf(line, sizeof(line), "NVIC_IABR0: 0x%08X (Active interrupts)", 
                             dump_state.crash_info->nvic_iabr0);
                    prepare_string(line);
                    dump_state.current_region++;
                } else {
                    /* NVIC info done, start memory dump */
                    prepare_string("\r\nMemory dump:\r\n");
                    dump_state.state = STATE_SENDING_MEMORY;
                    dump_state.current_region = 0;
                    dump_state.current_ptr = (uint8_t*)ram_regions[0].start;
                    dump_state.bytes_remaining = ram_regions[0].end - ram_regions[0].start;
                    dump_state.current_address = ram_regions[0].start;
                    dump_state.line_byte_count = 0;
                }
            }
            break;
            
        case STATE_SENDING_MEMORY:
            if (dump_state.send_buffer_pos >= dump_state.send_buffer_len) {
                /* Prepare next hex line */
                if (prepare_hex_line() == 0) {
                    /* Current region done, move to next */
                    dump_state.current_region++;
                    if (dump_state.current_region < (int)(sizeof(ram_regions)/sizeof(ram_regions[0]))) {
                        /* Prepare region header */
                        char header[64];
                        snprintf(header, sizeof(header), "\r\n%s:\r\n", ram_regions[dump_state.current_region].name);
                        prepare_string(header);
                        
                        /* Set up for new region */
                        dump_state.current_ptr = (uint8_t*)ram_regions[dump_state.current_region].start;
                        dump_state.bytes_remaining = ram_regions[dump_state.current_region].end - 
                                                     ram_regions[dump_state.current_region].start;
                        dump_state.current_address = ram_regions[dump_state.current_region].start;
                        dump_state.line_byte_count = 0;
                    } else {
                        /* All done, send footer */
                        prepare_string("\r\nEnd of dump\r\n");
                        dump_state.state = STATE_SENDING_FOOTER;
                    }
                }
            }
            break;
            
        case STATE_SENDING_FOOTER:
            if (dump_state.send_buffer_pos >= dump_state.send_buffer_len) {
                /* Footer sent, close connection */
                dump_state.state = STATE_CLOSING;
                uip_close();
            }
            break;
            
        default:
            break;
    }
    
    /* Send any pending data */
    if (dump_state.send_buffer_len > dump_state.send_buffer_pos && 
        (uip_newdata() || uip_acked() || uip_connected() || uip_poll())) {
        size_t to_send = dump_state.send_buffer_len - dump_state.send_buffer_pos;
        if (to_send > uip_mss()) {
            to_send = uip_mss();
        }
        
        memcpy(uip_appdata, &dump_state.send_buffer[dump_state.send_buffer_pos], to_send);
        uip_send(uip_appdata, to_send);
        dump_state.send_buffer_pos += to_send;
    }
    
    /* Check for timeouts */
    check_timeouts();
}

int tcp_crash_dump_is_complete(void) {
    return (dump_state.state == STATE_CLOSED || 
            dump_state.state == STATE_IDLE || 
            dump_state.state == STATE_ERROR);
}

int tcp_crash_dump_has_error(void) {
    return (dump_state.state == STATE_ERROR);
}

void tcp_crash_dump_poll(void) {
    /* Check timeouts even when not in uip appcall */
    if (dump_state.state == STATE_CONNECTING || 
        (dump_state.state >= STATE_SENDING_HEADER && dump_state.state <= STATE_SENDING_MEMORY)) {
        check_timeouts();
    }
}