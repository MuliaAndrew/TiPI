cmake_minimum_required(VERSION 3.28)
project(lsof)

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED On)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED On)

set(SRC ${CMAKE_CURRENT_LIST_DIR}/src)
execute_process(COMMAND echo ${SRC})

include(src/CMakeLists.txt)