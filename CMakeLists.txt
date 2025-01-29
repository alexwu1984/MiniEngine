cmake_minimum_required(VERSION 3.10)
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

set_target_properties(
	GLFFViewer
	PROPERTIES
	FOLDER Projects
)

set(GLTFModel_GLBS
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/floor.glb
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model1.glb
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model2.glb
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model2.json
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model3.glb
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model3.json
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model4.glb
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/Model4.json
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/noise.jpg
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/venice_sunset_1k.hdr
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/delta_2_2k.hdr
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/aerith_wallpaper_3840x2160.jpg
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/tifa_wallpaper_3840x2160.png
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/test.png
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/HDR/de_balie_2k.hdr
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/HDR/modern_buildings_night_2k.hdr
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/HDR/museum_of_ethnography_2k.hdr
	${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel/HDR/spruit_sunrise_2k.hdr
)

copyTargetCommand("${GLTFModel_GLBS}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG}/GLTFModel copied_glb_debug)
copyTargetCommand("${GLTFModel_GLBS}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE}/GLTFModel copied_glb_release)
copyTargetCommand("${GLTFModel_GLBS}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE_DEBINFO}/GLTFModel copied_glb_debinfo)

add_dependencies(GLFFViewer copied_glb_debug copied_glb_release copied_glb_debinfo)