# gz-gui: thin shell around gz.exe — MUST NOT compile, include, or link any file under GroundZero/.
# Only paths under GroundZeroGUI/ are allowed as gz-gui sources (enforced below).
# Platform UI sources live only under platform/win32/, platform/gtk/, platform/cocoa/ (see platform/README.md).
set(GZ_GUI_CORE_SOURCES
  core/gui_core_actions.cpp
  core/gui_persist.cpp
  core/gui_shell_actions.cpp
)
if(WIN32)
  set(GZ_GUI_PLATFORM_SOURCES
    main/main_win32.cpp
    platform/win32/win32_encoding.cpp
    platform/win32/win32_fsutil.cpp
    platform/win32/win32_gui.cpp
    platform/win32/win32_modal_file_dialog_center.cpp
    platform/win32/win32_paths.cpp
    platform/win32/win32_run_process.cpp
    platform/win32/win32_text_util.cpp
    platform/win32/win32_window_placement.cpp
    resources/gz_gui.rc
  )
elseif(APPLE)
  set(GZ_GUI_PLATFORM_SOURCES
    main/main_cocoa.mm
    platform/cocoa/cocoa_gui.mm
    platform/cocoa/cocoa_gui_bridge.mm
    platform/cocoa/cocoa_gui_handlers.mm
    platform/cocoa/cocoa_gui_log.mm
    platform/cocoa/cocoa_gui_state.mm
    platform/cocoa/cocoa_gui_string.mm
    platform/cocoa/cocoa_gui_window.mm
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  set(GZ_GUI_PLATFORM_SOURCES
    main/main_unix.cpp
    platform/gtk/gtk_gui.cpp
    platform/gtk/gtk_gui_handlers.cpp
    platform/gtk/gtk_gui_log.cpp
    platform/gtk/gtk_gui_state.cpp
    platform/gtk/gtk_gui_window.cpp
  )
endif()
set(GZ_GUI_ALL_SOURCES ${GZ_GUI_PLATFORM_SOURCES} ${GZ_GUI_CORE_SOURCES})
foreach(_gz_gui_src IN LISTS GZ_GUI_ALL_SOURCES)
  if(_gz_gui_src MATCHES "^\\.\\./")
    message(FATAL_ERROR "gz-gui: every source file must stay under GroundZeroGUI/ (got: ${_gz_gui_src})")
  endif()
endforeach()

if(WIN32)
  add_executable(gz-gui WIN32 ${GZ_GUI_ALL_SOURCES})
  target_link_libraries(gz-gui PRIVATE shell32 ole32 comctl32 user32 gdi32 uuid)
elseif(APPLE)
  add_executable(gz-gui ${GZ_GUI_ALL_SOURCES})
  find_library(COCOA_FRAMEWORK Cocoa)
  target_link_libraries(gz-gui PRIVATE ${COCOA_FRAMEWORK})
else()
  add_executable(gz-gui ${GZ_GUI_ALL_SOURCES})
  target_include_directories(gz-gui PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_libraries(gz-gui PRIVATE ${GTK3_LIBRARIES})
  target_compile_options(gz-gui PRIVATE ${GTK3_CFLAGS_OTHER})
endif()
set_target_properties(gz-gui PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  OUTPUT_NAME gz-gui
)
target_include_directories(gz-gui PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/core"
)
if(MSVC)
  target_compile_definitions(gz-gui PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(gz-gui PRIVATE /utf-8)
endif()

# GTK/Cocoa load gz_gui.png from the executable directory; keep in sync with install rule.
add_custom_command(
  TARGET gz-gui
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/resources/gz_gui.png"
          "$<TARGET_FILE_DIR:gz-gui>/gz_gui.png"
  COMMENT "Copy gz_gui.png next to gz-gui for dock/window icon")

install(TARGETS gz-gui RUNTIME DESTINATION bin COMPONENT ${GZ_RUNTIME_COMPONENT})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/gz_gui.png" DESTINATION bin COMPONENT ${GZ_RUNTIME_COMPONENT})
