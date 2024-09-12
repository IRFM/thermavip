cmake_minimum_required(VERSION 3.16)
project(Pipeline VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(Pipeline main.cpp pipeline.h)
# Configure project
set(TARGET_PROJECT Pipeline)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})

