cmake_minimum_required(VERSION 3.20)
project(gz_reverse_cmake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gz_reverse_cmake
  src/main.cpp
  src/cmake_parse.cpp
  src/cmake_interpret.cpp
)

if(MSVC)
  target_compile_options(gz_reverse_cmake PRIVATE /utf-8)
endif()

# 与根 CMakeLists.txt 中 GZ_RUNTIME_COMPONENT 一致；单独 -S gz_reverse_cmake 配置时给默认
if(NOT DEFINED GZ_RUNTIME_COMPONENT)
  set(GZ_RUNTIME_COMPONENT gz_runtime)
endif()
install(TARGETS gz_reverse_cmake
  RUNTIME DESTINATION bin
  COMPONENT ${GZ_RUNTIME_COMPONENT})
