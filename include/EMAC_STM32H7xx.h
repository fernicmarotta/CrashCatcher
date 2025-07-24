#ifndef EMAC_STM32H7XX_H
#define EMAC_STM32H7XX_H

/* EMAC driver configuration header */
#include "stm32h7xx_hal.h"

/* DMA descriptors must be in specific SRAM region */
#define EMAC_DMA_DESCRIPTOR_ADDR  0x30040000  /* SRAM3 */

/* Buffer counts */
#define EMAC_RX_BUF_COUNT  4
#define EMAC_TX_BUF_COUNT  2

#endif /* EMAC_STM32H7XX_H */