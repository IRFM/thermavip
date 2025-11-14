
#https://www.reddit.com/r/cpp_questions/comments/xnu9e0/cmake_how_to_glob_or_add_all_subdirectories/
file(GLOB V_GLOB LIST_DIRECTORIES true "*")
foreach(item ${V_GLOB})
	if(IS_DIRECTORY ${item})
		add_subdirectory(${item})
	endif()
endforeach()