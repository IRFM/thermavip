# Minimal third-party project built against the package thermavip exports.
#
# It does not test the code. It tests that find_package(thermavip) still works:
# that the install rules produce a usable tree, that thermavipConfig.cmake finds
# its own export file, that the variables it publishes are set, and that the
# libraries link. Nothing in src/Tests covers any of this, because the tests
# link the in-tree targets directly and never see the installed package.
#
# It lives here rather than under src/Tests on purpose: it must be configured as
# a separate project, after `cmake --install`, against CMAKE_PREFIX_PATH. Adding
# it to the main build would have it link the in-tree targets, which is exactly
# the situation it exists to distinguish from.

cmake_minimum_required(VERSION 3.16)
project(sdk_consumer VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(thermavip REQUIRED)

# The variables the installed thermavipConfig.cmake actually publishes. Checked
# before the target is defined so a removed variable is a configure error with a
# name in it, not a link error a hundred lines later.
#
# THERMAVIP_LIB_DIR and THERMAVIP_BIN_DIR are NOT in this list: the config file
# leaves them commented out, because the exported targets carry their own
# location. Asserting on them would fail against a perfectly correct install.
#
# `if(NOT DEFINED ...)` and a quoted comparison, not `if(NOT ${var})`: two of
# these variables hold lists, and an unquoted expansion turns into several
# arguments, which if() rejects outright.
foreach(var IN ITEMS THERMAVIP_INCLUDE_DIRS THERMAVIP_LIBRARIES THERMAVIP_APPLICATION_DIR THERMAVIP_PLUGIN_DIR)
	if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
		message(FATAL_ERROR "The thermavip package no longer exposes ${var}")
	endif()
	message(STATUS "${var} = ${${var}}")
endforeach()

add_executable(sdk_consumer main.cpp)

# No target_link_directories and no explicit Qt list: the exported targets carry
# their include directories and link Qt PUBLIC (see src/compiler_flags.cmake).
# Naming Qt again here would hide a package that stopped propagating it.
target_include_directories(sdk_consumer PRIVATE ${THERMAVIP_INCLUDE_DIRS})
target_link_libraries(sdk_consumer PRIVATE ${THERMAVIP_LIBRARIES})
