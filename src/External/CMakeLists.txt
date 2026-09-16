# Extensions are listed here, one add_subdirectory per extension.
#
# This used to glob every subdirectory and add it, so a directory dropped in was
# configured and built without being declared anywhere and without appearing in
# any review. Adding an extension is a change to this file.
#
file(GLOB V_GLOB LIST_DIRECTORIES true "*")
foreach(item ${V_GLOB})
	if(IS_DIRECTORY ${item})
		add_subdirectory(${item})
	endif()
endforeach()