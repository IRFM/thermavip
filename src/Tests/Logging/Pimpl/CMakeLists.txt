cmake_minimum_required(VERSION 3.16)
project(TestPimpl VERSION 1.0 LANGUAGES C CXX)

add_executable(TestPimpl TestPimpl.cpp)
set(TARGET_PROJECT TestPimpl)
include(${THERMAVIP_TEST_SETUP_FILE})
