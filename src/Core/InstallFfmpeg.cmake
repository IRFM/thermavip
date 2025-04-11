


MESSAGE("Ffmpeg libs path:  ${COMPLETE_FFMPEG_LIBS}")

foreach(ffmpeg_lib IN ITEMS ${COMPLETE_FFMPEG_LIBS} )
	file(GLOB CORRECT_FFMPEG_LIB_PATH "${FFMPEG_LIB_DIR}/*${ffmpeg_lib}*${FFMPEG_SUFFIX}*")
	foreach(ffmpeg_lib_path ${CORRECT_FFMPEG_LIB_PATH})
		
		IF(ffmpeg_lib_path)
			message("Found file ${ffmpeg_lib_path}, copy to ${CMAKE_INSTALL_PREFIX}/thermavip")
			file(COPY ${ffmpeg_lib_path} DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip)
		endif()
	endforeach()
endforeach()
