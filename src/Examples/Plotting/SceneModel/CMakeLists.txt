cmake_minimum_required(VERSION 3.16)
project(SceneModel VERSION 1.0 LANGUAGES C CXX)

# Create executable
add_executable(SceneModel main.cpp )
# Configure project
set(TARGET_PROJECT SceneModel)
include(${THERMAVIP_EXAMPLE_SETUP_FILE})
