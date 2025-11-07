

find_package(QT NAMES Qt5 Qt6 REQUIRED )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport Svg Concurrent)
message(STATUS "Qt found for ${TARGET_PROJECT}, version ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")

set(QT_PREFIX Qt${QT_VERSION_MAJOR})
set(CMAKE_AUTOMOC ON)

if(${QT_VERSION_MAJOR} LESS 6)
set(QT_LIBS Qt5::Core
    Qt5::Gui
    Qt5::Network
	Qt5::Widgets
	Qt5::OpenGL
	Qt5::PrintSupport
    Qt5::Xml
	Qt5::Sql
	Qt5::Svg
	)
else()
set(QT_LIBS Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
	Qt::Sql
	Qt::Svg
	)
endif()



target_link_libraries(${TARGET_PROJECT} PRIVATE ${QT_LIBS})



if(${QT_VERSION_MAJOR} LESS 6)
else()
find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
target_link_libraries(${TARGET_PROJECT} PRIVATE Qt6::Core5Compat)
target_link_libraries(${TARGET_PROJECT} PRIVATE Qt6::OpenGLWidgets)
endif()

if(NOT ${QT_VERSION_MAJOR} LESS 6)
	if(WITH_WEBENGINE)
		if((NOT ${TARGET_PROJECT} STREQUAL "VipLogging") AND (NOT ${TARGET_PROJECT} STREQUAL "VipDataType") AND (NOT ${TARGET_PROJECT} STREQUAL "VipCore") AND (NOT ${TARGET_PROJECT} STREQUAL "VipPlotting"))
			# Add WebEnging if possible
			#if(${QT_VERSION_MAJOR} LESS 6)
			#	set(WEB_ENGINE Qt${QT_VERSION_MAJOR}WebEngine)
			#else()
			#	set(WEB_ENGINE Qt${QT_VERSION_MAJOR}WebEngineCore)
			#endif()
			#find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS ${WEB_ENGINE} )
			#find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Qt${QT_VERSION_MAJOR}WebEngineWidgets )

			find_package(Qt6 QUIET COMPONENTS WebEngineCore)
			find_package(Qt6 QUIET COMPONENTS WebEngineWidgets )
			set(WEB_ENGINE Qt6WebEngineCore)
			
			if (${WEB_ENGINE}_FOUND)
				if (${WEB_ENGINE}_VERSION VERSION_LESS 5.15.0)
					#message(STATUS "Too old web engine version!")
				else()
					#message(STATUS "Using web engine for ${TARGET_PROJECT}")
					find_package(Qt${QT_VERSION_MAJOR} COMPONENTS WebEngineWidgets REQUIRED)
					if(${QT_VERSION_MAJOR} LESS 6)
					target_link_libraries(${TARGET_PROJECT} PRIVATE
					Qt5::WebEngine
					Qt5::WebEngineWidgets
					)
					else()
					target_link_libraries(${TARGET_PROJECT} PRIVATE
					Qt::WebEngineCore
					Qt::WebEngineWidgets
					)
					endif()
					target_compile_definitions(${TARGET_PROJECT} PRIVATE __VIP_USE_WEB_ENGINE)
				endif()
			else()
				message(STATUS "WebEngine not found!")
			endif()
		endif()
	endif()
endif()


# Add HDF5 libraries if required
set(HAS_HDF5 OFF CACHE INTERNAL "")
if(WITH_HDF5)
	find_package (HDF5 COMPONENTS C GLOBAL)
	if(NOT HDF5_FOUND)
		include(FetchContent)
		set(HDF5_ENABLE_SZIP_SUPPORT OFF)
		set(HDF5_ENABLE_Z_LIB_SUPPORT OFF)
		set(BUILD_TESTING OFF)
		set(HDF5_BUILD_TOOLS OFF)
		set(HDF5_BUILD_EXAMPLES OFF)
		set(H5EX_BUILD_EXAMPLES OFF)
		FetchContent_Declare(
			hdf5
			GIT_REPOSITORY https://github.com/HDFGroup/hdf5
			GIT_TAG hdf5-1.14.6
			OVERRIDE_FIND_PACKAGE
		)
		find_package (hdf5 REQUIRED COMPONENTS C GLOBAL)
		if (TARGET hdf5_hl-shared)
			get_target_property(HDF5_INCLUDE_DIRS hdf5_hl-shared INCLUDE_DIRECTORIES)
			get_target_property(HDF5_LIBRARIES hdf5_hl-shared LINK_LIBRARIES)
		endif()
	endif()
	
	target_include_directories(${TARGET_PROJECT} PRIVATE ${HDF5_INCLUDE_DIRS} )
	target_link_libraries(${TARGET_PROJECT} PRIVATE ${HDF5_LIBRARIES})
	target_compile_definitions(${TARGET_PROJECT} PUBLIC -DVIP_WITH_HDF5)
	set(HAS_HDF5 ON CACHE INTERNAL "")
