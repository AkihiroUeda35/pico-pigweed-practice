cmake_minimum_required(VERSION 3.20)
# pw_thread (スレッド機能) の実体として Zephyr を使う
set(pw_thread_BACKEND "pw_thread_zephyr.thread" CACHE STRING "" FORCE)
set(pw_thread_id_BACKEND "pw_thread_zephyr.id" CACHE STRING "" FORCE)
set(pw_thread_sleep_BACKEND "pw_thread_zephyr.sleep" CACHE STRING "" FORCE)
# pw_sync (同期機能) の実体として Zephyr を使う
set(pw_sync_binary_semaphore_BACKEND "pw_sync_zephyr.binary_semaphore" CACHE STRING "" FORCE)
set(pw_sync_mutex_BACKEND "pw_sync_zephyr.mutex" CACHE STRING "" FORCE)
# Nanopb の場所を明示的に指定
set(dir_pw_third_party_nanopb "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/lib/nanopb" CACHE PATH "" FORCE)


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app_pigweed)

target_link_libraries(app PRIVATE pw_tokenizer pw_log)

target_sources(app PRIVATE main.cpp)

pw_proto_library(device_service_proto
  SOURCES
    proto/service.proto
  PREFIX
    practice_rpc
  STRIP_PREFIX
    proto
)

target_link_libraries(app PRIVATE
  device_service_proto.nanopb_rpc
  pw_rpc.server                  
  pw_rpc.nanopb.method_union       # Nanopb用のメソッド管理
)
