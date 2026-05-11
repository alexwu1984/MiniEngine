cmake_minimum_required(VERSION 3.15)
include(${CMAKE_HOME_DIRECTORY}/common.cmake)
project(MiniEngine)


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

# Recommend: configure from repo root — GenerateProjectFiles.bat, or:
#   cmake -S . -B build -A x64
# Optional generator (when CMake default is wrong): append e.g. -G "Visual Studio 17 2022"
# VS builds are multi-config; Debug/Release is chosen at build time (--config).

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

file(GLOB Headers CONFIGURE_DEPENDS "GLFFViewer/*.h")
file(GLOB Sources CONFIGURE_DEPENDS "GLFFViewer/*.cpp")

add_executable(GLFFViewer ${Sources} ${Headers})

set(MINIENGINE_APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/MiniEngine.rc")
target_sources(GLFFViewer PRIVATE ${MINIENGINE_APP_ICON_RC})

target_link_libraries(GLFFViewer LINK_PUBLIC Core DirectXTex Render Engine
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avcodec.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avdevice.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avfilter.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avformat.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/avutil.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/swresample.lib>
	$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64d/swscale.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avcodec.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avdevice.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avfilter.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avformat.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/avutil.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/swresample.lib>
	$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ffmpeg/lib/x64/swscale.lib>)

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
		COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sync_shaderlib_dx.py"
			--source "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX"
			--dest "${runtime_output_dir}/ShaderLibDX"
		COMMENT "Copying runtime assets for ${target_name}")
	add_dependencies(${target_name} ${target_name}_RuntimeAssets)
endfunction()

# Every link: sync HLSL next to the built exe (matches core::process_directory() + /ShaderLibDX/).
function(sync_shaderlib_dx_postbuild target_name)
	add_custom_command(TARGET ${target_name} POST_BUILD
		COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sync_shaderlib_dx.py"
			--source "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX"
			--dest "$<TARGET_FILE_DIR:${target_name}>/ShaderLibDX"
		VERBATIM
		COMMENT "Sync ShaderLibDX for ${target_name}")
endfunction()

copy_runtime_assets(GLFFViewer)
sync_shaderlib_dx_postbuild(GLFFViewer)

file(GLOB SoftwareRender_Headers CONFIGURE_DEPENDS "SoftwareRender/*.h")
file(GLOB SoftwareRender_Sources CONFIGURE_DEPENDS "SoftwareRender/*.cpp")

add_executable(SoftwareRender ${SoftwareRender_Sources} ${SoftwareRender_Headers})

target_sources(SoftwareRender PRIVATE ${MINIENGINE_APP_ICON_RC})

target_link_libraries(SoftwareRender LINK_PUBLIC Core DirectXTex Render Engine )

set_target_properties(SoftwareRender PROPERTIES
	PROJECT_LABEL "SoftwareRender")

copy_runtime_assets(SoftwareRender)
sync_shaderlib_dx_postbuild(SoftwareRender)

file(GLOB DemoRunner_Headers CONFIGURE_DEPENDS "DemoRunner/*.h")
file(GLOB DemoRunner_Sources CONFIGURE_DEPENDS "DemoRunner/*.cpp")
file(GLOB DemoRunner_DemoHeaders CONFIGURE_DEPENDS "DemoRunner/Demos/*.h")
file(GLOB DemoRunner_DemoSources CONFIGURE_DEPENDS "DemoRunner/Demos/*.cpp")

add_executable(DemoRunner ${DemoRunner_Sources} ${DemoRunner_Headers} ${DemoRunner_DemoSources} ${DemoRunner_DemoHeaders})

target_sources(DemoRunner PRIVATE ${MINIENGINE_APP_ICON_RC})

