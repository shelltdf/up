cmake_minimum_required(VERSION 3.20)
project(gz_reverse_file_read_write_smoke C)
# add_library 在前, 使 file(READ/WRITE) 挂到 last_tname
add_library(helper STATIC helper.c)
add_library(frw STATIC frw.c)
add_dependencies(frw helper)
add_custom_target(gz_ct ALL COMMENT "gz_reverse: add_custom_target smoke" DEPENDS helper)
add_dependencies(frw gz_ct)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/in.txt" FRW_BUF)
string(REGEX MATCHALL "x" FRW_MATCHES "${FRW_BUF}")
string(APPEND FRW_OUT "x")
string(CONCAT FRW_CONCAT "a" "b")
foreach (_i IN LISTS FRW_MATCHES)
  string(APPEND FRW_OUT "${_i}")
endforeach()
while(0)
  string(APPEND FRW_OUT "z")
endwhile()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gen_frw.c" "/* generated */\n")
target_sources(frw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gen_frw.c")
# file(APPEND) 与 add_custom_command(OUTPUT) 登记
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/_gz_append.txt" "m")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_gz_ac.txt"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/_gz_byp.txt"
  COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/_gz_ac.txt"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/in.txt" "${CMAKE_CURRENT_SOURCE_DIR}/frw.c"
)
