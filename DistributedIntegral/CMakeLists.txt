project(DistributedIntegral)
cmake_minimum_required(3.14)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED On)

target_include_directories(src)
add_subdirectory(src)

add_subdirectory(app/client)
add_subdirectory(app/server)
