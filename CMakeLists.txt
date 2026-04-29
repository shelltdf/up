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
add_subdirectory(gz_reverse_cmake)

# Optional smoke: configure + build with redistribution XML (library-only facet of hello_simple_lib).
enable_testing()
if(TARGET gz)
  set(_gz_hello_simple "${CMAKE_SOURCE_DIR}/test_projects/hello_simple_lib")
  add_test(
    NAME gz_redist_emit_configure
    COMMAND "$<TARGET_FILE:gz>" configure --build-dir-name redist_smoke --scan .
    WORKING_DIRECTORY "${_gz_hello_simple}")
  add_test(
    NAME gz_redist_emit_build
    COMMAND "$<TARGET_FILE:gz>" build --build-dir-name redist_smoke
    WORKING_DIRECTORY "${_gz_hello_simple}")
  set_tests_properties(gz_redist_emit_build PROPERTIES DEPENDS gz_redist_emit_configure)
endif()
if(TARGET gz_reverse_cmake)
  add_test(
    NAME gz_reverse_cmake_help
    COMMAND "$<TARGET_FILE:gz_reverse_cmake>" --help
  )
endif()
