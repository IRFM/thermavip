cmake_minimum_required(VERSION 3.16)
project(TestLogSharedMemory VERSION 1.0 LANGUAGES C CXX)

add_executable(TestLogSharedMemory TestLogSharedMemory.cpp)
set(TARGET_PROJECT TestLogSharedMemory)
include(${THERMAVIP_TEST_SETUP_FILE})
