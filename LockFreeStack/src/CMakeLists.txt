include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

add_link_options(-latomic)
add_compile_options(-g)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

set(
  SRC
  ${CMAKE_CURRENT_LIST_DIR}
)

set(
  SOURCES
  ${SRC}/lock_free_stack.hpp
)

add_executable(
  test_exec
  ${SRC}/tests.cpp
  ${SOURCES}
)

target_link_libraries(
    test_exec
    GTest::gtest_main
)