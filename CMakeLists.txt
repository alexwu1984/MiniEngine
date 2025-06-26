cmake_minimum_required(VERSION 3.15)
include(${CMAKE_HOME_DIRECTORY}/common.cmake)
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
include_directories(${PROJECT_SOURCE_DIR}/ThirdParty/ffmpeg/include)
include_directories(${PROJECT_SOURCE_DIR}/Core)
include_directories(${PROJECT_SOURCE_DIR}/Render)
include_directories(${PROJECT_SOURCE_DIR}/Engine)

set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS" )

file(GLOB Headers "GLFFViewer/*.h")
file(GLOB Sources "GLFFViewer/*.cpp")

add_executable(GLFFViewer ${Sources} ${Headers})

target_link_libraries(GLFFViewer LINK_PUBLIC Core DirectXTex Render Engine )

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
target_link_libraries(GLFFViewer LINK_PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avcodec.lib 
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avdevice.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avfilter.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avformat.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avutil.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/swresample.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/swscale.lib)
else ()
target_link_libraries(GLFFViewer LINK_PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avcodec.lib 
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avdevice.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avfilter.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avformat.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avutil.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/swresample.lib
											  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/swscale.lib)
endif ()

set_target_properties(GLFFViewer PROPERTIES
	PROJECT_LABEL "GLFFViewer")

file(GLOB SoftwareRender_Headers "SoftwareRender/*.h")
file(GLOB SoftwareRender_Sources "SoftwareRender/*.cpp")

add_executable(SoftwareRender ${SoftwareRender_Sources} ${SoftwareRender_Headers})

target_link_libraries(SoftwareRender LINK_PUBLIC Core DirectXTex Render Engine )

set_target_properties(SoftwareRender PROPERTIES
	PROJECT_LABEL "SoftwareRender")

set_target_properties(
	GLFFViewer
	PROPERTIES
	FOLDER Projects
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel
     DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG}
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                      GROUP_READ GROUP_EXECUTE
                      WORLD_READ WORLD_EXECUTE
          DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                           WORLD_READ WORLD_EXECUTE
     USE_SOURCE_PERMISSIONS
     FOLLOW_SYMLINK_CHAIN)  

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel
     DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE}
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                      GROUP_READ GROUP_EXECUTE
                      WORLD_READ WORLD_EXECUTE
     DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                           GROUP_READ GROUP_EXECUTE
                           WORLD_READ WORLD_EXECUTE
     USE_SOURCE_PERMISSIONS
     FOLLOW_SYMLINK_CHAIN)
	
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel
     DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE_DEBINFO}
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                      GROUP_READ GROUP_EXECUTE
                      WORLD_READ WORLD_EXECUTE
     DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                           GROUP_READ GROUP_EXECUTE
                           WORLD_READ WORLD_EXECUTE
     USE_SOURCE_PERMISSIONS
     FOLLOW_SYMLINK_CHAIN)
