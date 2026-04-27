cmake_minimum_required(VERSION 3.20)
project(GroundZero LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# build.py / install.py 仅针对下列目标；安装时使用 COMPONENT 以便 cmake --install --component
set(GZ_RUNTIME_COMPONENT gz_runtime)
set(GZ_DEV_COMPONENT gz_dev)

add_subdirectory(GroundZero/lib)
add_subdirectory(GroundZero/exe)
add_subdirectory(GroundZeroGUI)
