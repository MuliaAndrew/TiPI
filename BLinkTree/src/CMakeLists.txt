cmake_minimum_required(VERSION 3.14)
project(BLinkTree)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

add_compile_options(-pthread)

set(SOURCES
    node.c
    nodeIO.cpp
    tree.cpp
)

# add_executable(
#     main
#     main.cpp
#     ${SOURCES}
# )

add_executable(
    test_exec
    tests.cpp
    ${SOURCES}
)

target_link_libraries(
    test_exec
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_exec)