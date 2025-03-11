cmake_minimum_required(VERSION 3.16)
project(HistogramStyles VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(HistogramStyles main.cpp )
# Configure project
set(TARGET_PROJECT HistogramStyles)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
