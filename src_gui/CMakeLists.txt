# up-gui: thin shell around up.exe — MUST NOT compile, include, or link any file under src/.
# Only paths under src_gui/ are allowed as up-gui sources (enforced below).
set(UP_GUI_CORE_SOURCES
  core/gui_core_actions.cpp
  core/gui_persist.cpp
  core/gui_shell_actions.cpp
)
if(WIN32)
  set(UP_GUI_PLATFORM_SOURCES
    main/main_win32.cpp
    platform/win32_encoding.cpp
    platform/win32_fsutil.cpp
    platform/win32_gui.cpp
    platform/win32_modal_file_dialog_center.cpp
    platform/win32_paths.cpp
    platform/win32_text_util.cpp
    platform/win32_window_placement.cpp
    resources/up_gui.rc
  )
elseif(APPLE)
  set(UP_GUI_PLATFORM_SOURCES
    main/main_cocoa.mm
    platform/cocoa_gui.mm
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  set(UP_GUI_PLATFORM_SOURCES
    main/main_unix.cpp
    platform/gtk_gui.cpp
  )
endif()
set(UP_GUI_ALL_SOURCES ${UP_GUI_PLATFORM_SOURCES} ${UP_GUI_CORE_SOURCES})
foreach(_up_gui_src IN LISTS UP_GUI_ALL_SOURCES)
  if(_up_gui_src MATCHES "^\\.\\./")
    message(FATAL_ERROR "up-gui: every source file must stay under src_gui/ (got: ${_up_gui_src})")
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
set_target_properties(up-gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(up-gui PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/core"
)
if(MSVC)
  target_compile_definitions(up-gui PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up-gui PRIVATE /utf-8)
endif()
if(UP_ENABLE_REVERSE)
  target_compile_definitions(up-gui PRIVATE UP_ENABLE_REVERSE=1)
else()
  target_compile_definitions(up-gui PRIVATE UP_ENABLE_REVERSE=0)
endif()

# GTK/Cocoa load up_gui.png from the executable directory; keep in sync with install rule.
add_custom_command(
  TARGET up-gui
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/resources/up_gui.png"
          "$<TARGET_FILE_DIR:up-gui>/up_gui.png"
  COMMENT "Copy up_gui.png next to up-gui for dock/window icon")

install(TARGETS up-gui RUNTIME DESTINATION bin COMPONENT ${UP_RUNTIME_COMPONENT})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/up_gui.png" DESTINATION bin COMPONENT ${UP_RUNTIME_COMPONENT})
