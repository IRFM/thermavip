cmake_minimum_required(VERSION 3.16)
project(TestSqlQuery VERSION 1.0 LANGUAGES C CXX)

add_executable(TestSqlQuery TestSqlQuery.cpp)
set(TARGET_PROJECT TestSqlQuery)
include(${THERMAVIP_TEST_SETUP_FILE})
