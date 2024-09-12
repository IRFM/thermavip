cmake_minimum_required(VERSION 3.16)
project(StyleSheet VERSION 1.0 LANGUAGES C CXX)


# Create executable
add_executable(StyleSheet main.cpp )
# Configure project
set(TARGET_PROJECT StyleSheet)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
