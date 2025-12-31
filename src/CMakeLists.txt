cmake_minimum_required(VERSION 3.20.0)

# Add Pigweed as an extra module
get_filename_component(PIGWEED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/pigweed ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${PIGWEED_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pico_pigweed_practice)

target_sources(app PRIVATE main.cpp)

# Enable C++17
target_compile_features(app PRIVATE cxx_std_17)

# Verify Pigweed is loaded
if(NOT TARGET pw_build.empty)
  message(WARNING "Pigweed targets not found. Make sure the module is loaded.")
endif()

# Create the proto library using Pigweed's function
# We rely on pw_proto_library which should be available via Pigweed module
if(COMMAND pw_proto_library)
  pw_proto_library(service_proto
    SOURCES proto/service.proto
  )

  target_link_libraries(app PRIVATE
    service_proto.pwpb
    service_proto.pwpb_rpc
    pw_rpc.server
    pw_rpc.pwpb.method_union
    pw_log
    pw_status
    pw_hdlc
  )
else()
  message(FATAL_ERROR "pw_proto_library command not found.")
endif()
