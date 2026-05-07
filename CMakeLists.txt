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
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(TestSlave C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(TestSlave TestSlave.c master_test.c uart_protocol.c slave_app.c reset_button.c)

pico_generate_pio_header(TestSlave ${CMAKE_CURRENT_LIST_DIR}/uart_tx.pio)
pico_generate_pio_header(TestSlave ${CMAKE_CURRENT_LIST_DIR}/uart_rx.pio)

pico_set_program_name(TestSlave "TestSlave")
pico_set_program_version(TestSlave "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(TestSlave 0)
pico_enable_stdio_usb(TestSlave 1)

# Add the standard library to the build
target_link_libraries(TestSlave  pico_stdlib hardware_pio hardware_i2c hardware_adc hardware_pwm)

# Define RP2350B variant to enable GPIOs 30-47
target_compile_definitions(TestSlave PRIVATE PICO_RP2350A=0)

# Add the standard include files to the build
target_include_directories(TestSlave PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(TestSlave)

