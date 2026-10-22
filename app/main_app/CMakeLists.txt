# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_wake_up_light)

target_include_directories(app PRIVATE src)
target_sources(app PRIVATE
    src/main.c
    src/led_control.c
    src/encoder.c
    src/status_led.c
    src/logic/light_model.c
    src/logic/coap_payload.c
    src/logic/encoder_decoder.c
    src/network/ot_manager.c
    src/network/coap_wakeup_server.c
    src/network/srp_service.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/local/app.cmake OPTIONAL)
