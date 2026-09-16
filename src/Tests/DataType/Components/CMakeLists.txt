cmake_minimum_required(VERSION 3.16)
project(TestComponents VERSION 1.0 LANGUAGES C CXX)

add_executable(TestComponents TestComponents.cpp)
set(TARGET_PROJECT TestComponents)
include(${THERMAVIP_TEST_SETUP_FILE})
