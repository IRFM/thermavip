cmake_minimum_required(VERSION 3.16)
project(TestLogFormat VERSION 1.0 LANGUAGES C CXX)

add_executable(TestLogFormat TestLogFormat.cpp)
set(TARGET_PROJECT TestLogFormat)
include(${THERMAVIP_TEST_SETUP_FILE})
