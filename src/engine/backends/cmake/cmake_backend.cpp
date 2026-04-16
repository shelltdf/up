#include "cmake/cmake_backend.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace up {

std::string build_cmake_build_command(const BuildBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << ctx.src_dir.string() << "\" -B \"" << ctx.bin_dir.string()
      << "\" -DCMAKE_INSTALL_PREFIX=\"" << ctx.install_dir.string() << "\"";
  if (!ctx.cmake_generator.empty()) {
    cmd << " -G \"" << ctx.cmake_generator << "\"";
  }
  for (const auto& kv : ctx.opts) {
    cmd << " -D" << kv.first << "=\"" << kv.second << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  cmd << " && cmake --build \"" << ctx.bin_dir.string() << "\"";
  if (ctx.multi_config) {
    cmd << " --config " << ctx.config_name;
  }
  cmd << " --target install";
  return cmd.str();
}

std::string build_cmake_configure_command(const ConfigureBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << ctx.source_dir.string() << "\" -B \"" << ctx.out_dir.string() << "\"";
  if (!ctx.cmake_generator.empty()) {
    cmd << " -G \"" << ctx.cmake_generator << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  return cmd.str();
}

int write_cmake_lists(const ConfigureGraphModel& model) {
  std::ostringstream cm;
  int command_idx = 0;
  cm << "cmake_minimum_required(VERSION 3.20)\n";
  cm << "project(" << model.package_name << " LANGUAGES CXX)\n";
  cm << "set(CMAKE_CXX_STANDARD 17)\n";
  cm << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";

  for (const auto& t : model.targets) {
    if (!(t.type == "static_library" || t.type == "shared_library"))
      continue;
    const char* kind = (t.type == "shared_library") ? "SHARED" : "STATIC";
    cm << "add_library(" << t.name << " " << kind;
    for (const auto& s : t.source_paths)
      cm << " \"" << s << "\"";
    cm << ")\n";
    if (!t.include_dirs.empty()) {
      cm << "target_include_directories(" << t.name << " PUBLIC";
      for (const auto& inc : t.include_dirs)
        cm << " \"" << inc << "\"";
      cm << ")\n";
    }
    for (const auto& s : t.source_rules) {
      if (!s.preprocess_command.empty()) {
        const std::string stamp = "pre_stamp_" + std::to_string(command_idx++);
        cm << "add_custom_command(OUTPUT " << stamp << " COMMAND " << s.preprocess_command
           << " COMMAND ${CMAKE_COMMAND} -E touch " << stamp << " DEPENDS \"" << s.path << "\")\n";
        cm << "add_custom_target(pre_target_" << command_idx << " DEPENDS " << stamp << ")\n";
        cm << "add_dependencies(" << t.name << " pre_target_" << command_idx << ")\n";
      }
      if (!s.postprocess_command.empty()) {
        cm << "add_custom_command(TARGET " << t.name << " POST_BUILD COMMAND " << s.postprocess_command << ")\n";
      }
    }
  }

  for (const auto& t : model.targets) {
    if (t.type != "executable")
      continue;
    cm << "add_executable(" << t.name;
    for (const auto& s : t.source_paths)
      cm << " \"" << s << "\"";
    cm << ")\n";
    if (!t.links.empty()) {
      cm << "target_link_libraries(" << t.name << " PRIVATE";
      for (const auto& n : t.links)
        cm << " " << n;
      cm << ")\n";
    }
    if (!t.include_dirs.empty()) {
      cm << "target_include_directories(" << t.name << " PRIVATE";
      for (const auto& inc : t.include_dirs)
        cm << " \"" << inc << "\"";
      cm << ")\n";
    }
  }

  cm << "include(CTest)\n";
  cm << "enable_testing()\n";
  for (const auto& t : model.targets) {
    if (t.type == "executable")
      cm << "add_test(NAME " << t.name << " COMMAND $<TARGET_FILE:" << t.name << ">)\n";
  }
  cm << "install(TARGETS";
  for (const auto& exe_name : model.install_exe_names)
    cm << " " << exe_name;
  cm << " RUNTIME DESTINATION bin)\n";
  for (const auto& rule : model.install_dir_rules) {
    if (!rule.preprocess_command.empty())
      cm << "add_custom_target(pre_include_dir_" << command_idx++ << " COMMAND " << rule.preprocess_command << ")\n";
    if (!rule.postprocess_command.empty())
      cm << "add_custom_target(post_include_dir_" << command_idx++ << " COMMAND " << rule.postprocess_command << ")\n";
    cm << "install(DIRECTORY \"" << rule.src << "/\" DESTINATION " << rule.dst
       << " FILES_MATCHING PATTERN \"*.h\" PATTERN \"*.hh\" PATTERN \"*.hpp\" PATTERN \"*.hxx\")\n";
  }
  for (const auto& rule : model.install_file_rules) {
    if (!rule.preprocess_command.empty())
      cm << "add_custom_target(pre_include_file_" << command_idx++ << " COMMAND " << rule.preprocess_command << ")\n";
    if (!rule.postprocess_command.empty())
      cm << "add_custom_target(post_include_file_" << command_idx++ << " COMMAND " << rule.postprocess_command << ")\n";
    cm << "install(FILES \"" << rule.src << "\" DESTINATION " << rule.dst << ")\n";
  }
  for (const auto& rule : model.asset_dir_rules) {
    if (!rule.preprocess_command.empty())
      cm << "add_custom_target(pre_asset_dir_" << command_idx++ << " COMMAND " << rule.preprocess_command << ")\n";
    if (!rule.postprocess_command.empty())
      cm << "add_custom_target(post_asset_dir_" << command_idx++ << " COMMAND " << rule.postprocess_command << ")\n";
    cm << "install(DIRECTORY \"" << rule.src << "/\" DESTINATION " << rule.dst << ")\n";
  }
  for (const auto& rule : model.asset_file_rules) {
    if (!rule.preprocess_command.empty())
      cm << "add_custom_target(pre_asset_file_" << command_idx++ << " COMMAND " << rule.preprocess_command << ")\n";
    if (!rule.postprocess_command.empty())
      cm << "add_custom_target(post_asset_file_" << command_idx++ << " COMMAND " << rule.postprocess_command << ")\n";
    cm << "install(FILES \"" << rule.src << "\" DESTINATION " << rule.dst << ")\n";
  }

  const auto out_cmake = model.build_root / "CMakeLists.txt";
  std::ofstream f(out_cmake);
  if (!f) {
    return 5;
  }
  f << cm.str();
  std::cout << "Wrote " << out_cmake << "\n";
  return 0;
}

}  // namespace up
