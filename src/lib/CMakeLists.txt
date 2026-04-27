set(UP_LIB_SOURCES
  engine/commands/configure.cpp
  engine/commands/build.cpp
  engine/commands/run.cpp
  engine/commands/test.cpp
  engine/commands/pack.cpp
  engine/commands/spec.cpp
  engine/commands/list.cpp
  engine/commands/commands_common.cpp
  engine/backends/cmake/cmake_backend.cpp
  engine/backends/ninja/ninja_backend.cpp
  engine/backends/test/ctest_backend.cpp
  engine/backends/archive/archive_backend.cpp
  engine/backends/core/backend_dispatch.cpp
  infra/i18n/lang.cpp
  infra/platform/path_check.cpp
  engine/xml/simple_xml.cpp
  engine/xml/var_subst.cpp
  engine/dom/dom_node_visitor.cpp
  engine/dom/dom_document.cpp
  engine/dom/script_execution.cpp
  engine/dom/dom_types.hpp
  engine/dom/dom_vars.hpp
  engine/dom/dom_nodes.hpp
  engine/dom/dom_node_visitor.hpp
  engine/dom/dom_document.hpp
  engine/dom/script_execution.hpp
  infra/platform/paths.cpp
  engine/commands/build.hpp
  engine/commands/configure.hpp
  engine/commands/run.hpp
  engine/commands/test.hpp
  engine/commands/pack.hpp
  engine/commands/spec.hpp
  engine/commands/list.hpp
  engine/commands/commands_common.hpp
  engine/backends/core/backend_types.hpp
  engine/backends/cmake/cmake_backend.hpp
  engine/backends/ninja/ninja_backend.hpp
  engine/backends/test/ctest_backend.hpp
  engine/backends/archive/archive_backend.hpp
  engine/backends/core/backend_dispatch.hpp
  infra/platform/paths.hpp
  engine/xml/simple_xml.hpp
  engine/xml/var_subst.hpp
  engine/dom/dom_model.hpp
)
if(UP_ENABLE_PROJECT)
  list(APPEND UP_LIB_SOURCES
    engine/commands/project.cpp
    engine/commands/project.hpp
    engine/project/project_import.cpp
    engine/project/project_import_common.cpp
    engine/project/import_cmake.cpp
    engine/project/import_autotools.cpp
    engine/project/import_qmake.cpp
    engine/project/import_meson.cpp
    engine/project/import_sources.cpp
    engine/project/project_probe.cpp
    engine/project/project_import.hpp
    engine/project/project_import_internal.hpp
    engine/project/project_import_common.hpp
  )
endif()

add_library(up-lib STATIC ${UP_LIB_SOURCES})
set_target_properties(up-lib PROPERTIES
  OUTPUT_NAME up
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(up-lib PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/commands"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/backends"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/xml"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/project"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/dom"
  "${CMAKE_CURRENT_SOURCE_DIR}/infra/platform"
  "${CMAKE_CURRENT_SOURCE_DIR}/infra/i18n"
)

if(MSVC)
  target_compile_definitions(up-lib PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up-lib PRIVATE /utf-8)
endif()
if(UP_ENABLE_PROJECT)
  target_compile_definitions(up-lib PUBLIC UP_ENABLE_PROJECT=1)
else()
  target_compile_definitions(up-lib PUBLIC UP_ENABLE_PROJECT=0)
endif()
if(UP_DISABLE_PACKAGE_XML_CMAKE)
  target_compile_definitions(up-lib PUBLIC UP_DISABLE_PACKAGE_XML_CMAKE=1)
else()
  target_compile_definitions(up-lib PUBLIC UP_DISABLE_PACKAGE_XML_CMAKE=0)
endif()

install(TARGETS up-lib ARCHIVE DESTINATION lib COMPONENT ${UP_DEV_COMPONENT})
