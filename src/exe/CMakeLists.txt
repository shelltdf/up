add_executable(up main.cpp cli_paths.cpp cli_dispatch.cpp)
target_link_libraries(up PRIVATE up-lib)
set_target_properties(up PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(MSVC)
  target_compile_definitions(up PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(up PRIVATE /utf-8)
endif()

install(TARGETS up RUNTIME DESTINATION bin COMPONENT ${UP_RUNTIME_COMPONENT})
