cmake_minimum_required(VERSION 3.16)
project(StreamingCurvePipeline VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(StreamingCurvePipeline main.cpp)
# Configure project
set(TARGET_PROJECT StreamingCurvePipeline)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
