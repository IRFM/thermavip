cmake_minimum_required(VERSION 3.16)
project(StreamingMandelbrotPipeline VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(StreamingMandelbrotPipeline main.cpp)
# Configure project
set(TARGET_PROJECT StreamingMandelbrotPipeline)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})

