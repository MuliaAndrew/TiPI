cmake_minimum_required(VERSION 3.28)
project(SpinLock)

set(C_STANDARD_REQUIRED On)
set(C_STANDARD 11)

set(CXX_STANDARD_REQUIRED On)
set(CXX_STANDARD 17)

include(src/CMakeLists.txt)

make_directory(res)