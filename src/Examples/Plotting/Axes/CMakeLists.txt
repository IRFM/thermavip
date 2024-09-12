cmake_minimum_required(VERSION 3.16)
project(Axes VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(Axes main.cpp)
# Configure project
set(TARGET_PROJECT Axes)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
