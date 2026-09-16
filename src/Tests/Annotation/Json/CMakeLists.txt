cmake_minimum_required(VERSION 3.16)
project(TestEventJson VERSION 1.0 LANGUAGES C CXX)

add_executable(TestEventJson TestEventJson.cpp)
set(TARGET_PROJECT TestEventJson)
include(${THERMAVIP_TEST_SETUP_FILE})
