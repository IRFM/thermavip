cmake_minimum_required(VERSION 3.16)
project(TestNDArray VERSION 1.0 LANGUAGES C CXX)

add_executable(TestNDArray TestNDArray.cpp)
set(TARGET_PROJECT TestNDArray)
include(${THERMAVIP_TEST_SETUP_FILE})
