# Runtime memory checking. One command everywhere: `ctest -T memcheck`.
# Only the backend changes, selected by THERMAVIP_SANITIZER.
#
#   defect              | Linux             | Windows (MSVC)
#   --------------------|-------------------|---------------------
#   corruption, UAF     | address           | address
#   leaks               | address (LSan)    | CRT debug heap (*)
#   uninitialised reads | Valgrind, nightly | Dr. Memory, nightly
#   data races          | thread            | none (**)
#   undefined behaviour | undefined         | none (**)
#
# (*)  LSan is not ported to Windows; AddressSanitizer detects no leak there.
#      The CRT debug heap is armed in src/Tests/vip_test_main.h instead.
# (**) MSVC has neither ThreadSanitizer nor UndefinedBehaviorSanitizer.
#
# macOS compiles but is not instrumented.

set(THERMAVIP_SANITIZER "none" CACHE STRING "Runtime memory checking: none, address, thread or undefined")
set_property(CACHE THERMAVIP_SANITIZER PROPERTY STRINGS none address thread undefined)

set(_vip_known_sanitizers none address thread undefined)
if(NOT THERMAVIP_SANITIZER IN_LIST _vip_known_sanitizers)
	message(FATAL_ERROR "THERMAVIP_SANITIZER='${THERMAVIP_SANITIZER}' unknown. Accepted: ${_vip_known_sanitizers}.")
endif()

if(THERMAVIP_SANITIZER STREQUAL "none")
	function(thermavip_apply_sanitizer)
	endfunction()
	return()
endif()

# A replacement allocator makes every checker blind, silently: micro_proxy takes
# over malloc and operator new globally, so nothing is ever reported.
if(WITH_MICRO)
	message(FATAL_ERROR
		"THERMAVIP_SANITIZER=${THERMAVIP_SANITIZER} is incompatible with WITH_MICRO=ON: "
		"micro_proxy replaces malloc and operator new globally, so the checker reports "
		"nothing at all, without any error. Reconfigure with -DWITH_MICRO=OFF.")
endif()

if(MSVC)
	if(NOT THERMAVIP_SANITIZER STREQUAL "address")
		message(FATAL_ERROR
			"MSVC provides neither ThreadSanitizer nor UndefinedBehaviorSanitizer. "
			"Only 'address' exists on Windows; races and undefined behaviour are covered on Linux.")
	endif()

	# AddressSanitizer is incompatible with the /RTC* runtime checks CMake adds to
	# debug builds, and with incremental linking.
	foreach(_cfg "" _DEBUG _RELWITHDEBINFO _MINSIZEREL _RELEASE)
		foreach(_lang C CXX)
			if(DEFINED CMAKE_${_lang}_FLAGS${_cfg})
				string(REGEX REPLACE "/RTC[1csu]+" "" _vip_stripped "${CMAKE_${_lang}_FLAGS${_cfg}}")
				set(CMAKE_${_lang}_FLAGS${_cfg} "${_vip_stripped}" CACHE STRING "" FORCE)
			endif()
		endforeach()
	endforeach()
	foreach(_kind EXE SHARED MODULE)
		set(CMAKE_${_kind}_LINKER_FLAGS "${CMAKE_${_kind}_LINKER_FLAGS} /INCREMENTAL:NO" CACHE STRING "" FORCE)
	endforeach()

	# AddressSanitizer needs a consistent runtime library; the project pins none.
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
	endif()

	# Measured: a deliberate 4096-byte leak is reported without AddressSanitizer and
	# goes unnoticed with it, since it replaces the allocator. The two Windows tools
	# are mutually exclusive in one binary, hence two build configurations.
	# vip_test_main.h reads this to leave the CRT debug heap disarmed.
	set(_vip_san_compile /fsanitize=address)
	set(_vip_san_link "")
	set(_vip_memcheck_type "AddressSanitizer")
	add_compile_definitions(THERMAVIP_WITH_ASAN=1)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	# -fno-omit-frame-pointer: call stacks are unusable without it.
	if(THERMAVIP_SANITIZER STREQUAL "address")
		set(_vip_san_compile -fsanitize=address -fno-omit-frame-pointer -g)
		set(_vip_san_link -fsanitize=address)
		set(_vip_memcheck_type "AddressSanitizer")
	elseif(THERMAVIP_SANITIZER STREQUAL "thread")
		set(_vip_san_compile -fsanitize=thread -fno-omit-frame-pointer -g)
		set(_vip_san_link -fsanitize=thread)
		set(_vip_memcheck_type "ThreadSanitizer")
	else()
		set(_vip_san_compile -fsanitize=undefined -fno-omit-frame-pointer -g)
		set(_vip_san_link -fsanitize=undefined)
		set(_vip_memcheck_type "UndefinedBehaviorSanitizer")
	endif()

else()
	message(FATAL_ERROR "THERMAVIP_SANITIZER=${THERMAVIP_SANITIZER}: unsupported compiler (${CMAKE_CXX_COMPILER_ID}).")
endif()

# Must be set before include(CTest), which writes it to DartConfiguration.tcl.
set(MEMORYCHECK_TYPE "${_vip_memcheck_type}" CACHE STRING "" FORCE)

# allocator_may_return_null: a test asks for an allocation far larger than the
# address space on purpose, to check that the array handle survives a refusal.
# Without this the sanitizer aborts on the request instead of returning null,
# which is the behaviour under test.
set(MEMORYCHECK_SANITIZER_OPTIONS "allocator_may_return_null=1" CACHE STRING "" FORCE)

# Without suppressions the job is permanently red on Qt and driver noise, and
# gets ignored within a week.
set(THERMAVIP_SANITIZER_SUPPRESSIONS "${CMAKE_CURRENT_LIST_DIR}/sanitizer-suppressions.txt"
	CACHE FILEPATH "AddressSanitizer/LeakSanitizer suppression file")

message(STATUS "Memory checking enabled: THERMAVIP_SANITIZER=${THERMAVIP_SANITIZER} (CTest backend: ${_vip_memcheck_type})")

# Flags travel through the cache: the function is called from another scope, and
# a CMake function resolves variables in the caller's scope.
set(THERMAVIP_SANITIZER_COMPILE_OPTIONS "${_vip_san_compile}" CACHE INTERNAL "")
set(THERMAVIP_SANITIZER_LINK_OPTIONS "${_vip_san_link}" CACHE INTERNAL "")

function(thermavip_apply_sanitizer _target)
	if(NOT TARGET ${_target})
		return()
	endif()
	target_compile_options(${_target} PRIVATE ${THERMAVIP_SANITIZER_COMPILE_OPTIONS})
	if(THERMAVIP_SANITIZER_LINK_OPTIONS)
		target_link_options(${_target} PRIVATE ${THERMAVIP_SANITIZER_LINK_OPTIONS})
	endif()
endfunction()
