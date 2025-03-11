cmake_minimum_required(VERSION 3.16)
project(PieChart VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(PieChart main.cpp )
# Configure project
set(TARGET_PROJECT PieChart)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
