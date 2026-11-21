# Stand-in for the MADS build consuming the dashboard with FetchContent:
# shared libraries and tests on, its own `glfw` 3.4 and non-docking `imgui`
# under the names MADS's src/gui uses, Mads::Mads and MadsMainDeps targets.
# Checks that the dashboard builds there and changes none of these.
#   cmake -S tests/consumer -B /tmp/consumer -G Ninja && cmake --build /tmp/consumer
cmake_minimum_required(VERSION 3.28)
project(fake_mads LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS ON CACHE BOOL "MADS builds MadsCore shared")
set(BUILD_TESTING ON CACHE BOOL "")

execute_process(COMMAND mads -p OUTPUT_VARIABLE MADS_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
list(APPEND CMAKE_PREFIX_PATH "${MADS_PREFIX}")
find_package(Mads REQUIRED) # stands in for the in-tree Mads::Mads alias
add_library(MadsMainDeps INTERFACE)

include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git
                     GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git
                     GIT_TAG v1.92.6 GIT_SHALLOW TRUE SOURCE_SUBDIR _no_cmake)
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE) # as MADS does
FetchContent_MakeAvailable(glfw imgui)
set(_parent_imgui ${imgui_SOURCE_DIR})

set(MADS_DASH_INSTALL_COMPONENT MadsGui)
FetchContent_Declare(mads_dashboard SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
FetchContent_MakeAvailable(mads_dashboard)

foreach(check IN ITEMS "BUILD_SHARED_LIBS:ON" "BUILD_TESTING:ON")
  string(REPLACE ":" ";" kv ${check})
  list(GET kv 0 var)
  list(GET kv 1 want)
  get_property(have CACHE ${var} PROPERTY VALUE)
  if(NOT have STREQUAL want)
    message(FATAL_ERROR "dashboard changed the parent's ${var}: ${have}")
  endif()
endforeach()
if(NOT imgui_SOURCE_DIR STREQUAL _parent_imgui)
  message(FATAL_ERROR "dashboard replaced the parent's imgui")
endif()
if(TARGET dash_tests OR TARGET testsource)
  message(FATAL_ERROR "tests/testsource must be off when not top-level")
endif()
message(STATUS "consumer checks passed")
