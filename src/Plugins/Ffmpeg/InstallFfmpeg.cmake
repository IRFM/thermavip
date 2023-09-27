


MESSAGE("Ffmpeg libs path:  ${COMPLETE_FFMPEG_LIBS}")

foreach(ffmpeg_lib IN ITEMS ${COMPLETE_FFMPEG_LIBS} )
	file(GLOB CORRECT_FFMPEG_LIB_PATH "${FFMPEG_LIB_DIR}/*${ffmpeg_lib}*${FFMPEG_SUFFIX}*")
	foreach(ffmpeg_lib_path ${CORRECT_FFMPEG_LIB_PATH})
		
		IF(ffmpeg_lib_path)
			MESSAGE("Found file ${ffmpeg_lib_path}, copy to ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
			file(COPY ${ffmpeg_lib_path} DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip)
		endif()
	endforeach()
endforeach()

IF (WIN32)
	IF (NOT MSVC)
		MESSAGE("Custom mingw installation")
		# mingw/msys2
		file(GLOB ffmpeg_dlls "${CMAKE_SOURCE_DIR}/3rd_64/ffmpeg/install/bin/*.dll")
		MESSAGE("Copy files: ${ffmpeg_dlls}")
		file(COPY ${ffmpeg_dlls} DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip)
	ENDIF()
ENDIF()