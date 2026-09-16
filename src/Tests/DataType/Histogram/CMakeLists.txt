cmake_minimum_required(VERSION 3.16)
project(TestHistogram VERSION 1.0 LANGUAGES C CXX)

add_executable(TestHistogram TestHistogram.cpp)
set(TARGET_PROJECT TestHistogram)
include(${THERMAVIP_TEST_SETUP_FILE})
