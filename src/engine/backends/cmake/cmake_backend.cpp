#include "cmake/cmake_backend.hpp"
#include "cmake/cmake_third_party_heuristics.hpp"

#include "paths.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

namespace up {

namespace {

bool path_starts_with(const std::filesystem::path& prefix_raw, const std::filesystem::path& p_raw) {
  std::error_code ec;
  std::filesystem::path prefix = std::filesystem::weakly_canonical(prefix_raw, ec);
  if (ec)
    prefix = std::filesystem::absolute(prefix_raw);
  std::filesystem::path full = std::filesystem::weakly_canonical(p_raw, ec);
  if (ec)
    full = std::filesystem::absolute(p_raw);
  const std::string pre = prefix.generic_string();
  const std::string s = full.generic_string();
  if (pre.size() > s.size())
    return false;
  if (s.compare(0, pre.size(), pre) != 0)
    return false;
  if (pre.size() == s.size())
    return true;
  return s[pre.size()] == '/';
}

// Longest filesystem path that is a prefix directory of every source file (for single-tree packages).
std::optional<std::filesystem::path> infer_common_source_root(const ConfigureGraphModel& model) {
  std::vector<std::filesystem::path> paths;
  for (const auto& t : model.targets) {
    for (const auto& sp : t.source_paths)
      paths.emplace_back(sp);
  }
  if (paths.empty())
    return std::nullopt;
  std::filesystem::path common = paths[0];
  for (size_t i = 1; i < paths.size(); ++i) {
    while (!common.empty() && !path_starts_with(common, paths[i]))
      common = common.parent_path();
  }
  if (common.empty())
    return std::nullopt;
  std::error_code ec;
  if (std::filesystem::is_regular_file(common, ec))
    common = common.parent_path();
  if (common.empty())
    return std::nullopt;
  return common;
}

}  // namespace

std::string build_cmake_build_command(const BuildBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << to_posix_path_string(ctx.src_dir) << "\" -B \"" << to_posix_path_string(ctx.bin_dir)
      << "\" -DCMAKE_INSTALL_PREFIX=\"" << to_posix_path_string(ctx.install_dir) << "\"";
  if (!ctx.cmake_generator.empty()) {
    cmd << " -G \"" << ctx.cmake_generator << "\"";
  }
  for (const auto& kv : ctx.opts) {
    cmd << " -D" << kv.first << "=\"" << kv.second << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  // --verbose: ask CMake to run the underlying tool verbosely (e.g. MSBuild shows more than default minimal).
  cmd << " && cmake --build \"" << to_posix_path_string(ctx.bin_dir) << "\" --verbose";
  if (ctx.multi_config) {
    cmd << " --config " << ctx.config_name;
  }
  cmd << " --target install";
  return cmd.str();
}

std::string build_cmake_configure_command(const ConfigureBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << to_posix_path_string(ctx.source_dir) << "\" -B \"" << to_posix_path_string(ctx.out_dir) << "\"";
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
  cm << "project(" << model.package_name << " LANGUAGES C CXX)\n";
  cm << "set(CMAKE_CXX_STANDARD 17)\n";
  cm << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";

  const std::optional<std::filesystem::path> pkg_src_root = infer_common_source_root(model);
  if (pkg_src_root)
    cmake_third_party::emit_detected_upstream_snippets(cm, *pkg_src_root);

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
    if (pkg_src_root) {
      cm << "target_include_directories(" << t.name << " PUBLIC \""
          << to_posix_path_string(std::filesystem::absolute(*pkg_src_root)) << "\")\n";
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
  // Flush before a later system(cmake) so captured stdout does not splice into this line.
  std::cout << "Wrote " << to_posix_path_string(out_cmake) << std::endl;
  return 0;
}

}  // namespace up
