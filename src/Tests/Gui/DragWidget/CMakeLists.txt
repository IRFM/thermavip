cmake_minimum_required(VERSION 3.16)
project(TestDragWidget VERSION 1.0 LANGUAGES C CXX)

add_executable(TestDragWidget TestDragWidget.cpp)
set(TARGET_PROJECT TestDragWidget)
include(${THERMAVIP_TEST_SETUP_FILE})