target_link_libraries(DemoRunner LINK_PUBLIC Core DirectXTex Render Engine )
target_include_directories(DemoRunner PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(DemoRunner PRIVATE ${PROJECT_SOURCE_DIR}/GLFFViewer)
set_target_properties(DemoRunner PROPERTIES PROJECT_LABEL "DemoRunner")
copy_runtime_assets(DemoRunner)
sync_shaderlib_dx_postbuild(DemoRunner)

# Offline shader bytecode (D3DCompileFromFile via CompileShaderBlob). Writes ShaderLibDX/Built/*.cso; runtime loads before JIT.
add_executable(CompileShaderBlob Tools/CompileShaderBlob.cpp)
target_link_libraries(CompileShaderBlob PRIVATE d3dcompiler.lib)
if(MSVC)
	target_link_options(CompileShaderBlob PRIVATE /SUBSYSTEM:CONSOLE)
endif()
set_target_properties(CompileShaderBlob PROPERTIES FOLDER Tools PROJECT_LABEL "CompileShaderBlob")

# Offline HLSL: Debug precompile -> tier 2 (SKIP_OPTIMIZATION); non-Debug -> tier 1 (OPT_LEVEL0). Optional full opt / forced LEVEL0.
option(MINIENGINE_FULL_OPT_SHADER_PRECOMPILE "Pass --full-opt to offline HLSL precompile (full FXC optimization)" OFF)
option(MINIENGINE_LEVEL0_SHADER_PRECOMPILE "Pass --hlsl-level0 to offline HLSL precompile (FXC OPT_LEVEL0, all configs)" OFF)
if(MINIENGINE_FULL_OPT_SHADER_PRECOMPILE AND MINIENGINE_LEVEL0_SHADER_PRECOMPILE)
	message(FATAL_ERROR "Choose at most one of MINIENGINE_FULL_OPT_SHADER_PRECOMPILE and MINIENGINE_LEVEL0_SHADER_PRECOMPILE")
endif()

find_program(MINIENGINE_PYTHON_EXECUTABLE NAMES python python3 py)
if(MINIENGINE_PYTHON_EXECUTABLE)
	# $<IF:...> needs CMake 3.15+ (project already requires 3.15). Non-Debug configs share the Release tier sidecar.
	set(MINIENGINE_PRECOMPILE_CONFIG_GENEX $<IF:$<CONFIG:Debug>,Debug,Release>)
	if(MINIENGINE_FULL_OPT_SHADER_PRECOMPILE)
		set(MINIENGINE_PRECOMPILE_SHADER_EXTRA
			--full-opt
			--precompile-config
			${MINIENGINE_PRECOMPILE_CONFIG_GENEX})
	elseif(MINIENGINE_LEVEL0_SHADER_PRECOMPILE)
		set(MINIENGINE_PRECOMPILE_SHADER_EXTRA
			--hlsl-level0
			--precompile-config
			${MINIENGINE_PRECOMPILE_CONFIG_GENEX})
	else()
		set(MINIENGINE_PRECOMPILE_SHADER_EXTRA
			--precompile-config
			${MINIENGINE_PRECOMPILE_CONFIG_GENEX}
			--compile-tier
			$<IF:$<CONFIG:Debug>,2,1>)
	endif()
	add_custom_target(PrecompileMiniEngineShaders
		COMMAND "${MINIENGINE_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/gen_precompile_manifest.py"
			--output "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX/precompile_manifest.json"
		COMMAND "${MINIENGINE_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/build_precompiled_shaders.py"
			--compiler "$<TARGET_FILE:CompileShaderBlob>"
			--manifest "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX/precompile_manifest.json"
			--shader-root "${CMAKE_CURRENT_SOURCE_DIR}/Render/ShaderLibDX"
			--built-dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<LOWER_CASE:$<CONFIG>>/ShaderLibDX/Built"
			${MINIENGINE_PRECOMPILE_SHADER_EXTRA}
		DEPENDS CompileShaderBlob
			"${CMAKE_CURRENT_SOURCE_DIR}/Tools/gen_precompile_manifest.py"
			"${CMAKE_CURRENT_SOURCE_DIR}/Tools/build_precompiled_shaders.py"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Incremental precompile HLSL -> $<LOWER_CASE:$<CONFIG>>/ShaderLibDX/Built (under CMAKE_RUNTIME_OUTPUT_DIRECTORY)")
	set_target_properties(PrecompileMiniEngineShaders PROPERTIES FOLDER Tools)
	# Run before robocopy ShaderLibDX; Built/ is written under the config output dir (sync excludes source Built/).
	# Offline bytecode is sized for the main GLTF viewer; DemoRunner/SoftwareRender JIT extras as needed.
	add_dependencies(GLFFViewer_RuntimeAssets PrecompileMiniEngineShaders)
else()
	message(STATUS "MiniEngine: Python not found (python/python3/py); offline shader precompile disabled (JIT only).")
endif()

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
