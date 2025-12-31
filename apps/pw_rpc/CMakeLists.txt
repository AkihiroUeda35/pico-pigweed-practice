cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app_pigweed)

target_link_libraries(app PRIVATE pw_tokenizer pw_log)

target_sources(app PRIVATE main.cpp)