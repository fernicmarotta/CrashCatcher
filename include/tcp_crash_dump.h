/**
 * @file tcp_crash_dump.h
 * @brief TCP-based crash dump transmission interface
 */

#ifndef TCP_CRASH_DUMP_H
#define TCP_CRASH_DUMP_H

#include <stdint.h>
#include "crash_info.h"

/**
 * Initialize TCP crash dump module
 * 
 * This function should be called during system initialization,
 * before any crash might occur. It sets up the network stack
 * but doesn't activate the interface until needed.
 */
void tcp_crash_dump_init(void);

/**
 * Start sending crash dump via TCP
 * 
 * @param crash_info Pointer to crash information structure
 * @return 0 on success, -1 on error
 */
int tcp_crash_dump_start(crash_info_t *crash_info);

/**
 * Check if dump transmission is complete
 * 
 * @return 1 if complete (success or error), 0 if still in progress
 */
int tcp_crash_dump_is_complete(void);

/**
 * Check if dump transmission has error
 * 
 * @return 1 if error occurred, 0 otherwise
 */
int tcp_crash_dump_has_error(void);

/**
 * Process network packets (for polled mode)
 * 
 * Call this periodically if not using interrupts for Ethernet
 */
void tcp_crash_dump_poll(void);

/**
 * uIP application callback for TCP connections
 * 
 * This is called by uIP stack for TCP events
 */
void tcp_crash_dump_appcall(void);

#endif /* TCP_CRASH_DUMP_H */