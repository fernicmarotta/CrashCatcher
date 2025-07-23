/**
 * @file ethernetif.h
 * @brief Ethernet interface header
 */

#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

/* Function prototypes */
err_t ethernetif_init(struct netif *netif);
void ethernetif_input(struct netif *netif);

#endif /* ETHERNETIF_H */