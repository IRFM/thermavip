# The Python side is checked by reading and exercising the modules rather than
# by running the application: the interpreter is not always built in.
find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
	# Structural, so it needs nothing but the interpreter.
	add_test(NAME CheckThermavipProtocol
		 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_thermavip_protocol.py)

	# Starts the interpreter process and waits for it to end. It skips itself
	# when numpy is missing, so it is registered unconditionally.
	add_test(NAME CheckPyProcess
		 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_pyprocess.py)

	# These two run the processings, so they need numpy and scipy. They skip
	# themselves rather than fail the suite when those are missing.
	execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy, scipy" RESULT_VARIABLE _vip_have_scipy OUTPUT_QUIET ERROR_QUIET)
	if(_vip_have_scipy EQUAL 0)
		add_test(NAME CheckEstimTau
			 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_estim_tau.py)
		add_test(NAME CheckPythonProcessings
			 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_processings.py)
	endif()
endif()
