cmake_minimum_required(VERSION 3.15)
project(MinHook C)

add_library(MinHook STATIC
  src/buffer.c
  src/hook.c
  src/trampoline.c
  src/hde/hde32.c
  src/hde/hde64.c
)

target_include_directories(MinHook
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Internal headers are included via relative paths inside MinHook sources.
target_include_directories(MinHook
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

if (MSVC)
  # Keep it warning-clean-ish without being strict.
  target_compile_options(MinHook PRIVATE /W3)
endif()

