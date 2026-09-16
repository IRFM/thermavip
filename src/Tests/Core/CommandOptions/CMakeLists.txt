cmake_minimum_required(VERSION 3.16)
project(TestCommandOptions VERSION 1.0 LANGUAGES C CXX)

add_executable(TestCommandOptions TestCommandOptions.cpp)
set(TARGET_PROJECT TestCommandOptions)
include(${THERMAVIP_TEST_SETUP_FILE})
