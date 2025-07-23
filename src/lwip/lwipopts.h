/**
 * @file lwipopts.h
 * @brief lwIP configuration for minimal TCP crash dump transmission
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* NO_SYS - No operating system */
#define NO_SYS                          1
#define LWIP_TIMERS                     1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* Memory options - Minimize footprint for 64KB ITCM */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (2*1024)  /* 2KB heap */
#define MEMP_NUM_PBUF                   2
#define MEMP_NUM_TCP_PCB                1         /* Only need 1 connection */
#define MEMP_NUM_TCP_PCB_LISTEN         1
#define MEMP_NUM_TCP_SEG                2         /* Minimal segments */

/* PBUF options */
#define PBUF_POOL_SIZE                  2
#define PBUF_POOL_BUFSIZE              256       /* Smaller buffers */
#define TCP_MSS                        256        /* Smaller MSS */
#define TCP_SND_BUF                    512        /* Minimal send buffer */
#define TCP_WND                        512        /* Minimal window */

/* TCP options */
#define LWIP_TCP                        1
#define TCP_QUEUE_OOSEQ                 0         /* Disable out-of-order queueing */
#define TCP_OVERSIZE                    0         /* Disable segment optimization */
#define LWIP_TCP_TIMESTAMPS             0
#define LWIP_TCP_KEEPALIVE              0
#define LWIP_DISABLE_TCP_SANITY_CHECKS  1         /* Disable sanity checks for small memory */

/* IP options */
#define LWIP_IPV4                       1
#define IP_REASSEMBLY                   0         /* No reassembly to save memory */
#define IP_FRAG                         0         /* No fragmentation */
#define IP_REASS_MAX_PBUFS              0
#define IP_DEFAULT_TTL                  64

/* ICMP - Disabled to save space */
#define LWIP_ICMP                       0

/* DHCP - Disabled for static IP */
#define LWIP_DHCP                       0
#define LWIP_AUTOIP                     0
#define LWIP_ARP                        1
#define ARP_TABLE_SIZE                  4

/* Network interfaces */
#define LWIP_NETIF_HOSTNAME             0
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

/* Statistics - Disabled to save space */
#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0

/* Debugging - Enable only for development */
#define LWIP_DEBUG                      0
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_OFF

/* Checksum options */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* CMSIS-Driver support */
/* LWIP_PROVIDE_ERRNO is already defined in arch/cc.h */

/* TCP Performance tuning for crash dumps */
#define TCP_SND_QUEUELEN                2         /* Minimal queue */
#define TCP_SNDLOWAT                    (TCP_SND_BUF/2)

/* Disable unused features */
#define LWIP_UDP                        0
#define LWIP_RAW                        0
#define LWIP_SNMP                       0
#define LWIP_IGMP                       0
#define LWIP_DNS                        0
#define LWIP_MDNS_RESPONDER             0
#define LWIP_NUM_NETIF_CLIENT_DATA      0

/* Thread safety (not needed for NO_SYS) */
#define SYS_LIGHTWEIGHT_PROT            0

/* Custom memory functions (optional) */
#define MEM_LIBC_MALLOC                 0
#define MEMP_MEM_MALLOC                 0

#endif /* LWIPOPTS_H */