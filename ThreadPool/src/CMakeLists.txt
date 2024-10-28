include(FetchContent)
FetchContent_Declare(
  googletest
<<<<<<< HEAD
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
=======
  GIT https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
>>>>>>> master
)

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

set(
    SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.hpp
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.cpp
)

add_executable(
    test_exec
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool_tests.cpp
    ${SOURCES}
)

set_property(TARGET test_exec PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(
    test_exec
    GTest::gtest_main
)

add_executable(
    main
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/data_samples.cpp
    ${CMAKE_CURRENT_LIST_DIR}/data_samples.hpp
    ${SOURCES}
)

include(GoogleTest)
gtest_discover_tests(test_exec)