# uIP configuration for minimal TCP stack
# Using uIP from nube_bootloader

# uIP sources
set(UIP_SRCS
    # Core uIP stack
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uip/uip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uip/uip_arp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uip/uip_timer.c
    
    # Network drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/EMAC_STM32H7xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/PHY_DP83848C.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/netdev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_crash_dump_uip.c
)

# uIP include paths
set(UIP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uip
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add to project sources and includes
set(CC_SRC_FILES ${CC_SRC_FILES} ${UIP_SRCS})
set(INC_PATHS ${INC_PATHS} ${UIP_INCLUDES})