cmake_minimum_required(VERSION 3.16)
project(TestPythonDevices VERSION 1.0 LANGUAGES C CXX)

add_executable(TestPythonDevices TestPythonDevices.cpp)
set(TARGET_PROJECT TestPythonDevices)
include(${THERMAVIP_TEST_SETUP_FILE})
