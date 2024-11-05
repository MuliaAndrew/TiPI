include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

set(
  SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.hpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
)

add_executable(
  main
  ${SOURCES}
)

# gtest_discover_tests(test_exec)