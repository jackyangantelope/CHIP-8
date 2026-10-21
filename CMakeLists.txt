cmake_minimum_required(VERSION 3.20)
set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Adafruit Fruit Jam" FORCE)
set(PICO_PSRAM_CS_PIN 47 CACHE STRING "Fruit Jam PSRAM CS" FORCE)
set(PICO_PSRAM_SIZE_BYTES 0x800000 CACHE STRING "Fruit Jam PSRAM size" FORCE)
set(PICO_DEFAULT_UART 0 CACHE STRING "Diagnostic UART" FORCE)
set(PICO_DEFAULT_UART_TX_PIN 44 CACHE STRING "Diagnostic UART TX" FORCE)
set(PICO_DEFAULT_UART_RX_PIN 45 CACHE STRING "Diagnostic UART RX" FORCE)
if(NOT DEFINED PICO_SDK_PATH OR NOT DEFINED PICO_PIO_USB_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH and PICO_PIO_USB_PATH")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(CHIP8 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
set(ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SHARED "${ROOT}/deps/pico_shared")
set(FATFS "${SHARED}/drivers/pico_fatfs")
set(HDMI "${SHARED}/drivers/pico_hdmi")
set(SAVED_TINYUSB_TOP "${TOP}")
set(TOP "${PICO_SDK_PATH}/lib/tinyusb")
check_and_add_pico_pio_usb_support()
set(TOP "${SAVED_TINYUSB_TOP}")
add_subdirectory("${FATFS}" "${CMAKE_BINARY_DIR}/pico_fatfs")
add_library(chip8_core STATIC "${ROOT}/core/jack_chip8.c")
target_include_directories(chip8_core PUBLIC "${ROOT}/core")
target_include_directories(chip8_core PRIVATE "${ROOT}/deps/c-octo/src")
target_compile_options(chip8_core PRIVATE -O3 -funsigned-char -ffunction-sections -fdata-sections)
add_library(chip8_hdmi STATIC
  "${ROOT}/platform/hdmi/hstx.c" "${ROOT}/platform/hdmi/video_output.c"
  "${HDMI}/hstx_data_island_queue.c" "${HDMI}/hstx_packet.c")
target_include_directories(chip8_hdmi PUBLIC "${ROOT}/platform/hdmi" "${HDMI}")
target_compile_definitions(chip8_hdmi PRIVATE JACK_HSTX_DOUBLE_BUFFER=1
  GPIOHSTXD0=15 GPIOHSTXD1=17 GPIOHSTXD2=19 GPIOHSTXCK=13
  GPIOHSTXINVERTED=1 JACK_HSTX_126MHZ=1)
target_compile_options(chip8_hdmi PRIVATE -O3 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(chip8_hdmi PUBLIC pico_stdlib pico_multicore hardware_dma
  hardware_sync hardware_clocks hardware_vreg hardware_pio hardware_spi
  hardware_i2c hardware_pwm hardware_adc)
add_executable(picoChip8
  "${ROOT}/firmware/chip8.c"
  "${ROOT}/platform/input/jack_input.c"
  "${ROOT}/platform/input/jack_input_devices.c"
  "${ROOT}/platform/text/jack_terminal.c"
  "${ROOT}/platform/input/jack_tinyusb_input.c"
  "${ROOT}/platform/input/jack_nespad_input.c")
target_include_directories(picoChip8 PRIVATE "${ROOT}/platform/input" "${ROOT}/platform/text"
  "${FATFS}/fatfs" "${SHARED}")
target_compile_definitions(picoChip8 PRIVATE HW_CONFIG=8 PICO_STACK_SIZE=4096
  JACK_TINYUSB_KEYBOARD_RECOVERY=1 PICO_PIO_USE_GPIO_BASE=1 CFG_TUH_RPI_PIO_USB=1
  PIO_USB_USE_PIO=2 PICO_DEFAULT_PIO_USB_DP_PIN=1
  SDCARD_PIN_CS=39 SDCARD_PIN_SCK=34 SDCARD_PIN_MOSI=35 SDCARD_PIN_MISO=36
  SDCARD_SPI=spi0 SDCARD_PIO=pio0)
target_compile_options(picoChip8 PRIVATE -O3 -Wall -Wextra -Werror -Wno-unused-parameter
  -Wno-cast-function-type -Wno-missing-field-initializers -Wno-attributes -Wno-sign-compare)
target_link_libraries(picoChip8 PRIVATE chip8_core chip8_hdmi pico_fatfs pico_stdlib
  pico_multicore hardware_dma hardware_pio hardware_spi hardware_clocks hardware_vreg
  tinyusb_host tinyusb_board tinyusb_pico_pio_usb m)
target_link_options(picoChip8 PRIVATE -Wl,--gc-sections -Wl,--print-memory-usage)
pico_set_program_name(picoChip8 "picoChip8")
pico_set_program_version(picoChip8 "0.1.1-fruit-jam-source")
pico_enable_stdio_uart(picoChip8 1)
pico_enable_stdio_usb(picoChip8 0)
include("${ROOT}/cmake/LoaderPartition.cmake")
chip8_link_for_loader(picoChip8)
pico_add_extra_outputs(picoChip8)
