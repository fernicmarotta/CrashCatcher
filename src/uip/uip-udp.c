/**
 ***********************************************************************************************************************
 * @file uip-udp.c
 * @author fmarotta
 * @version 1.0.0
 * @date Creation: 26/02/2021
 * @date Last modification: 26/02/2021
 * @brief 
 * @par
 *  COPYRIGHT NOTICE: (c) 2021 Power Electronics.
 *  All rights reserved
 ***********************************************************************************************************************
    @addtogroup uip_udp
    @{
*/

#include "string.h"
#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "uip/netdev.h"
#include "board/board.h"
#include "uip/uip-udp.h"

/** get global lenght of payload for udp/tcp protocol */
extern uint16_t uip_slen;

void uip_udp_send_datagram(struct uip_udp_conn *udp_conn, void *data, int len )
{
    if(!data)
        return;

    /* Set udp socket to use for uip_process */
    uip_udp_conn = udp_conn;
    /* Set length of udp payload with length of msg */
    uip_slen = len;
    /* copy msg into buffer ethernet since initial udp payload position(uip_buf: is global ethernet buffer, the)*/
    memcpy(&uip_buf[UIP_LLH_LEN + UIP_IPUDPH_LEN], data, len > UIP_BUFSIZE? UIP_BUFSIZE: len);
    /* call uip_process to compose ethernet frame*/
    uip_process(UIP_UDP_SEND_CONN);
    /* If length of frame is >0 send frame*/
    if (uip_len > 0)
    {
        /* process arp table before sending frame, to fill it with MAC dst */
        uip_arp_out();
        /* low layer send */
        netdev_send ();
        /* reset length */
        uip_len = 0;
    }
    /* reset payload length */
    uip_slen = 0;
}

/** 
    @}
 */
/************************* (C) COPYRIGHT Power Electronics *****END OF FILE********************************************/
