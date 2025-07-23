/**
 * @file tcp_crash_dump.h
 * @brief TCP-based crash dump transmission interface
 */

#ifndef TCP_CRASH_DUMP_H
#define TCP_CRASH_DUMP_H

#include <stdint.h>

/**
 * Initialize TCP crash dump module
 * 
 * This function should be called during system initialization,
 * before any crash might occur. It sets up the network stack
 * but doesn't activate the interface until needed.
 */
void tcp_crash_dump_init(void);

/**
 * Process network packets (for polled mode)
 * 
 * Call this periodically if not using interrupts for Ethernet
 */
void tcp_crash_dump_poll(void);

#endif /* TCP_CRASH_DUMP_H */