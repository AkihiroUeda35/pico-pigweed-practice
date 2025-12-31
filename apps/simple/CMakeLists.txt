cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pico2w_wifi_cpp)

target_sources(app PRIVATE main.cpp)