cmake_minimum_required(VERSION 3.16)
project(HeavyCurveStreaming VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(HeavyCurveStreaming main.cpp )
# Configure project
set(TARGET_PROJECT HeavyCurveStreaming)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
