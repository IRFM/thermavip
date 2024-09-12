cmake_minimum_required(VERSION 3.16)
project(StreamingMandelbrot VERSION 1.0 LANGUAGES C CXX)


# Create executable
add_executable(StreamingMandelbrot main.cpp )
# Configure project
set(TARGET_PROJECT StreamingMandelbrot)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
