set(
  COMPILE_FLAGS
  -lboost_program_options
)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "*.cpp")

add_link_options(-lboost_program_options)

add_executable(
  my_lsof
  ${SOURCES}
)