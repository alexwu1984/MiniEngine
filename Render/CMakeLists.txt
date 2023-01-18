cmake_minimum_required(VERSION 3.6)
project(Render)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB Header_RHI RHI/*.h)
file(GLOB Header_D3D11 D3D11/*.h)
file(GLOB Header_D3D12 D3D12/*.h)
file(GLOB Header_RHIPrivate RHIPrivate/*.h)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty/DirectXTex)
include_directories(${PROJECT_SOURCE_DIR}/../Core)

source_group("Header Files\\RHI" FILES ${Header_RHI} )
source_group("Header Files\\D3D11" FILES ${Header_D3D11} )
source_group("Header Files\\D3D12" FILES ${Header_D3D12} )
source_group("Header Files\\RHIPrivate" FILES ${Header_RHIPrivate} )

file(GLOB Src_RHI Src/RHI/*.cpp)
file(GLOB Src_D3D11 Src/D3D11/*.cpp)
file(GLOB Src_RHIPrivate Src/RHIPrivate/*.cpp)

source_group("Source Files\\Src\\RHI" FILES ${Src_RHI} )
source_group("Source Files\\Src\\D3D11" FILES ${Src_D3D11} )
source_group("Source Files\\Src\\RHIPrivate" FILES ${Src_RHIPrivate} )

add_library(Render ${Src_RHI} ${Src_D3D11} ${Src_RHIPrivate} ${Header_RHI} ${Header_D3D11} ${Header_D3D12} ${Header_RHIPrivate} )



#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DGUARDED_TEXTURE_CREATES)

target_link_libraries(Render LINK_PUBLIC DirectXTex)
target_link_libraries(Render LINK_PUBLIC amd_ags)

target_link_libraries(Render LINK_PUBLIC Core dinput8.lib dxguid.lib xinput.lib dxgi.lib d3d11.lib D3DCompiler.lib)

target_include_directories(Render PUBLIC RHI)