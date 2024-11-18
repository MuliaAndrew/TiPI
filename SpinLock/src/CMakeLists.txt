include(FetchContent)
FetchContent_Declare(
  googletest
  GIT https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

add_compile_options(-pthread)

set(
  SOURCES
  
)

add_executable(
  text_exec
)

target_link_libraries(
    test_exec
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_exec)