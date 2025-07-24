# uIP TCP/IP stack sources
set(UIP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/uip")

set(UIP_SOURCES
    ${UIP_DIR}/uip.c
    ${UIP_DIR}/uip_arp.c
    ${UIP_DIR}/uip_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/arch/netdev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/drivers/EMAC_STM32H7xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/drivers/PHY_DP83848C.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/drivers/eth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/network_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src/tcp_crash_dump_uip.c
)

# uIP include directories
set(UIP_INCLUDES
    ${UIP_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/inc
)