cmake_minimum_required(VERSION 3.16)
project(SimpleSerialize VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(SimpleSerialize main.cpp objects.h)
# Configure project
set(TARGET_PROJECT SimpleSerialize)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})

