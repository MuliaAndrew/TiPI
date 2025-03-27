add_executable(client main.c)

target_include_directories(client PRIVATE ../src)
target_link_libraries(client Logger clientlib)