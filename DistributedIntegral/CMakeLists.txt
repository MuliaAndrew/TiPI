cmake_minimum_required(VERSION 3.30)
project(DistributedIntegral)

set(CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED On)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED On)
add_compile_options("-O0" "-g" "-Wall" "-Wextra")

add_subdirectory(src)

add_subdirectory(app/client)
add_subdirectory(app/server)
