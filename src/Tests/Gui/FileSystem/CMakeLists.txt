cmake_minimum_required(VERSION 3.16)
project(TestFileSystem VERSION 1.0 LANGUAGES C CXX)

add_executable(TestFileSystem TestFileSystem.cpp)
set(TARGET_PROJECT TestFileSystem)
include(${THERMAVIP_TEST_SETUP_FILE})
