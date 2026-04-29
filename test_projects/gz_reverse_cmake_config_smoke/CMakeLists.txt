# 最小 CMake 样例：供 gz_reverse_cmake 回归 configure_file -> <config_files>（不跑 cmake 配置）
cmake_minimum_required(VERSION 3.20)
project(gz_reverse_config_smoke C)

# 在任意 add_* 之前：应出现在逆向结果的 package.xml 包级 <config_files>
configure_file(top.in top.out)

add_library(lib_a STATIC lib.c)

# 在 add_library 之后：应归到目标 lib_a 的 target.xml
configure_file(config.h.in config.h)
