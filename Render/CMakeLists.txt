cmake_minimum_required(VERSION 3.15)
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
file(GLOB Src_D3D12 Src/D3D12/*.cpp)
file(GLOB Src_RHIPrivate Src/RHIPrivate/*.cpp)
file(GLOB Src_Imgui Src/Imgui/*.cpp)

source_group("Source Files\\Src\\RHI" FILES ${Src_RHI} )
source_group("Source Files\\Src\\D3D11" FILES ${Src_D3D11} )
source_group("Source Files\\Src\\D3D12" FILES ${Src_D3D12} )
source_group("Source Files\\Src\\RHIPrivate" FILES ${Src_RHIPrivate} )
source_group("Source Files\\Imgui" FILES ${Src_Imgui} )

#-----shader

add_library(Render ${Src_RHI} ${Src_D3D11} ${Src_D3D12} Src/D3D12/D3D12CallStats.cpp Src/D3D12/D3D12UploadWCDiagnostics.cpp Src/D3D12/D3D12RuntimeStatsMonitor.cpp ${Src_RHIPrivate} ${Header_RHI} ${Header_D3D11} ${Header_D3D12} ${Header_RHIPrivate} ${Header_Imgui} ${Src_Imgui})

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DGUARDED_TEXTURE_CREATES)
add_compile_options(/wd4201)

target_link_libraries(Render LINK_PUBLIC DirectXTex)
target_link_libraries(Render LINK_PUBLIC amd_ags)
target_link_libraries(Render LINK_PUBLIC Assimp)
target_link_libraries(Render LINK_PUBLIC Core dinput8.lib dxguid.lib xinput.lib dxgi.lib d3d11.lib D3DCompiler.lib d3d12.lib)

target_include_directories(Render PUBLIC RHI)