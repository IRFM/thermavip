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
