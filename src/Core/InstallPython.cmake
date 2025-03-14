
# For Windows only, copy Python dlls to thermavip folder
if (WIN32)
	MESSAGE("Python lib path:  ${PYTHON_RUNTIME}")
	file(GLOB SHARED_LIBS LIST_DIRECTORIES true  "${PYTHON_RUNTIME}/*.dll")

	foreach(dll ${SHARED_LIBS} )
		#MESSAGE("Found file ${dll}, copy to ${CMAKE_INSTALL_PREFIX}/thermavip")
		file(COPY ${dll} DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip)
	endforeach()
endif()

# Copy Python folder from Plugin directory to thermavip folder
file(COPY ${CMAKE_SOURCE_DIR}/src/Python DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip/)