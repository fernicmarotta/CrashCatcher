# Minimal STM32H7 HAL files - only what's needed for CrashCatcher
set(CC_SRC_STM32_FILES
        # Core HAL
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c"
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c"
        
        # Clock and Power
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c"
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c"
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c"
        
        # GPIO
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c"
)
#/home/fnicolas/.wine/drive_c/users/fnicolas/AppData/Local/Arm/Packs/lwIP/lwIP
set(LWIP "${USER_PATH}/lwIP/lwIP/")

include(cmake/${CMAKE_C_COMPILER_ID}/src.c.cmake)

# Include lwIP sources
include(cmake/lwip.cmake)

# CrashCatcher sources
set(CC_SRC_CRASHCATCHER
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashCatcher/Core/src/CrashCatcher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashCatcher/Core/src/CrashCatcher_armv7m.S
)

# NO Ethernet CMSIS drivers - using STM32H7 HAL directly
# set(CC_SRC_ETH_DRIVERS
#     ${USER_PATH}/ARM/CMSIS-Driver/${ARM_CMSIS_DRIVER}/Ethernet_PHY/DP83848C/PHY_DP83848C.c
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/eth_driver/ETH_STM32H7xx.c
# )

# TcpDump sources for CrashCatcher
set(CC_SRC_TCPDUMP
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TcpDump/tcp_crash_dump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TcpDump/tcp_crash_dump_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lwip/sys_arch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lwip/ethernetif_minimal.c
)

# System sources
set(CC_SRC_SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system_stm32h7xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32h7xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jump_to_app.S
)

set(CC_SRC_FILES ${CC_SRC_FILES} ${CC_SRC_STM32_FILES} ${CC_SRC_CRASHCATCHER} ${CC_SRC_TCPDUMP} ${CC_SRC_SYSTEM})

