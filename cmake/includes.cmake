include(cmake/${CMAKE_C_COMPILER_ID}/includes.cmake)

if(NOT DEFINED USER_PATH OR USER_PATH STREQUAL "")
    message(FATAL_ERROR "USER_PATH environment variable is not set!!")
endif ()

set( INC_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TcpDump
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lwip
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eth_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashCatcher/include
    ${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Inc
    ${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ${USER_PATH}/ARM/CMSIS/${ARM_CMSIS}/CMSIS/Core/Include
    ${USER_PATH}/ARM/CMSIS/${ARM_CMSIS}/CMSIS/Driver/Include
    ${USER_PATH}/ARM/CMSIS-Driver/${ARM_CMSIS_DRIVER}/Include
    ${USER_PATH}/ARM/CMSIS-Driver/${ARM_CMSIS_DRIVER}/Ethernet_PHY/DP83848C
    ${LWIP_PORT_DIR}/cmsis-driver/netif
    ${LWIP_PORT_DIR}/cmsis-driver
)