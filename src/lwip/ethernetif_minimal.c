/**
 * @file ethernetif_stm32h7.c
 * @brief Ethernet interface for lwIP on STM32H7 using HAL
 */

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"
#include "ethernetif.h"
#include "ethif_config.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* ETH Handle */
ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

/* Memory Pool Declaration */
#define ETH_RX_BUFFER_CNT         4U
#define ETH_TX_BUFFER_CNT         4U

/* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_BUFFER_CNT];
/* Ethernet Tx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_BUFFER_CNT];

/**
 * @brief Initialize the ethernet hardware
 */
static void low_level_init(struct netif *netif) {
    uint8_t macaddress[6];
    
    /* Set MAC hardware address */
    macaddress[0] = MAC_ADDR0;
    macaddress[1] = MAC_ADDR1;
    macaddress[2] = MAC_ADDR2;
    macaddress[3] = MAC_ADDR3;
    macaddress[4] = MAC_ADDR4;
    macaddress[5] = MAC_ADDR5;
    
    /* Initialize MAC address */
    netif->hwaddr[0] = macaddress[0];
    netif->hwaddr[1] = macaddress[1];
    netif->hwaddr[2] = macaddress[2];
    netif->hwaddr[3] = macaddress[3];
    netif->hwaddr[4] = macaddress[4];
    netif->hwaddr[5] = macaddress[5];
    
    /* Set netif MAC hardware address length */
    netif->hwaddr_len = ETH_HWADDR_LEN;
    
    /* Set netif MTU */
    netif->mtu = 1500;
    
    /* Accept broadcast address and ARP traffic */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
    
    /* Configure GPIO for Ethernet */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* Configure Ethernet pins */
    /* RMII pins */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    
    /* RMII_REF_CLK, RMII_MDIO, RMII_CRS_DV */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* RMII_TXD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* RMII_MDC, RMII_RXD0, RMII_RXD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* RMII_TX_EN, RMII_TXD0 */
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* Enable Ethernet clocks */
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    
    /* Reset Ethernet */
    __HAL_RCC_ETH1MAC_FORCE_RESET();
    __HAL_RCC_ETH1MAC_RELEASE_RESET();
    
    /* Init ETH */
    heth.Instance = ETH;
    heth.Init.MACAddr = macaddress;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;
    
    if (HAL_ETH_Init(&heth) != HAL_OK) {
        /* Initialization Error */
        return;
    }
    
    /* Initialize Tx Descriptors list */
    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
    
    /* Configure PHY: DP83848C */
    /* Enable PHY reset */
    uint32_t phyreg = 0;
    
    /* Reset PHY */
    HAL_ETH_WritePHYRegister(&heth, ETH_PHY_ADDRESS, 0x00, 0x8000);
    HAL_Delay(1);
    
    /* Auto-negotiation enable */
    HAL_ETH_WritePHYRegister(&heth, ETH_PHY_ADDRESS, 0x00, 0x1000);
    
    /* Wait for link */
    uint32_t timeout = 0;
    do {
        HAL_ETH_ReadPHYRegister(&heth, ETH_PHY_ADDRESS, 0x01, &phyreg);
        timeout++;
    } while (((phyreg & 0x0004) == 0) && (timeout < ETH_LINK_TIMEOUT));
    
    /* Start ETH */
    HAL_ETH_Start(&heth);
}

/**
 * @brief Send packet to interface
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    err_t errval = ERR_OK;
    ETH_BufferTypeDef Txbuffer[ETH_TX_BUFFER_CNT] = {0};
    uint32_t framelength = 0;
    uint32_t bufferoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t payloadoffset = 0;
    uint8_t *buffer;
    struct pbuf *q;
    uint32_t i = 0;
    
    /* Copy pbuf chain to TX buffer */
    for (q = p; q != NULL; q = q->next) {
        if (i >= ETH_TX_BUFFER_CNT) {
            return ERR_IF;
        }
        
        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len = q->len;
        framelength += q->len;
        
        if (i > 0) {
            Txbuffer[i-1].next = &Txbuffer[i];
        }
        
        i++;
    }
    
    TxConfig.Length = framelength;
    TxConfig.TxBuffer = Txbuffer;
    
    /* Send packet */
    HAL_StatusTypeDef ret = HAL_ETH_Transmit(&heth, &TxConfig, 1000);
    
    if (ret != HAL_OK) {
        errval = ERR_IF;
    }
    
    return errval;
}

/**
 * @brief Receive packet from interface
 */
static struct pbuf *low_level_input(struct netif *netif) {
    struct pbuf *p = NULL;
    ETH_BufferTypeDef RxBuff;
    uint32_t framelength = 0;
    
    if (HAL_ETH_IsRxDataAvailable(&heth) == 1) {
        HAL_ETH_GetRxDataBuffer(&heth, &RxBuff);
        HAL_ETH_GetRxDataLength(&heth, &framelength);
        
        /* Allocate pbuf */
        p = pbuf_alloc(PBUF_RAW, framelength, PBUF_POOL);
        
        if (p != NULL) {
            pbuf_take(p, RxBuff.buffer, framelength);
        }
        
        /* Build Rx descriptors */
        HAL_ETH_BuildRxDescriptors(&heth);
    }
    
    return p;
}

/**
 * @brief Process received ethernet packet
 */
void ethernetif_input(struct netif *netif) {
    struct pbuf *p;
    
    /* Get received frame */
    p = low_level_input(netif);
    
    /* No packet could be read */
    if (p == NULL) {
        return;
    }
    
    /* Pass to upper layer */
    if (netif->input(p, netif) != ERR_OK) {
        pbuf_free(p);
    }
}

/**
 * @brief Initialize the ethernet interface
 */
err_t ethernetif_init(struct netif *netif) {
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    
#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */
    
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    
    /* Set output function */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    
    /* Initialize hardware */
    low_level_init(netif);
    
    return ERR_OK;
}