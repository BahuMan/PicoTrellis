# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoTrellis CXX C ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(MyTrellis)

# Add executable. Default name is the project name, version 0.1

add_executable(PicoTrellis PicoTrellis.cpp )

pico_set_program_name(PicoTrellis "PicoTrellis")
pico_set_program_version(PicoTrellis "0.1")

target_compile_definitions(PicoTrellis PRIVATE
    PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
    PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_RESET_TO_BOOTSEL=1
    PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR=1
)

# Generate PIO header
# pico_generate_pio_header(PicoTrellis ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PicoTrellis 0)
pico_enable_stdio_usb(PicoTrellis 1)

# Add the standard library to the build
target_link_libraries(PicoTrellis
        pico_stdlib
)

# Add the standard include files to the build
target_include_directories(PicoTrellis PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(PicoTrellis 
        hardware_spi
        hardware_i2c
        hardware_pio
        MyTrellis
        )

pico_add_extra_outputs(PicoTrellis)


