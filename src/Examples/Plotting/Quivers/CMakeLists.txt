cmake_minimum_required(VERSION 3.16)
project(Quiver VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(Quiver main.cpp )
# Configure project
set(TARGET_PROJECT Quiver)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
