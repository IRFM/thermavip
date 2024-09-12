cmake_minimum_required(VERSION 3.16)
project(VStack VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(VStack main.cpp )
# Configure project
set(TARGET_PROJECT VStack)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
