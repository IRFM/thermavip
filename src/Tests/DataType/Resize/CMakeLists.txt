cmake_minimum_required(VERSION 3.16)
project(TestResize VERSION 1.0 LANGUAGES C CXX)

add_executable(TestResize TestResize.cpp)
set(TARGET_PROJECT TestResize)
include(${THERMAVIP_TEST_SETUP_FILE})
