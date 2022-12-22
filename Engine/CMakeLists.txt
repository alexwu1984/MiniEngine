cmake_minimum_required(VERSION 3.6)
project(Engine)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB App_Header App/*.h)
file(GLOB Engine_Header Engine/*.h)
file(GLOB Engine_thread_Header Engine/Thread/*.h)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../Core)
include_directories(${PROJECT_SOURCE_DIR}/../Render)

source_group("Header Files\\App" FILES ${App_Header} )
source_group("Header Files\\Engine" FILES ${Engine_Header} )
source_group("Header Files\\Engine\\Thread" FILES ${Engine_thread_Header} )

file(GLOB App_Source Src/App/*.cpp)
file(GLOB Engine_Source Src/Engine/*.cpp)
file(GLOB Engine_Source Src/Engine/Thread/*.cpp)

source_group("Source Files\\Src\\App" FILES ${App_Source} )
source_group("Source Files\\Src\\Engine" FILES ${Engine_Source} )
source_group("Source Files\\Src\\Engine\\Thread" FILES ${Engine_thread_Source} )

add_library(Engine ${App_Source} ${Engine_Source} ${App_Header} ${Engine_Header} ${Engine_thread_Source} ${Engine_thread_Header})

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)

target_link_libraries(Engine LINK_PUBLIC Render)
target_include_directories(Render PUBLIC App Engine)