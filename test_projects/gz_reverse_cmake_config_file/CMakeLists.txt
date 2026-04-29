cmake_minimum_required(VERSION 3.20)
# 最小样例: 包级 configure_file( 出现在任何 add_library 之前 ) + 目标级( 在 add_library 之后 )
project(gz_rev_cfgf_smoke C)

# 先 package.xml 的 <config_files> (无 prior add_* 于本文件)
configure_file(templates/pkg_meta.h.in gen/pkg_meta.h)

add_library(smalllib STATIC small.c)
# 同 Listfile 内、上一 add_* 为 smalllib → 记到该 target
configure_file(config.h.in out/config.h)
# 关键字式
configure_file(INPUT "templates/version.h.in" OUTPUT "gen/version.h")
