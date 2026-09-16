cmake_minimum_required(VERSION 3.16)
project(TestSerialization VERSION 1.0 LANGUAGES C CXX)

add_executable(TestSerialization TestSerialization.cpp)
set(TARGET_PROJECT TestSerialization)
include(${THERMAVIP_TEST_SETUP_FILE})
