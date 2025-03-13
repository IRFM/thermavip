
# For Windows only, copy Python dlls to thermavip folder
if (WIN32)
	MESSAGE("HDF5 bin path:  ${HDF5_BIN}")
	file(GLOB SHARED_LIBS LIST_DIRECTORIES true  "${HDF5_BIN}/*.dll" )

	foreach(dll ${SHARED_LIBS} )
		#MESSAGE("Found file ${dll}, copy to ${CMAKE_INSTALL_PREFIX}/thermavip")
		file(COPY ${dll} DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip)
	endforeach()
endif()
