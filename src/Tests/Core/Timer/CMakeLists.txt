cmake_minimum_required(VERSION 3.16)
project(TestTimer VERSION 1.0 LANGUAGES C CXX)

add_executable(TestTimer TestTimer.cpp)
set(TARGET_PROJECT TestTimer)
include(${THERMAVIP_TEST_SETUP_FILE})
