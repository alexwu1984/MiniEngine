cmake_minimum_required(VERSION 3.6)
project(Render)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB RHI_Header RHI/*.h)
file(GLOB D3D11_Header D3D11/*.h)
file(GLOB D3D12_Header D3D12/*.h)
file(GLOB D3D11_Private_Header RHIPrivate/*.h)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty/DirectXTex)
include_directories(${PROJECT_SOURCE_DIR}/../Core)

source_group("Header Files\\RHI" FILES ${RHI_Header} )
source_group("Header Files\\D3D11" FILES ${D3D11_Header} )
source_group("Header Files\\D3D12" FILES ${D3D12_Header} )
source_group("Header Files\\RHIPrivate" FILES ${D3D11_Private_Header} )

file(GLOB Render_RHI_Src Src/RHI/*.cpp)
file(GLOB Render_D3D11_Src Src/D3D11/*.cpp)


source_group("Source Files\\Src\\RHI" FILES ${Render_RHI_Src} )
source_group("Source Files\\Src\\D3D11" FILES ${Render_D3D11_Src} )

add_library(Render ${Render_RHI_Src} ${Render_D3D11_Src} ${RHI_Header} ${D3D11_Header} ${D3D12_Header} ${D3D11_Private_Header} )



#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-DGUARDED_TEXTURE_CREATES)

target_link_libraries(Render LINK_PUBLIC DirectXTex)
target_link_libraries(Render LINK_PUBLIC amd_ags)

target_link_libraries(Render LINK_PUBLIC Core dinput8.lib dxguid.lib xinput.lib dxgi.lib d3d11.lib)

target_include_directories(Render PUBLIC RHI)