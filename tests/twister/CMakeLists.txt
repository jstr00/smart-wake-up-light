cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_wakeup_unit_tests)

target_sources(app PRIVATE
  test_logic.c
  ../../app/main_app/src/logic/light_model.c
  ../../app/main_app/src/logic/coap_payload.c
  ../../app/main_app/src/logic/encoder_decoder.c
)
target_include_directories(app PRIVATE ../../app/main_app/src/logic)
