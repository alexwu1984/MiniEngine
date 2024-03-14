cmake_minimum_required(VERSION 3.6)
project(Render)
include(${CMAKE_HOME_DIRECTORY}/common.cmake)
#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB Header_RHI RHI/*.h)
file(GLOB Header_D3D11 D3D11/*.h)
file(GLOB Header_D3D12 D3D12/*.h)
file(GLOB Header_RHIPrivate RHIPrivate/*.h)
file(GLOB Header_Imgui Imgui/*.h)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty/DirectXTex)
include_directories(${PROJECT_SOURCE_DIR}/../Core)

source_group("Header Files\\RHI" FILES ${Header_RHI} )
source_group("Header Files\\D3D11" FILES ${Header_D3D11} )
source_group("Header Files\\D3D12" FILES ${Header_D3D12} )
source_group("Header Files\\RHIPrivate" FILES ${Header_RHIPrivate} )
source_group("Header Files\\Imgui" FILES ${Header_Imgui} )

file(GLOB Src_RHI Src/RHI/*.cpp)
file(GLOB Src_D3D11 Src/D3D11/*.cpp)
file(GLOB Src_RHIPrivate Src/RHIPrivate/*.cpp)
file(GLOB Src_Imgui Src/Imgui/*.cpp)

source_group("Source Files\\Src\\RHI" FILES ${Src_RHI} )
source_group("Source Files\\Src\\D3D11" FILES ${Src_D3D11} )
source_group("Source Files\\Src\\RHIPrivate" FILES ${Src_RHIPrivate} )
source_group("Source Files\\Imgui" FILES ${Src_Imgui} )

#-----shader

set(Shaders_GLTF_src
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/GLTFPbrPass-IO.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/GLTFVertexFactory.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/GLTFPbrPass-VS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/PBRMaterial.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/FurMaterial.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/PerFrameStruct.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/Skinning.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/ShaderUtils.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/EnvironmentShaders.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/CubeBackground.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/PostProcess.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/TAA.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/TAASharpenerCS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/Blend.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/ShadowPass-PS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader/ShadowPass-VS.hlsl
)

add_library(Render ${Src_RHI} ${Src_D3D11} ${Src_RHIPrivate} ${Header_RHI} ${Header_D3D11} ${Header_D3D12} ${Header_RHIPrivate} ${Header_Imgui} ${Src_Imgui})

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DGUARDED_TEXTURE_CREATES)
add_compile_options(/wd4201)

target_link_libraries(Render LINK_PUBLIC DirectXTex)
target_link_libraries(Render LINK_PUBLIC amd_ags)

target_link_libraries(Render LINK_PUBLIC Core dinput8.lib dxguid.lib xinput.lib dxgi.lib d3d11.lib D3DCompiler.lib)

target_include_directories(Render PUBLIC RHI)

copyTargetCommand("${Shaders_GLTF_src}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG}/ShaderLibDX copied_dx_shaders_gltf_debug)
copyTargetCommand("${Shaders_GLTF_src}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE}/ShaderLibDX copied_dx_shaders_gltf_release)
copyTargetCommand("${Shaders_GLTF_src}" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE_DEBINFO}/ShaderLibDX copied_dx_shaders_gltf_release_debinfo)

add_dependencies(Render copied_dx_shaders_gltf_debug copied_dx_shaders_gltf_release copied_dx_shaders_gltf_release_debinfo)


set_source_files_properties(${Shaders_GLTF_src} PROPERTIES VS_TOOL_OVERRIDE "Text")
set_source_files_properties(${Shaders_PostProc_src} PROPERTIES VS_TOOL_OVERRIDE "Text")