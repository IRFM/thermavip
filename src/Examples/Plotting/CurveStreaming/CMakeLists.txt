cmake_minimum_required(VERSION 3.16)
project(CurveStreaming VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(CurveStreaming main.cpp)
# Configure project
set(TARGET_PROJECT CurveStreaming)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
