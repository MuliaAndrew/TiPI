include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

set(
  SRC
  ${CMAKE_CURRENT_LIST_DIR}
)
set(
  SOURCES
  ${SRC}/TASLock.hpp
  ${SRC}/TTASLock.hpp
  ${SRC}/TicketLock.hpp
)

add_executable(
  test_exec
  ${SRC}/tests.cpp
  ${SOURCES}
)

add_executable(
  bench
  ${SRC}/main.cpp
  ${SOURCES}
)

target_link_libraries(
    test_exec
    GTest::gtest_main
)

gtest_discover_tests(test_exec)