aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SOURCES)
execute_process(COMMAND echo ${SOURCES})

add_compile_options(-luring)
add_link_options(-luring)

add_executable(
  cp
  ${SOURCES}
)