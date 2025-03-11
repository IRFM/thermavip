cmake_minimum_required(VERSION 3.16)
project(CurveStyles VERSION 1.0 LANGUAGES C CXX)


add_executable(CurveStyles main.cpp )
set(TARGET_PROJECT CurveStyles)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
