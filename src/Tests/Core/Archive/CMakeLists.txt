cmake_minimum_required(VERSION 3.16)
project(TestArchive VERSION 1.0 LANGUAGES C CXX)

add_executable(TestArchive TestArchive.cpp)
set(TARGET_PROJECT TestArchive)
include(${THERMAVIP_TEST_SETUP_FILE})
