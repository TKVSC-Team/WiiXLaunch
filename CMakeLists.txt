cmake_minimum_required(VERSION 3.15)
# Option to select target platform
if(NOT DEFINED PLATFORM)
    if(DEFINED SWITCH)
        set(PLATFORM "SWITCH")
    elseif(DEFINED WIIU)
        set(PLATFORM "WIIU")
    elseif(DEFINED CEMU)
        set(PLATFORM "CEMU")
    else()
        message(STATUS "No PLATFORM specified. Defaulting to SWITCH (Use -DPLATFORM=SWITCH, -DPLATFORM=WIIU, or -DPLATFORM=CEMU)")
        set(PLATFORM "SWITCH")
    endif()
endif()

if(PLATFORM STREQUAL "CEMU" OR PLATFORM STREQUAL "WIIU")
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
    set(CMAKE_C_COMPILER powerpc-eabi-gcc)
    set(CMAKE_CXX_COMPILER powerpc-eabi-g++)
endif()

project(WiiXLaunch C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically synchronize master wiixlaunch.json into build/generated/
execute_process(
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_config.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE CONFIG_GEN_RESULT
)

# Include directories: Order matters! build/generated/include shadows vendor/exlaunch headers cleanly
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/build/generated/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(PLATFORM STREQUAL "SWITCH")
    message(STATUS "Building WiiXLaunch for Nintendo Switch (ARM64)")
    add_compile_definitions(__SWITCH__=1 WIIXL_SWITCH=1)

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/exlaunch/source
    )

    file(GLOB_RECURSE SOURCES "src/*.cpp")

    add_executable(wiixlaunch_switch ${SOURCES})
    target_link_libraries(wiixlaunch_switch PRIVATE)

elseif(PLATFORM STREQUAL "WIIU")
    message(STATUS "Building WiiXLaunch for Nintendo Wii U (PowerPC)")
    add_compile_definitions(__WIIU__=1 WIIXL_WIIU=1)

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/wut/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/wups/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libfunctionpatcher/include
    )

    file(GLOB_RECURSE SOURCES "src/*.cpp")

    add_library(wiixlaunch_wiiu SHARED ${SOURCES})
    target_link_libraries(wiixlaunch_wiiu PRIVATE)

elseif(PLATFORM STREQUAL "CEMU")
    message(STATUS "Building WiiXLaunch for Cemu Code Cave (PowerPC)")
    add_compile_definitions(__CEMU__=1 WIIXL_CEMU=1)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE -msdata=none")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE -msdata=none")
    set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -T ${CMAKE_CURRENT_SOURCE_DIR}/scripts/cemu.ld -Wl,-q")

    file(GLOB_RECURSE SOURCES "src/*.cpp")
    
    set(TARGET_CEMU wiixlaunch_cemu)
    add_executable(${TARGET_CEMU} ${SOURCES})

    add_custom_command(TARGET ${TARGET_CEMU} POST_BUILD
        COMMAND powerpc-eabi-objcopy -O binary --set-section-flags .bss=alloc,load,contents $<TARGET_FILE:${TARGET_CEMU}> ${CMAKE_BINARY_DIR}/payload.bin
        COMMENT "Extracting raw binary for Cemu Graphic Pack..."
    )

else()
    message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}. Must be SWITCH, WIIU, or CEMU.")
endif()

# Post-build deployment step
add_custom_target(deploy
    COMMAND ${CMAKE_COMMAND} -E echo "Running WiiXLaunch Deployment Packager..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy.py
    COMMENT "Packaging build artifacts into deploy/ folder..."
)
