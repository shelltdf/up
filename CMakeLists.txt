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
  src/infra/platform/paths.hpp
  src/engine/xml/simple_xml.hpp
)

target_include_directories(up PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/commands"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/xml"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/infra/platform"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/infra/i18n"
)

if(MSVC)
  target_compile_definitions(up PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up PRIVATE /utf-8)
endif()

set(UP_GUI_CORE_SOURCES
  src_gui/core/gui_core_actions.cpp
)

if(WIN32)
  add_executable(up-gui WIN32
    src_gui/up_gui_win32.cpp
    ${UP_GUI_CORE_SOURCES}
  )
  target_link_libraries(up-gui PRIVATE shell32 ole32 comctl32 user32 gdi32 uuid)
elseif(APPLE)
  add_executable(up-gui
    src_gui/main/main_cocoa.mm
    src_gui/platform/cocoa_gui.mm
    ${UP_GUI_CORE_SOURCES}
  )
  find_library(COCOA_FRAMEWORK Cocoa)
  target_link_libraries(up-gui PRIVATE ${COCOA_FRAMEWORK})
else()
  add_executable(up-gui
    src_gui/main/main_x11.cpp
    src_gui/platform/xlib_gui.cpp
    ${UP_GUI_CORE_SOURCES}
  )
  find_package(X11 REQUIRED)
  target_link_libraries(up-gui PRIVATE X11::X11)
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
