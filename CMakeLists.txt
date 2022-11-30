cmake_minimum_required(VERSION 3.10)

project(MiniEngineTutorial)

add_compile_definitions(_UNICODE UNICODE)
add_compile_options("/std:c++20")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE_DEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo)

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Core)
add_subdirectory(ThirdParty)
add_subdirectory(Render)
add_subdirectory(Engine)

include_directories(${PROJECT_SOURCE_DIR}/ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/Core)
include_directories(${PROJECT_SOURCE_DIR}/Render)
include_directories(${PROJECT_SOURCE_DIR}/Engine)

set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS" )

add_executable(Tutorial01 Tutorial01/Tutorial01.cpp)
target_link_libraries(Tutorial01 LINK_PUBLIC Core DirectXTex Render Engine)
set_target_properties(Tutorial01 PROPERTIES
	PROJECT_LABEL "Tutorial01 - Empty Project")

set_target_properties(
	Tutorial01
	PROPERTIES
		FOLDER Tutorials
)