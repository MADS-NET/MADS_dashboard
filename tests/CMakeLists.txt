include(FetchContent)
FetchContent_Declare(doctest
  SOURCE_SUBDIR _no_cmake
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
add_executable(dash_tests ${TEST_SOURCES})
target_include_directories(dash_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_link_libraries(dash_tests PRIVATE mads_dash_ui dash_hdf5)
set_target_properties(dash_tests PROPERTIES BUILD_RPATH "${MADS_PREFIX}/lib")
add_test(NAME dash_tests COMMAND dash_tests)
