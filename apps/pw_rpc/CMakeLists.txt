cmake_minimum_required(VERSION 3.20)
include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_set_backend(pw_thread pw_thread_zephyr.thread)
pw_set_backend(pw_thread_id pw_thread_zephyr.id)
pw_set_backend(pw_thread_sleep pw_thread_zephyr.sleep)
pw_set_backend(pw_sync_binary_semaphore pw_sync_zephyr.binary_semaphore)
pw_set_backend(pw_sync_mutex pw_sync_zephyr.mutex)
#pw_set_backend(pw_sys_io pw_sys_io_zephyr)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app_pigweed)

target_sources(app PRIVATE main.cpp)

pw_proto_library(device_service_proto
  SOURCES
    proto/service.proto
  PREFIX
    practice_rpc
  STRIP_PREFIX
    proto
  DEPS
    pw_third_party.nanopb.proto
)

target_link_libraries(app PRIVATE
  device_service_proto.nanopb_rpc
  pw_tokenizer 
  pw_log 
  pw_stream
  pw_rpc.server
  pw_hdlc
  #pw_sys_io
)
