set(ASM_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/flash/startup_stm32h743xx_gcc.S
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jump_to_app.S
)

include(cmake/${CMAKE_C_COMPILER_ID}/src.asm.cmake)