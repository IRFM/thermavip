cmake_minimum_required(VERSION 3.16)
project(TestProcessingObject VERSION 1.0 LANGUAGES C CXX)

add_executable(TestProcessingObject TestProcessingObject.cpp)
set(TARGET_PROJECT TestProcessingObject)
include(${THERMAVIP_TEST_SETUP_FILE})
