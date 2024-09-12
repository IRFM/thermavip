cmake_minimum_required(VERSION 3.16)
project(Scatter VERSION 1.0 LANGUAGES C CXX)


# Create executable
add_executable(Scatter main.cpp )
# Configure project
set(TARGET_PROJECT Scatter)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
