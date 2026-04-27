add_executable(gz main.cpp cli_paths.cpp cli_dispatch.cpp)
target_link_libraries(gz PRIVATE gz-lib)
set_target_properties(gz PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  OUTPUT_NAME gz
)

if(MSVC)
  target_compile_definitions(gz PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(gz PRIVATE /utf-8)
endif()

install(TARGETS gz RUNTIME DESTINATION bin COMPONENT ${GZ_RUNTIME_COMPONENT})
