/**
 ***********************************************************************************************************************
 * @file uip-udp.h
 * @author fmarotta
 * @version 1.0.0
 * @date Creation: 26/02/2021
 * @date Last modification: 26/02/2021
 * @brief UDP services
 * @par
 *  COPYRIGHT NOTICE: (c) 2021 Power Electronics.
 *  All rights reserved
 ***********************************************************************************************************************
    @addtogroup uip
    @{
    @defgroup uip_udp uIP UDP
    @{
    @brief Udp Services
*/

#ifndef _UIP_UDP_H
#define _UIP_UDP_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "uip.h"

/**
* @brief  Send a datagram through UDP
* @details before require get MAC address of IP dst, and obtatin a valid uip_udp_conn
* @param[in] data: pointer of log msg
* @param[in] len: lenght of log msg
* @return NONE
*
*/
void uip_udp_send_datagram(struct uip_udp_conn *udp_conn, void *data, int len );

#ifdef __cplusplus
}
#endif
#endif /* _UIP_UDP_H */
/**
  * @}
  */
/** 
    @}
 */
/************************* (C) COPYRIGHT Power Electronics *****END OF FILE********************************************/
