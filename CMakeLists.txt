cmake_minimum_required(VERSION 3.16)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(OpenMiniDisplay C CXX ASM)

pico_sdk_init()

add_subdirectory(src)
