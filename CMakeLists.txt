cmake_minimum_required(VERSION 3.15)
project(WiiXLaunch C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically synchronize master wiixlaunch.json into build/generated/
execute_process(
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_config.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE CONFIG_GEN_RESULT
)

# Option to select target platform
if(NOT DEFINED PLATFORM)
    if(DEFINED SWITCH)
        set(PLATFORM "SWITCH")
    elseif(DEFINED WIIU)
        set(PLATFORM "WIIU")
    else()
        message(STATUS "No PLATFORM specified. Defaulting to SWITCH (Use -DPLATFORM=SWITCH or -DPLATFORM=WIIU)")
        set(PLATFORM "SWITCH")
    endif()
endif()

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

else()
    message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}. Must be SWITCH or WIIU.")
endif()

# Post-build deployment step
add_custom_target(deploy
    COMMAND ${CMAKE_COMMAND} -E echo "Running WiiXLaunch Deployment Packager..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy.py
    COMMENT "Packaging build artifacts into deploy/ folder..."
)
