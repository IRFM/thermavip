if(WITH_FFMPEG)
add_subdirectory(Ffmpeg)
else()
message(STATUS "FFmpeg plugin not built")
endif()

if(WITH_PYTHON)
add_subdirectory(Python)
else()
message(STATUS "Python plugin not built")
endif()

add_subdirectory(IRCameras)

if(WITH_HDF5)
add_subdirectory(H5StillImage)
else()
message(STATUS "HDF5 plugin not built")
endif()