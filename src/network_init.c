#include "stm32h7xx_hal.h"
#include "uip.h"
#include "uip_arp.h"
#include "netdev.h"
#include "tcp_crash_dump_uip.h"
#include "timer.h"
#include <string.h>

/* Network configuration */
#define BOOTLOADER_IP_ADDR0 192
#define BOOTLOADER_IP_ADDR1 168
#define BOOTLOADER_IP_ADDR2 1
#define BOOTLOADER_IP_ADDR3 200

#define BOOTLOADER_NETMASK0 255
#define BOOTLOADER_NETMASK1 255
#define BOOTLOADER_NETMASK2 255
#define BOOTLOADER_NETMASK3 0

#define BOOTLOADER_MAC_ADDR0 0x00
#define BOOTLOADER_MAC_ADDR1 0x0A
#define BOOTLOADER_MAC_ADDR2 0x35
#define BOOTLOADER_MAC_ADDR3 0x00
#define BOOTLOADER_MAC_ADDR4 0x01
#define BOOTLOADER_MAC_ADDR5 0x02

/* Timer for periodic uIP processing */
#define UIP_PERIODIC_TIMER_MS 500
#define UIP_ARP_TIMER_MS 10000

static struct timer periodic_timer;
static struct timer arp_timer;

/* Initialize Ethernet hardware pins */
static void ETH_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* Enable SYSCFG clock */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    
    /* Enable ETH clock */
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    
    /* Configure RMII pins */
    /* PA1: ETH_REF_CLK */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* PA2: ETH_MDIO */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* PA7: ETH_CRS_DV */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* PC1: ETH_MDC */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* PC4: ETH_RXD0 */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* PC5: ETH_RXD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* PG11: ETH_TX_EN */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* PG13: ETH_TXD0 */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* PG14: ETH_TXD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_14;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* Configure Ethernet RMII interface */
    HAL_SYSCFG_ETHInterfaceSelect(SYSCFG_ETH_RMII);
}

/* Initialize network stack */
int network_init(void) {
    struct uip_eth_addr mac_addr;
    
    /* Initialize Ethernet GPIO pins */
    ETH_GPIO_Init();
    
    /* Initialize network device driver */
    if (netdev_init() != 0) {
        return -1;
    }
    
    /* Initialize MAC address and PHY */
    netdev_init_mac();
    
    /* Initialize uIP */
    uip_init();
    
    /* Set MAC address */
    mac_addr.addr[0] = BOOTLOADER_MAC_ADDR0;
    mac_addr.addr[1] = BOOTLOADER_MAC_ADDR1;
    mac_addr.addr[2] = BOOTLOADER_MAC_ADDR2;
    mac_addr.addr[3] = BOOTLOADER_MAC_ADDR3;
    mac_addr.addr[4] = BOOTLOADER_MAC_ADDR4;
    mac_addr.addr[5] = BOOTLOADER_MAC_ADDR5;
    uip_setethaddr(mac_addr);
    
    /* Set IP address */
    uip_ipaddr_t ip_addr;
    uip_ipaddr(&ip_addr, BOOTLOADER_IP_ADDR0, BOOTLOADER_IP_ADDR1, 
               BOOTLOADER_IP_ADDR2, BOOTLOADER_IP_ADDR3);
    uip_sethostaddr(&ip_addr);
    
    /* Set netmask */
    uip_ipaddr_t netmask;
    uip_ipaddr(&netmask, BOOTLOADER_NETMASK0, BOOTLOADER_NETMASK1,
               BOOTLOADER_NETMASK2, BOOTLOADER_NETMASK3);
    uip_setnetmask(&netmask);
    
    /* Initialize timers */
    timer_set(&periodic_timer, UIP_PERIODIC_TIMER_MS);
    timer_set(&arp_timer, UIP_ARP_TIMER_MS);
    
    /* Initialize TCP crash dump module */
    tcp_crash_dump_init();
    
    /* Send gratuitous ARP announcement */
    /* Build ARP packet directly in uip_buf */
    struct arp_hdr {
        struct uip_eth_hdr ethhdr;
        u16_t hwtype;
        u16_t protocol;
        u8_t hwlen;
        u8_t protolen;
        u16_t opcode;
        struct uip_eth_addr shwaddr;
        u16_t sipaddr[2];
        struct uip_eth_addr dhwaddr;
        u16_t dipaddr[2];
    };
    
    struct arp_hdr *arp = (struct arp_hdr *)uip_buf;
    
    /* Fill Ethernet header - broadcast */
    memset(arp->ethhdr.dest.addr, 0xff, 6);  /* Broadcast MAC */
    memcpy(arp->ethhdr.src.addr, uip_ethaddr.addr, 6);
    arp->ethhdr.type = HTONS(UIP_ETHTYPE_ARP);
    
    /* Fill ARP header */
    arp->hwtype = HTONS(1);  /* Ethernet */
    arp->protocol = HTONS(UIP_ETHTYPE_IP);
    arp->hwlen = 6;
    arp->protolen = 4;
    arp->opcode = HTONS(1);  /* ARP Request */
    
    /* Sender hardware address (our MAC) */
    memcpy(arp->shwaddr.addr, uip_ethaddr.addr, 6);
    
    /* Sender IP address (our IP) */
    uip_ipaddr_t our_ip;
    uip_ipaddr(&our_ip, BOOTLOADER_IP_ADDR0, BOOTLOADER_IP_ADDR1, 
               BOOTLOADER_IP_ADDR2, BOOTLOADER_IP_ADDR3);
    uip_ipaddr_copy(arp->sipaddr, our_ip);
    
    /* Target hardware address (zeros for gratuitous ARP) */
    memset(arp->dhwaddr.addr, 0x00, 6);
    
    /* Target IP address (our IP - gratuitous ARP) */
    uip_ipaddr_copy(arp->dipaddr, our_ip);
    
    /* Send the gratuitous ARP */
    uip_len = sizeof(struct arp_hdr);
    netdev_send();
    
    return 0;
}

/* Process network packets - call this in main loop */
void network_process(void) {
    int i;
    
    /* Check for received packet */
    uip_len = netdev_read();
    if (uip_len > 0) {
        if (((struct uip_eth_hdr *)uip_buf)->type == htons(UIP_ETHTYPE_IP)) {
            uip_arp_ipin();
            uip_input();
            
            /* If packet should be sent, send it */
            if (uip_len > 0) {
                uip_arp_out();
                netdev_send();
            }
        } else if (((struct uip_eth_hdr *)uip_buf)->type == htons(UIP_ETHTYPE_ARP)) {
            uip_arp_arpin();
            
            /* If ARP reply should be sent, send it */
            if (uip_len > 0) {
                netdev_send();
            }
        }
    }
    
    /* Periodic processing */
    if (timer_expired(&periodic_timer)) {
        timer_reset(&periodic_timer);
        
        for (i = 0; i < UIP_CONNS; i++) {
            uip_periodic(i);
            
            /* If packet should be sent, send it */
            if (uip_len > 0) {
                uip_arp_out();
                netdev_send();
            }
        }
        
        /* Periodic ARP processing */
        if (timer_expired(&arp_timer)) {
            timer_reset(&arp_timer);
            uip_arp_timer();
        }
    }
}

/* uIP application callback */
void uip_appcall(void) {
    tcp_crash_dump_appcall();
}