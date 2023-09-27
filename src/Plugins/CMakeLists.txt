if(WITH_FFMEG)
add_subdirectory(Ffmpeg)
endif()

if(WITH_PYTHON)
add_subdirectory(Python)
endif()

add_subdirectory(IRCameras)

if(WITH_HDF5)
add_subdirectory(H5StillImage)
endif()