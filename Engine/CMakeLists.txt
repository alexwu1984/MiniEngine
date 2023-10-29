cmake_minimum_required(VERSION 3.6)
project(Engine)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../Core)
include_directories(${PROJECT_SOURCE_DIR}/../Render)

file(GLOB App_Header App/*.h)
file(GLOB Engine_Header Engine/*.h)
file(GLOB Engine_Thread_Header Engine/Thread/*.h)
file(GLOB Engine_Scene_Header Engine/Scene/*.h)
file(GLOB Engine_GltfModel_Header Engine/GltfModel/*.h)
file(GLOB Engine_Render_Header Engine/Render/*.h)

source_group("Header Files\\App" FILES ${App_Header} )
source_group("Header Files\\Engine" FILES ${Engine_Header} )
source_group("Header Files\\Engine\\Thread" FILES ${Engine_Thread_Header} )
source_group("Header Files\\Engine\\Scene" FILES ${Engine_Scene_Header} )
source_group("Header Files\\Engine\\GltfModel" FILES ${Engine_GltfModel_Header} )
source_group("Header Files\\Engine\\Render" FILES ${Engine_Render_Header} )

file(GLOB App_Source Src/App/*.cpp)
file(GLOB Engine_Source Src/Engine/*.cpp)
file(GLOB Engine_Thread_Source Src/Engine/Thread/*.cpp)
file(GLOB Engine_Scene_Source Src/Engine/Scene/*.cpp)
file(GLOB Engine_GltfModel_Source Src/Engine/GltfModel/*.cpp)
file(GLOB Engine_Render_Source Src/Engine/Render/*.cpp)

source_group("Source Files\\Src\\App" FILES ${App_Source} )
source_group("Source Files\\Src\\Engine" FILES ${Engine_Source} )
source_group("Source Files\\Src\\Engine\\Thread" FILES ${Engine_Thread_Source} )
source_group("Source Files\\Src\\Engine\\Scene" FILES ${Engine_Scene_Source} )
source_group("Source Files\\Src\\Engine\\GltfModel" FILES ${Engine_GltfModel_Source} )
source_group("Source Files\\Src\\Engine\\Render" FILES ${Engine_Render_Source} )

add_library(Engine ${App_Source} ${Engine_Source} ${App_Header} ${Engine_Header} ${Engine_Thread_Source} ${Engine_Thread_Header} 
                   ${Engine_Scene_Header} ${Engine_Scene_Source} ${Engine_GltfModel_Header} ${Engine_GltfModel_Source}
                   ${Engine_Render_Header} ${Engine_Render_Source} )

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_compile_options(/wd4201)

target_link_libraries(Engine LINK_PUBLIC Render tinygltf)
target_include_directories(Render PUBLIC App Engine)