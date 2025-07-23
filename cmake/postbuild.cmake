# Set the output name for the binary
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_${PROJECT_VERSION}")

include(cmake/${CMAKE_C_COMPILER_ID}/postbuild.cmake)