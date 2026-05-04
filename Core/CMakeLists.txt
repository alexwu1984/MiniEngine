cmake_minimum_required(VERSION 3.15)
project(Core)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
file(GLOB core_headers CONFIGURE_DEPENDS core/*.h)
file(GLOB network_headers CONFIGURE_DEPENDS network/*.h)
file(GLOB win_headers CONFIGURE_DEPENDS win/*.h)
file(GLOB common_headers CONFIGURE_DEPENDS common/*.h)
file(GLOB math_headers CONFIGURE_DEPENDS math/*.h)
file(GLOB templates_headers CONFIGURE_DEPENDS templates/*.h)

include_directories(${PROJECT_SOURCE_DIR})

source_group("Header Files\\core" FILES ${core_headers} )
source_group("Header Files\\network" FILES ${network_headers} )
source_group("Header Files\\win" FILES ${win_headers} )
source_group("Header Files\\common" FILES ${common_headers} )
source_group("Header Files\\math" FILES ${math_headers} )
source_group("Header Files\\templates" FILES ${templates_headers} )

file(GLOB Core_sources CONFIGURE_DEPENDS "*.cpp")
add_library(Core ${Core_sources} ${core_headers} ${network_headers} ${win_headers} ${common_headers} ${math_headers} ${templates_headers})

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_compile_options(/wd4201)

#-------------------------------------------------------------------------------------------
# Include dirs
#-------------------------------------------------------------------------------------------
target_include_directories(Core PUBLIC common core network win)

if (WIN32)
  target_link_libraries(Core PUBLIC dbghelp)
endif()



#-----target_link_libraries(EASTL EABase)

