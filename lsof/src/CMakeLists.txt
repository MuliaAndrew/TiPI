set(
  COMPILE_FLAGS
  -lboost_program_options
)

file(GLOB_RECURSE SOURCES ${SRC} "src/*.cpp")
execute_process(COMMAND echo ${SOURCES})

add_link_options(-lboost_program_options)

add_executable(
  my_lsof
  ${SOURCES}
)