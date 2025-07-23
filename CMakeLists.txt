# USER HEADER BEGIN
# USER HEADER END
# AUTOMATIC HEADER BEGIN
### pe-project-tools (5.5.0rc0)
cmake_minimum_required(VERSION 3.25)
execute_process(COMMAND pe-internal-tool data_path OUTPUT_VARIABLE data_path)
list(APPEND CMAKE_MODULE_PATH "${data_path}")
include(project_variables)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} DESCRIPTION ${PROJECT_DESCRIPTION})
include(ccpp)
# AUTOMATIC HEADER END
### pe-project-tools (5.4.0rc2) automatic header ###################################################

include(cmake/libraries.cmake)
include(cmake/cflags.cmake)
include(cmake/defines.cmake)
include(cmake/includes.cmake)
include(cmake/src.asm.cmake)
include(cmake/src.c.cmake)

add_executable(${PROJECT_NAME} ${CC_SRC_FILES} ${ASM_SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${INC_PATHS})

include(cmake/subdirectories.cmake)

include(cmake/postbuild.cmake)