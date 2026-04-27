cmake_minimum_required(VERSION 3.20)
project(up LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(UP_ENABLE_REVERSE "Build the up reverse subcommand and CMake/qmake/… importers" OFF)
# When ON (default): strip package.xml `<cmake/>` parsing, configure ExternalProject wiring, and `up spec` documentation
# for that tag (macro **UP_DISABLE_PACKAGE_XML_CMAKE**). Set OFF to restore full package-native-CMake support.
option(UP_DISABLE_PACKAGE_XML_CMAKE "Ignore package.xml <cmake/> at compile time (strip related logic)" ON)

# build.py / install.py 仅针对下列目标；安装时使用 COMPONENT 以便 cmake --install --component
set(UP_RUNTIME_COMPONENT up_runtime)
set(UP_DEV_COMPONENT up_dev)

add_subdirectory(src/lib)
add_subdirectory(src/exe)
add_subdirectory(src_gui)
