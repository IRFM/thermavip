cmake_minimum_required(VERSION 3.16)
project(MultiPlot VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(MultiPlot main.cpp )
# Configure project
set(TARGET_PROJECT MultiPlot)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
