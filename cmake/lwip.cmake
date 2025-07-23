# lwIP configuration for minimal TCP stack
# Optimized for crash dump transmission over TCP

# lwIP paths - note the version variable is ${lwIP} from libraries.cmake
set(LWIP_DIR "${USER_PATH}/lwIP/lwIP/${lwIP}/lwip")
set(LWIP_PORT_DIR "${USER_PATH}/lwIP/lwIP/${lwIP}/ports")

# Core lwIP sources - minimal set for TCP
set(LWIP_CORE_SRCS
    # Core functionality
    ${LWIP_DIR}/src/core/init.c
    ${LWIP_DIR}/src/core/def.c
    ${LWIP_DIR}/src/core/inet_chksum.c
    ${LWIP_DIR}/src/core/ip.c
    ${LWIP_DIR}/src/core/mem.c
    ${LWIP_DIR}/src/core/memp.c
    ${LWIP_DIR}/src/core/netif.c
    ${LWIP_DIR}/src/core/pbuf.c
    ${LWIP_DIR}/src/core/stats.c
    ${LWIP_DIR}/src/core/sys.c
    ${LWIP_DIR}/src/core/tcp.c
    ${LWIP_DIR}/src/core/tcp_in.c
    ${LWIP_DIR}/src/core/tcp_out.c
    ${LWIP_DIR}/src/core/timeouts.c
    
    # IPv4 support
    ${LWIP_DIR}/src/core/ipv4/etharp.c
    ${LWIP_DIR}/src/core/ipv4/icmp.c
    ${LWIP_DIR}/src/core/ipv4/ip4.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
    ${LWIP_DIR}/src/core/ipv4/ip4_frag.c
    
    # Network interface
    ${LWIP_DIR}/src/netif/ethernet.c
)

# NO_SYS port for bare metal
set(LWIP_PORT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lwip/sys_arch.c
)

# NO CMSIS-Driver - we'll use STM32H7 HAL directly
# set(LWIP_CMSIS_SRCS
#     ${LWIP_PORT_DIR}/cmsis-driver/netif/ethernetif.c
# )

# All lwIP sources
set(LWIP_SRCS
    ${LWIP_CORE_SRCS}
    ${LWIP_PORT_SRCS}
)

# lwIP include paths
set(LWIP_INCLUDES
    ${LWIP_DIR}/src/include
    ${LWIP_PORT_DIR}/no_os/include
    ${LWIP_PORT_DIR}/cmsis-driver
    ${LWIP_PORT_DIR}/cmsis-driver/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lwip  # For lwipopts.h
)

# Add to project sources and includes
set(CC_SRC_FILES ${CC_SRC_FILES} ${LWIP_SRCS})
set(INC_PATHS ${INC_PATHS} ${LWIP_INCLUDES})