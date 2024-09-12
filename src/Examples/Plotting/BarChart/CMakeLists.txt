cmake_minimum_required(VERSION 3.16)
project(BarChart VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(BarChart main.cpp)
# Configure project
set(TARGET_PROJECT BarChart)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
