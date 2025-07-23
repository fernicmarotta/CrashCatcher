if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-int-conversion -Wno-incompatible-pointer-types -Wuninitialized -Wsign-compare -Wtype-limits -Wno-unused-variable -Wno-unused-parameter -Wenum-compare -Wenum-conversion -fno-common -munaligned-access -Og -g3 -ffunction-sections -fdata-sections")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-int-conversion -Wno-incompatible-pointer-types -Wuninitialized -Wsign-compare -Wtype-limits -Wno-unused-variable -Wno-unused-parameter -Wenum-compare -Wenum-conversion -fno-common -munaligned-access -Os -ffunction-sections -fdata-sections")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized -Wsign-compare -Wtype-limits -Wno-unused-variable -Wno-unused-parameter -Wenum-compare -Wenum-conversion")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_SOURCE_DIR}/flash/${CMAKE_PROJECT_NAME}.ld -Wl,--gc-sections")
