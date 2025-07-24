/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: netdev.c,v 1.8 2006/06/07 08:39:58 adam Exp $
 */

/**
 * \addtogroup STM32F7
 * \{
 * \addtogroup Uip-Arch
 * \{
 * \defgroup NetDev-Arch    Network Device
 * \{
 */

/*---------------------------------------------------------------------------*/
#include "netdev.h"
#include "stm32h7xx_hal.h"

#include "uip/uip.h"
#include "uip/uip_arp.h"

#include "Driver_ETH_MAC.h"
#include "Driver_ETH_PHY.h"

/*---------------------------------------------------------------------------*/
/** \brief  MAC device defined by the driver library */
extern ARM_DRIVER_ETH_MAC Driver_ETH_MAC0;
/** \brief  ETH device defined by the driver library */
extern ARM_DRIVER_ETH_PHY Driver_ETH_PHY0;

/** \brief  MAC device in use */
static ARM_DRIVER_ETH_MAC *mac;
/** \brief  ETH device in use */
static ARM_DRIVER_ETH_PHY *phy;
/** \brief  MAC address used by the ethernet module */
static ARM_ETH_MAC_ADDR own_mac_address;
/** \brief  Ethernet hardware capabilities */
static ARM_ETH_MAC_CAPABILITIES capabilities;

/*---------------------------------------------------------------------------*/

/**
 * \brief   Waits for PHY link to be up
 * \return  0 if link is up, -1 if timeout
 */
int netdev_wait_for_link_up(uint32_t timeout_ms);

/**
 * \brief   Dummy function needed by the MAC initialization as IRQ are not enabled. Therefore, this
 * function will never be called
 */
void ethernet_mac_notify (uint32_t event);

/*---------------------------------------------------------------------------*/

/**
 * \brief   Initializes the low level ethernet driver
 */
int netdev_init (void)
{
    mac = &Driver_ETH_MAC0;
    phy = &Driver_ETH_PHY0;

    mac->Initialize (ethernet_mac_notify);
    mac->PowerControl (ARM_POWER_FULL);

    mac->Control (ARM_ETH_MAC_CONFIGURE,
                  ARM_ETH_MAC_SPEED_100M | ARM_ETH_MAC_DUPLEX_FULL | ARM_ETH_MAC_ADDRESS_BROADCAST);
    mac->Control (ARM_ETH_MAC_CONTROL_TX, 1);
    mac->Control (ARM_ETH_MAC_CONTROL_RX, 1);
    
    return 0;
}

/**
 * \brief   Sets the MAC address for the ethernet
 */
void netdev_set_mac_address(struct uip_eth_addr *addr)
{
    /* Copy MAC address to driver structure */
    own_mac_address.b[0] = addr->addr[0];
    own_mac_address.b[1] = addr->addr[1];
    own_mac_address.b[2] = addr->addr[2];
    own_mac_address.b[3] = addr->addr[3];
    own_mac_address.b[4] = addr->addr[4];
    own_mac_address.b[5] = addr->addr[5];
}

/**
 * \brief   Gets the MAC address from the ethernet driver
 */
void netdev_get_mac_address(struct uip_eth_addr *addr)
{
    /* Get MAC address from driver */
    mac->GetMacAddress(&own_mac_address);
    
    /* Copy to uIP structure */
    addr->addr[0] = own_mac_address.b[0];
    addr->addr[1] = own_mac_address.b[1];
    addr->addr[2] = own_mac_address.b[2];
    addr->addr[3] = own_mac_address.b[3];
    addr->addr[4] = own_mac_address.b[4];
    addr->addr[5] = own_mac_address.b[5];
}

/**
 * \brief   Initializes the MAC address for the ethernet
 */
void netdev_init_mac (void)
{
    /* Initialize Media Access Controller */
    capabilities = mac->GetCapabilities ();

    /* Always set the MAC address we configured */
    mac->SetMacAddress (&own_mac_address);

    /* Initialize Physical Media Interface */
    if (phy->Initialize (mac->PHY_Read, mac->PHY_Write) == ARM_DRIVER_OK)
    {
        phy->PowerControl (ARM_POWER_FULL);
        phy->SetInterface (capabilities.media_interface);
        phy->SetMode (ARM_ETH_PHY_AUTO_NEGOTIATE);
    }
}

/**
 * \brief   Retrieves the MAC address used by the ethernet device
 * \param[out]  mac_addr    MAC address
 */
void netdev_get_mac (unsigned char *mac_addr)
{
    mac_addr[0] = own_mac_address.b[0];
    mac_addr[1] = own_mac_address.b[1];
    mac_addr[2] = own_mac_address.b[2];
    mac_addr[3] = own_mac_address.b[3];
    mac_addr[4] = own_mac_address.b[4];
    mac_addr[5] = own_mac_address.b[5];
}

/**
 * \brief   Reads a frame from the ethernet device to the uip_buf global pointer
 *
 * \return  0   When no frame has been read \n
 *          >0  With the frame's length, when a frame has been read
 */
unsigned int netdev_read (void)
{
    uint32_t ret = 0;

    uint32_t size = mac->GetRxFrameSize ();

    /* Check if there is a pending frame */
    if (size == 0)
    {
        return 0;
    }

    /* Frame excludes CRC */
    if ((size < 14) || (size > 1514))
    {
        /* Frame error, release it */
        mac->ReadFrame (NULL, 0);
    }

    int32_t len = mac->ReadFrame (uip_buf, UIP_BUFSIZE);

    if (len > 0)
    {
        /* Not interested on UDP packets */
        if (uip_buf[NETDEV_IP_TYPE] != NETDEV_TYPE_UDP)
        {
            ret = len;
        }
    }

    return ret;
}

/**
 * \brief   Sends the uip_buf global variable to the ethernet device
 */
void netdev_send (void)
{
    mac->SendFrame (uip_buf, uip_len, 0);
}

/*---------------------------------------------------------------------------*/

void ethernet_mac_notify (uint32_t event)
{
    /* Empty service as IRQ ar not enabled. It will never be called */
    (void)event;
}


/**
 * \brief   Waits for PHY link to be up
 * \param   timeout_ms Timeout in milliseconds
 * \return  0 if link is up, -1 if timeout
 */
int netdev_wait_for_link_up(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    ARM_ETH_LINK_STATE link_state;
    
    /* Check if PHY is initialized */
    if (phy == NULL) {
        return -1;
    }
    
    /* Wait for link to be up */
    do {
        link_state = phy->GetLinkState();
        
        if (link_state == ARM_ETH_LINK_UP) {
            return 0;  /* Link is up */
        }
        
        /* Small delay to avoid busy waiting */
        HAL_Delay(10);
        
    } while ((HAL_GetTick() - start_tick) < timeout_ms);
    
    return -1;  /* Timeout */
}

/**
 * \}
 * \}
 * \}
 */
