cmake_minimum_required(VERSION 3.16)
project(TemporalMandelbrotPipeline VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(TemporalMandelbrotPipeline main.cpp mandelbrot.h)
# Configure project
set(TARGET_PROJECT TemporalMandelbrotPipeline)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})

