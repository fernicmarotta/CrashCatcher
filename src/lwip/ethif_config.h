/**
 * @file ethif_config.h
 * @brief Ethernet interface configuration for DP83848C PHY
 */

#ifndef ETHIF_CONFIG_H
#define ETHIF_CONFIG_H

/* CMSIS Driver number used for Ethernet */
#define ETH_DRV_NUM         0

/* Ethernet PHY configuration for DP83848C */
#define ETH_PHY_ADDRESS     0x01  /* Default PHY address, adjust if needed */

/* Timeout configurations */
#define ETH_PHY_TIMEOUT     5000  /* PHY initialization timeout in ms */
#define ETH_LINK_TIMEOUT    5000  /* Link establishment timeout in ms */

/* MAC configuration */
#define ETH_MAC_ADDR0       0x02
#define ETH_MAC_ADDR1       0x00
#define ETH_MAC_ADDR2       0x00
#define ETH_MAC_ADDR3       0x00
#define ETH_MAC_ADDR4       0x00
#define ETH_MAC_ADDR5       0x01

/* Compatibility defines for lwIP */
#define MAC_ADDR0           ETH_MAC_ADDR0
#define MAC_ADDR1           ETH_MAC_ADDR1
#define MAC_ADDR2           ETH_MAC_ADDR2
#define MAC_ADDR3           ETH_MAC_ADDR3
#define MAC_ADDR4           ETH_MAC_ADDR4
#define MAC_ADDR5           ETH_MAC_ADDR5

/* Static IP configuration for crash dump server */
#define CRASH_DUMP_USE_STATIC_IP    1
#define CRASH_DUMP_IP_ADDR0         192
#define CRASH_DUMP_IP_ADDR1         168
#define CRASH_DUMP_IP_ADDR2         1
#define CRASH_DUMP_IP_ADDR3         100

#define CRASH_DUMP_NETMASK0         255
#define CRASH_DUMP_NETMASK1         255
#define CRASH_DUMP_NETMASK2         255
#define CRASH_DUMP_NETMASK3         0

#define CRASH_DUMP_GATEWAY0         192
#define CRASH_DUMP_GATEWAY1         168
#define CRASH_DUMP_GATEWAY2         1
#define CRASH_DUMP_GATEWAY3         1

/* Crash dump server configuration */
#define CRASH_DUMP_SERVER_IP0       192
#define CRASH_DUMP_SERVER_IP1       168
#define CRASH_DUMP_SERVER_IP2       1
#define CRASH_DUMP_SERVER_IP3       10
#define CRASH_DUMP_SERVER_PORT      9999

/* Buffer sizes */
#define ETH_RX_BUFFER_SIZE          1536
#define ETH_TX_BUFFER_SIZE          1536

#endif /* ETHIF_CONFIG_H */