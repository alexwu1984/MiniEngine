cmake_minimum_required(VERSION 3.15)
include(${CMAKE_HOME_DIRECTORY}/common.cmake)
project(MiniEngineTutorial)


add_compile_definitions(_UNICODE UNICODE)
add_compile_options("/std:c++20")
# Windows parallel build options
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Gm->) # /Gm is deprecated; disabling it helps parallel builds
cmake_host_system_information(RESULT CPU_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP${CPU_NUMBER_OF_LOGICAL_CORES}>)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE_DEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo)

# Keep executable output directories aligned with MSBuild configs (lower-case folder names).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/relwithdebinfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/minsizerel)

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

function(copy_runtime_assets target_name)
	# Copy assets to the config-specific runtime output directory.
	set(runtime_output_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<LOWER_CASE:$<CONFIG>>")
	add_custom_target(${target_name}_RuntimeAssets ALL
		COMMAND ${CMAKE_COMMAND} -E make_directory
				"${runtime_output_dir}"
		# On Windows, CMake's copy_directory fails hard if any file is locked by a running exe.
		# robocopy returns codes <= 7 for "success with some skips"; we treat those as success to keep builds stable.
		COMMAND ${CMAKE_COMMAND} -E echo "Copy runtime assets -> ${runtime_output_dir}"
		COMMAND cmd /c robocopy "${CMAKE_CURRENT_SOURCE_DIR}/GLTFModel" "${runtime_output_dir}/GLTFModel" /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP ^& if %ERRORLEVEL% LEQ 7 (exit 0) else (if %ERRORLEVEL% EQU 32 (exit 0) else (exit /b %ERRORLEVEL%))
		COMMAND powershell -NoProfile -ExecutionPolicy Bypass
			-File "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sync_shaderlib_dx.ps1"
			-SourceDir "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX"
			-DestDir "${runtime_output_dir}/ShaderLibDX"
		COMMENT "Copying runtime assets for ${target_name}")
	add_dependencies(${target_name} ${target_name}_RuntimeAssets)
endfunction()

# Every link: sync HLSL next to the built exe (matches core::process_directory() + /ShaderLibDX/).
function(sync_shaderlib_dx_postbuild target_name)
	add_custom_command(TARGET ${target_name} POST_BUILD
		COMMAND powershell -NoProfile -ExecutionPolicy Bypass
			-File "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sync_shaderlib_dx.ps1"
			-SourceDir "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX"
			-DestDir "$<TARGET_FILE_DIR:${target_name}>/ShaderLibDX"
		VERBATIM
		COMMENT "Sync ShaderLibDX for ${target_name}")
endfunction()

copy_runtime_assets(GLFFViewer)
sync_shaderlib_dx_postbuild(GLFFViewer)

file(GLOB SoftwareRender_Headers "SoftwareRender/*.h")
file(GLOB SoftwareRender_Sources "SoftwareRender/*.cpp")

add_executable(SoftwareRender ${SoftwareRender_Sources} ${SoftwareRender_Headers})

target_link_libraries(SoftwareRender LINK_PUBLIC Core DirectXTex Render Engine )

set_target_properties(SoftwareRender PROPERTIES
	PROJECT_LABEL "SoftwareRender")

copy_runtime_assets(SoftwareRender)
sync_shaderlib_dx_postbuild(SoftwareRender)

file(GLOB DemoRunner_Headers "DemoRunner/*.h")
file(GLOB DemoRunner_Sources "DemoRunner/*.cpp")
file(GLOB DemoRunner_DemoHeaders "DemoRunner/Demos/*.h")
file(GLOB DemoRunner_DemoSources "DemoRunner/Demos/*.cpp")

add_executable(DemoRunner ${DemoRunner_Sources} ${DemoRunner_Headers} ${DemoRunner_DemoSources} ${DemoRunner_DemoHeaders})
target_link_libraries(DemoRunner LINK_PUBLIC Core DirectXTex Render Engine )
target_include_directories(DemoRunner PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(DemoRunner PRIVATE ${PROJECT_SOURCE_DIR}/GLFFViewer)
set_target_properties(DemoRunner PROPERTIES PROJECT_LABEL "DemoRunner")
copy_runtime_assets(DemoRunner)
sync_shaderlib_dx_postbuild(DemoRunner)

set_target_properties(
	GLFFViewer
	PROPERTIES
	FOLDER Projects
)

set_target_properties(
	DemoRunner
	SoftwareRender
	PROPERTIES
	FOLDER Projects
)
