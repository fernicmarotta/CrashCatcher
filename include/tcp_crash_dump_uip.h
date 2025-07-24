#ifndef TCP_CRASH_DUMP_UIP_H
#define TCP_CRASH_DUMP_UIP_H

#include "crash_info.h"

/* Initialize TCP crash dump module */
void tcp_crash_dump_init(void);

/* Start sending crash dump */
int tcp_crash_dump_start(crash_info_t *crash_info);

/* uIP application callback - must be called from uip_appcall */
void tcp_crash_dump_appcall(void);

/* Check if dump is complete */
int tcp_crash_dump_is_complete(void);

#endif /* TCP_CRASH_DUMP_UIP_H */