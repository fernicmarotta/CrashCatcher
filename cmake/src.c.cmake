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
        
        # Ethernet
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_eth.c"
        "${USER_PATH}/Keil/STM32H7xx_DFP/${STM32H7XX_DFP}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_eth_ex.c"
)
include(cmake/${CMAKE_C_COMPILER_ID}/src.c.cmake)

# Include uIP sources
include(cmake/uip.cmake)

# CrashCatcher sources
set(CC_SRC_CRASHCATCHER
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashCatcher/Core/src/CrashCatcher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashCatcher/Core/src/CrashCatcher_armv7m.S
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CrashCatcher_hooks.c
)

# Ethernet HAL configuration from nube_bootloader
set(CC_SRC_ETH_HAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/eth.c
)

# System sources
set(CC_SRC_SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system_stm32h7xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32h7xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jump_to_app.S
)

set(CC_SRC_FILES ${CC_SRC_FILES} ${CC_SRC_STM32_FILES} ${CC_SRC_CRASHCATCHER} ${CC_SRC_SYSTEM} ${CC_SRC_ETH_HAL})

