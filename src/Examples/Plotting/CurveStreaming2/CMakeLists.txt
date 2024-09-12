cmake_minimum_required(VERSION 3.16)
project(CurveStreaming2 VERSION 1.0 LANGUAGES C CXX)


# Create executable
add_executable(CurveStreaming2 main.cpp)
# Configure project
set(TARGET_PROJECT CurveStreaming2)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})

