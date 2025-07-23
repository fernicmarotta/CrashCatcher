get_target_property(OUTPUT_NAME ${PROJECT_NAME} OUTPUT_NAME)

# Bin and Hex Conversion
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND rm -f *.bin *.hex *.srec COMMENT "Removing old bin, hex and srec files..."
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=._user_heap_stack --remove-section=.itcm --remove-section=._rw_dtcm --remove-section=.rw_no_init --remove-section=.sw_cfg_bckp --remove-section=.ramfunc --remove-section=.ram_no_cache $<TARGET_FILE:${PROJECT_NAME}> ${OUTPUT_NAME}.hex COMMENT "Generating hex file (${OUTPUT_NAME}.hex)"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND srec_cat ${OUTPUT_NAME}.hex -Intel -o ${OUTPUT_NAME}.srec -Motorola COMMENT "Generating srec file  (${OUTPUT_NAME}.srec)"
)