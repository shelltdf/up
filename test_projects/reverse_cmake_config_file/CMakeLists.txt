# 供 gz_reverse_cmake 回归: configure_file 逆为 <config_files> (无 cmake 执行)
cmake_minimum_required(VERSION 3.16)
project(reverse_cf_demo LANGUAGES C)

# 包级: 在首个 add_library 之前
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/gen/version.h"
  @ONLY
)

add_library(demo_lib STATIC lib.c)
# 目标级: 在 add_library 之后
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/gen/config.h"
)
