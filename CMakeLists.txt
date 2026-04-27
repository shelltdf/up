cmake_minimum_required(VERSION 3.20)
project(up LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# build.py / install.py 仅针对下列目标；安装时使用 COMPONENT 以便 cmake --install --component
set(UP_RUNTIME_COMPONENT up_runtime)
set(UP_DEV_COMPONENT up_dev)

add_subdirectory(src/lib)
add_subdirectory(src/exe)
add_subdirectory(src_gui)
