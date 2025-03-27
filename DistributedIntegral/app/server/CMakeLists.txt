add_executable(server main.c)

target_include_directories(server PRIVATE ../src)
target_link_libraries(server Logger)