endif()

# Add VTK libraries if required
if(WITH_VTK)
	find_package(VTK REQUIRED)
	#TEST: remove Qt based modules from VTK (possible conflict between different Qt versions)
	list(FILTER VTK_LIBRARIES EXCLUDE REGEX "^.*Qt.*$")

	target_include_directories(${TARGET_PROJECT} PRIVATE ${VTK_INCLUDE_DIRS})
	target_link_libraries(${TARGET_PROJECT} PRIVATE ${VTK_LIBRARIES})
	target_compile_definitions(${TARGET_PROJECT} PUBLIC -DVIP_WITH_VTK)
	
	#if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		# for msvc, avoid using release VTK with debug STL
		#target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_ITERATOR_DEBUG_LEVEL=0)
	#endif()
endif()


#external code added if exists
if(EXISTS my_compiler_flags.cmake ) 
      include(my_compiler_flags.cmake)
else()

	if (CMAKE_BUILD_TYPE STREQUAL "Release" )
		# Release build, all compilers
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DNDEBUG)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DQT_NO_DEBUG)
	endif()

	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(${TARGET_PROJECT} PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()
	
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# for gcc
		target_compile_options(${TARGET_PROJECT} PRIVATE -march=native -fopenmp -fPIC -mno-bmi2 -mno-fma -mno-avx -Wno-maybe-uninitialized)
		if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
			target_compile_options(${TARGET_PROJECT} PRIVATE -std=gnu++17)
		else()
			target_compile_options(${TARGET_PROJECT} PRIVATE -std=c++11)
			target_compile_definitions(${TARGET_PROJECT} PRIVATE -DQ_COMPILER_ATOMICS -DQ_COMPILER_CONSTEXPR)
		endif()
		target_link_options(${TARGET_PROJECT} PRIVATE -lgomp )
		
		if (CMAKE_BUILD_TYPE STREQUAL "Release" )
			# gcc release
			target_compile_options(${TARGET_PROJECT} PRIVATE -O3 -ftree-vectorize -march=native -fopenmp -fPIC -mno-bmi2 -mno-fma -mno-avx -Wno-maybe-uninitialized)
			if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
				target_compile_options(${TARGET_PROJECT} PRIVATE -std=gnu++17)
			else()
				target_compile_options(${TARGET_PROJECT} PRIVATE -std=c++11)
			endif()
		else()
			target_compile_options(${TARGET_PROJECT} PRIVATE -g)
		endif()

	endif()
	
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		# for msvc
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DNOMINMAX)
		
		# remove warnings generated by Qt
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
		
		target_compile_options(${TARGET_PROJECT} PRIVATE /MP)
		
		# For macro VIP_FOR_EACH_GENERIC, we need a compliant preprocessor
		target_compile_options(${TARGET_PROJECT} PUBLIC /Zc:preprocessor)
		
		# Weird bug with msvc 2022...
		target_compile_options(${TARGET_PROJECT} PUBLIC /wd"4828")
		
		# For BIG source files
		target_compile_options(${TARGET_PROJECT} PUBLIC /bigobj)
		
		#string(REPLACE "/utf-8" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		#string(REPLACE "-utf-8" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		#string(REPLACE "/utf-8" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG}")
		#string(REPLACE "-utf-8" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG}")
		#set_target_properties(${TARGET_PROJECT} PROPERTIES COMPILE_FLAGS "${_compile_flags}")
		#target_compile_options(${TARGET_PROJECT} PUBLIC /execution-charset:utf-8 /source-charset:utf-8)
		
		target_link_libraries(${TARGET_PROJECT} PRIVATE opengl32)
	endif()
	
	# add openmp for all
	find_package(OpenMP)
	if (OPENMP_FOUND)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
		set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
	endif()
	
	
	if(NO_WARNINGS)
		string(FIND "${THERMAVIP_LIBRARIES}" "${TARGET_PROJECT}" IS_SDK)
		if(IS_SDK MATCHES "-1")
		else()
			if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
				target_compile_options(${TARGET_PROJECT} PRIVATE  /WX /W3 )
			elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
				target_compile_options(${TARGET_PROJECT} PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
			else()
				target_compile_options(${TARGET_PROJECT} PRIVATE -Werror -Wall)
			endif()
		endif()
	endif()
	
	if (WIN32)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DNOMINMAX)
		#target_compile_definitions(${TARGET_PROJECT} PUBLIC -DWIN64)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DDISABLE_WINRT_DEPRECATION)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_WINDOWS)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -DUNICODE)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_USE_MATH_DEFINES)
		target_compile_definitions(${TARGET_PROJECT} PUBLIC -D_WIN32)
	endif()
	
	
endif() #end exists( my_compiler_flags.cmake ) 
