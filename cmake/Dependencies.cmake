# Third-party dependencies, all fetched at pinned tags and linked statically.
# nlohmann/json, toml++ and cppzmq come from MADS (Mads::Mads, MadsMainDeps)
# so that the dashboard shares the exact same headers as libMadsCore.
#
# Inside the MADS build (FetchContent), nothing here may leak into MADS:
#  - FetchContent names are prefixed (mads_dash_*): MADS fetches its own imgui
#    (without docking) as `imgui`, and the first declaration of a name wins;
#  - generic switches (BUILD_SHARED_LIBS, BUILD_TESTING, ...) are normal
#    variables, scoped to this directory, never cache entries;
#  - GLFW is shared: MADS's GUI already provides the same `glfw` target.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)
# let option() in the dependencies honour the normal variables set here
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# GLFW ########################################################################
if(NOT TARGET glfw)
  set(GLFW_LIBRARY_TYPE STATIC)
  set(GLFW_INSTALL OFF)
  set(GLFW_BUILD_DOCS OFF)
  set(GLFW_BUILD_TESTS OFF)
  set(GLFW_BUILD_EXAMPLES OFF)
  if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_X11 ON)
    set(GLFW_BUILD_WAYLAND OFF)
  endif()
  FetchContent_Declare(glfw # same name and tag as the MADS GUI
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(glfw)
endif()

# Dear ImGui (docking), ImPlot, ImPlot3D #########################################
FetchContent_Declare(mads_dash_imgui
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9b-docking
  GIT_SHALLOW TRUE
)
FetchContent_Declare(mads_dash_implot
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG v1.1
  GIT_SHALLOW TRUE
)
# Pinned to an upstream commit: release 0.1.0 passes the default file name to
# macOS as `default location`, so save dialogs fail silently (error -1700).
FetchContent_Declare(mads_dash_pfd
  SOURCE_SUBDIR _no_cmake
  GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
  GIT_TAG c12ea8c9a727f5320a2b4570aee863bbede2a204
)
FetchContent_Declare(mads_dash_implot3d
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/brenocq/implot3d.git
  GIT_TAG v0.5
  GIT_SHALLOW TRUE
)

# HDF5 (static, C library only) + HighFive (header-only C++ wrapper) #########
set(BUILD_SHARED_LIBS OFF) # this directory only (MadsCore stays shared)
set(BUILD_STATIC_LIBS ON)
set(BUILD_TESTING OFF)
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
FetchContent_Declare(mads_dash_hdf5
  GIT_REPOSITORY https://github.com/HDFGroup/hdf5.git
  GIT_TAG 2.2.0
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(mads_dash_highfive
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/highfive-devs/highfive.git
  GIT_TAG v3.3.0
  GIT_SHALLOW TRUE
)

# Lua 5.4 (user scripts): plain C sources, no CMake of its own ##############
FetchContent_Declare(mads_dash_lua
  SOURCE_SUBDIR _no_cmake # populate only, never add_subdirectory
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG v5.4.7
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(mads_dash_hdf5 mads_dash_imgui mads_dash_implot
  mads_dash_implot3d mads_dash_highfive mads_dash_pfd mads_dash_lua)
set(imgui_SOURCE_DIR ${mads_dash_imgui_SOURCE_DIR})
set(implot_SOURCE_DIR ${mads_dash_implot_SOURCE_DIR})
set(implot3d_SOURCE_DIR ${mads_dash_implot3d_SOURCE_DIR})
set(pfd_SOURCE_DIR ${mads_dash_pfd_SOURCE_DIR})
set(highfive_SOURCE_DIR ${mads_dash_highfive_SOURCE_DIR})

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
  ${pfd_SOURCE_DIR}
)
target_sources(dash_imgui PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/im_assert.cpp)
target_include_directories(dash_imgui PUBLIC ${PROJECT_SOURCE_DIR}/src)
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

# Lua: the library only (no lua.c/luac.c interpreters, no onelua.c amalgam,
# no ltests.c test harness of the git repository).
file(GLOB _lua_sources ${mads_dash_lua_SOURCE_DIR}/*.c)
list(FILTER _lua_sources EXCLUDE REGEX "/(lua|luac|onelua|ltests)\\.c$")
add_library(dash_lua STATIC ${_lua_sources})
set_target_properties(dash_lua PROPERTIES LINKER_LANGUAGE C)
target_include_directories(dash_lua SYSTEM PUBLIC ${mads_dash_lua_SOURCE_DIR})
if(UNIX) # POSIX only, no dlopen: scripts never load C modules
  target_compile_definitions(dash_lua PRIVATE LUA_USE_POSIX)
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(dash_lua PUBLIC m)
endif()
