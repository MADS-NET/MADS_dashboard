include(FetchContent)
FetchContent_Declare(doctest
  SOURCE_SUBDIR _no_cmake
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(dash_tests test_main.cpp test_version.cpp)
target_include_directories(dash_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_link_libraries(dash_tests PRIVATE mads_dash_core dash_hdf5)
set_target_properties(dash_tests PROPERTIES BUILD_RPATH "${MADS_PREFIX}/lib")
add_test(NAME dash_tests COMMAND dash_tests)
