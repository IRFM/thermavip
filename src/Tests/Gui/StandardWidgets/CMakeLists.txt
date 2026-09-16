cmake_minimum_required(VERSION 3.16)
project(TestStandardWidgets VERSION 1.0 LANGUAGES C CXX)

add_executable(TestStandardWidgets TestStandardWidgets.cpp)
set(TARGET_PROJECT TestStandardWidgets)
include(${THERMAVIP_TEST_SETUP_FILE})
