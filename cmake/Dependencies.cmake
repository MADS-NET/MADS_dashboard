# Third-party dependencies, all fetched at pinned tags and linked statically.
# nlohmann/json, toml++ and cppzmq come from the MADS install (Mads::Mads)
# so that the dashboard shares the exact same headers as libMadsCore.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# GLFW ########################################################################
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL
)

# Dear ImGui (docking), ImPlot, ImPlot3D #########################################
FetchContent_Declare(imgui
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9b-docking
  GIT_SHALLOW TRUE
)
FetchContent_Declare(implot
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG v1.1
  GIT_SHALLOW TRUE
)
FetchContent_Declare(implot3d
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/brenocq/implot3d.git
  GIT_TAG v0.5
  GIT_SHALLOW TRUE
)

# HDF5 (static, C library only) + HighFive (header-only C++ wrapper) #########
set(HDF5_EXTERNALLY_CONFIGURED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(HDF5_BUILD_CPP_LIB OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_FORTRAN OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_JAVA OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_HL_LIB OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_PARALLEL OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_THREADSAFE OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_ZLIB_SUPPORT OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_SZIP_SUPPORT OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_PLUGIN_SUPPORT OFF CACHE BOOL "" FORCE)
set(HDF5_INSTALL_NO_DEVELOPMENT ON CACHE BOOL "" FORCE)
set(HDF5_EXTERNALLY_CONFIGURED 1)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(hdf5
  GIT_REPOSITORY https://github.com/HDFGroup/hdf5.git
  GIT_TAG 2.2.0
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(highfive
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/highfive-devs/highfive.git
  GIT_TAG v3.3.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw hdf5 imgui implot implot3d highfive)

find_package(OpenGL REQUIRED)

add_library(dash_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  ${implot3d_SOURCE_DIR}/implot3d.cpp
  ${implot3d_SOURCE_DIR}/implot3d_items.cpp
  ${implot3d_SOURCE_DIR}/implot3d_meshes.cpp
)
target_include_directories(dash_imgui SYSTEM PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${implot_SOURCE_DIR}
  ${implot3d_SOURCE_DIR}
)
target_sources(dash_imgui PRIVATE ${CMAKE_SOURCE_DIR}/src/ui/im_assert.cpp)
target_include_directories(dash_imgui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(dash_imgui PUBLIC IMGUI_USER_CONFIG="ui/imconfig_dash.h")
target_link_libraries(dash_imgui PUBLIC glfw OpenGL::GL)
if(APPLE)
  target_compile_definitions(dash_imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()

# HDF5 static target name differs across versions: pick what exists.
if(TARGET hdf5-static)
  set(_hdf5_target hdf5-static)
elseif(TARGET hdf5_hdf5-static)
  set(_hdf5_target hdf5_hdf5-static)
else()
  message(FATAL_ERROR "HDF5 static target not found")
endif()
add_library(dash_hdf5 INTERFACE)
target_link_libraries(dash_hdf5 INTERFACE ${_hdf5_target})
target_include_directories(dash_hdf5 SYSTEM INTERFACE ${highfive_SOURCE_DIR}/include)
# HighFive v3 is header-only; tell it HDF5 is provided by us.
target_compile_definitions(dash_hdf5 INTERFACE H5_BUILT_AS_STATIC_LIB)
