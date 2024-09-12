cmake_minimum_required(VERSION 3.16)
project(PolarGauge VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(PolarGauge main.cpp )
# Configure project
set(TARGET_PROJECT PolarGauge)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
