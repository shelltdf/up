cmake_minimum_required(VERSION 3.20)
project(up LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(up
  src/cli/main.cpp
  src/engine/commands/configure.cpp
  src/engine/commands/build.cpp
  src/engine/commands/run.cpp
  src/engine/commands/test.cpp
  src/engine/commands/pack.cpp
  src/engine/commands/project.cpp
  src/engine/commands/commands_common.cpp
  src/engine/backends/cmake/cmake_backend.cpp
  src/engine/backends/ninja/ninja_backend.cpp
  src/engine/backends/test/ctest_backend.cpp
  src/engine/backends/archive/archive_backend.cpp
  src/engine/backends/core/backend_dispatch.cpp
  src/infra/i18n/lang.cpp
  src/infra/platform/path_check.cpp
  src/engine/xml/simple_xml.cpp
  src/infra/platform/paths.cpp
  src/engine/commands/build.hpp
  src/engine/commands/configure.hpp
  src/engine/commands/run.hpp
  src/engine/commands/test.hpp
  src/engine/commands/pack.hpp
  src/engine/commands/project.hpp
  src/engine/commands/commands_common.hpp
  src/engine/backends/core/backend_types.hpp
  src/engine/backends/cmake/cmake_backend.hpp
  src/engine/backends/ninja/ninja_backend.hpp
  src/engine/backends/test/ctest_backend.hpp
  src/engine/backends/archive/archive_backend.hpp
  src/engine/backends/core/backend_dispatch.hpp
  src/infra/platform/paths.hpp
  src/engine/xml/simple_xml.hpp
)

target_include_directories(up PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/commands"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/backends"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/xml"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/infra/platform"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/infra/i18n"
)

if(MSVC)
  target_compile_definitions(up PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up PRIVATE /utf-8)
endif()

# up-gui: thin shell around up.exe — MUST NOT compile, include, or link any file under src/.
# Only paths under src_gui/ are allowed as up-gui sources (enforced below).
set(UP_GUI_CORE_SOURCES
  src_gui/core/gui_core_actions.cpp
)
if(WIN32)
  set(UP_GUI_PLATFORM_SOURCES src_gui/up_gui_win32.cpp)
elseif(APPLE)
  set(UP_GUI_PLATFORM_SOURCES
    src_gui/main/main_cocoa.mm
    src_gui/platform/cocoa_gui.mm
    src_gui/core/gui_unix_shared.cpp
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  set(UP_GUI_PLATFORM_SOURCES
    src_gui/main/main_unix.cpp
    src_gui/platform/gtk_gui.cpp
    src_gui/core/gui_unix_shared.cpp
  )
endif()
set(UP_GUI_ALL_SOURCES ${UP_GUI_PLATFORM_SOURCES} ${UP_GUI_CORE_SOURCES})
foreach(_up_gui_src IN LISTS UP_GUI_ALL_SOURCES)
  if(NOT "${_up_gui_src}" MATCHES "^src_gui/")
    message(FATAL_ERROR "up-gui: every source file must be under src_gui/ (got: ${_up_gui_src})")
  endif()
endforeach()

if(WIN32)
  add_executable(up-gui WIN32 ${UP_GUI_ALL_SOURCES})
  target_link_libraries(up-gui PRIVATE shell32 ole32 comctl32 user32 gdi32 uuid)
elseif(APPLE)
  add_executable(up-gui ${UP_GUI_ALL_SOURCES})
  find_library(COCOA_FRAMEWORK Cocoa)
  target_link_libraries(up-gui PRIVATE ${COCOA_FRAMEWORK})
else()
  add_executable(up-gui ${UP_GUI_ALL_SOURCES})
  target_include_directories(up-gui PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_libraries(up-gui PRIVATE ${GTK3_LIBRARIES})
  target_compile_options(up-gui PRIVATE ${GTK3_CFLAGS_OTHER})
endif()
target_include_directories(up-gui PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src_gui"
  "${CMAKE_CURRENT_SOURCE_DIR}/src_gui/core"
)
if(MSVC)
  target_compile_definitions(up-gui PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up-gui PRIVATE /utf-8)
endif()

# build.py / install.py 仅针对下列目标；安装时使用 COMPONENT 以便 cmake --install --component
set(UP_RUNTIME_COMPONENT up_runtime)
install(TARGETS up up-gui RUNTIME DESTINATION bin COMPONENT ${UP_RUNTIME_COMPONENT})
