#include "cmake/cmake_backend.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

namespace gz {

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
    if (t.imported_prebuilt)
      continue;
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

std::string cmake_escape_string_value(const std::string& v) {
  std::string o;
  o.reserve(v.size() + 8);
  for (char c : v) {
    if (c == '\\')
      o += "\\\\";
    else if (c == '"')
      o += "\\\"";
    else if (c == ';')
      o += "\\;";
    else
      o.push_back(c);
  }
  return o;
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
    if (kv.first.rfind("GZ_", 0) != 0)
      continue;
    if (kv.first == "GZ_CMAKE_PREFIX_PATH")
      continue;
    cmd << " -D" << kv.first << "=\"" << kv.second << "\"";
  }
  if (!ctx.cmake_prefix_path.empty()) {
    cmd << " -DCMAKE_PREFIX_PATH=\"" << ctx.cmake_prefix_path << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  cmd << " && cmake --build \"" << to_posix_path_string(ctx.bin_dir) << "\" --parallel " << parallel_jobs_for_build(ctx.opts)
      << " --verbose";
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

  for (const auto& t : model.targets) {
    if (!t.imported_prebuilt)
      continue;
    const bool sh = (t.type == "prebuilt_shared_library");
    cm << "add_library(" << t.name << " " << (sh ? "SHARED" : "STATIC") << " IMPORTED)\n";
    if (sh) {
      cm << "if(WIN32)\n";
      cm << "  set_target_properties(" << t.name << " PROPERTIES\n";
      cm << "    IMPORTED_LOCATION \"" << cmake_escape_string_value(t.imported_location) << "\"\n";
      if (!t.imported_implib.empty())
        cm << "    IMPORTED_IMPLIB \"" << cmake_escape_string_value(t.imported_implib) << "\"\n";
      cm << "  )\n";
      cm << "else()\n";
      cm << "  set_target_properties(" << t.name << " PROPERTIES IMPORTED_LOCATION \""
         << cmake_escape_string_value(t.imported_location) << "\")\n";
      cm << "endif()\n";
    } else {
      cm << "set_target_properties(" << t.name << " PROPERTIES IMPORTED_LOCATION \""
         << cmake_escape_string_value(t.imported_location) << "\")\n";
    }
    if (!t.include_dirs.empty()) {
      cm << "target_include_directories(" << t.name << " INTERFACE";
      for (const auto& inc : t.include_dirs)
        cm << " \"" << inc << "\"";
      cm << ")\n";
    }
    if (pkg_src_root) {
      cm << "target_include_directories(" << t.name << " INTERFACE \""
          << to_posix_path_string(std::filesystem::absolute(*pkg_src_root)) << "\")\n";
    }
  }

  for (const auto& t : model.targets) {
    if (t.imported_prebuilt || t.type == "asset_bundle")
      continue;
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
    if (!t.compile_definitions.empty()) {
      cm << "target_compile_definitions(" << t.name << " PRIVATE";
      for (const auto& d : t.compile_definitions)
        cm << " \"" << cmake_escape_string_value(d) << "\"";
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
      cm << "target_link_libraries(" << t.name;
      std::vector<std::string> priv, pub, iface;
      for (const auto& pr : t.links) {
        if (pr.second == "public")
          pub.push_back(pr.first);
        else if (pr.second == "interface")
          iface.push_back(pr.first);
        else
          priv.push_back(pr.first);
      }
      auto emit_group = [&](const char* kw, const std::vector<std::string>& names) {
        if (names.empty())
          return;
        cm << " " << kw;
        for (const auto& n : names)
          cm << " " << n;
      };
      emit_group("PRIVATE", priv);
      emit_group("PUBLIC", pub);
      emit_group("INTERFACE", iface);
      cm << ")\n";
    }
    if (!t.include_dirs.empty()) {
      cm << "target_include_directories(" << t.name << " PRIVATE";
      for (const auto& inc : t.include_dirs)
        cm << " \"" << inc << "\"";
      cm << ")\n";
    }
    if (!t.compile_definitions.empty()) {
      cm << "target_compile_definitions(" << t.name << " PRIVATE";
      for (const auto& d : t.compile_definitions)
        cm << " \"" << cmake_escape_string_value(d) << "\"";
      cm << ")\n";
    }
  }

  cm << "include(CTest)\n";
  cm << "enable_testing()\n";
  for (const auto& t : model.targets) {
    if (t.type == "executable")
      cm << "add_test(NAME " << t.name << " COMMAND $<TARGET_FILE:" << t.name << ">)\n";
  }
  if (!model.install_exe_names.empty()) {
    cm << "install(TARGETS";
    for (const auto& exe_name : model.install_exe_names)
      cm << " " << exe_name;
    cm << " RUNTIME DESTINATION bin)\n";
  }

  {
    std::vector<std::string> native_lib_names;
    for (const auto& t : model.targets) {
      if (t.imported_prebuilt || t.type == "asset_bundle")
        continue;
      if (t.type == "static_library" || t.type == "shared_library")
        native_lib_names.push_back(t.name);
    }
    if (!native_lib_names.empty()) {
      cm << "install(TARGETS";
      for (const auto& n : native_lib_names)
        cm << " " << n;
      cm << " ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)\n";
    }
  }

  for (const auto& t : model.targets) {
    if (!t.imported_prebuilt)
      continue;
    if (t.type == "prebuilt_shared_library") {
      cm << "if(WIN32)\n";
      if (!t.imported_dll.empty())
        cm << "  install(FILES \"" << cmake_escape_string_value(t.imported_dll) << "\" DESTINATION bin)\n";
      if (!t.imported_implib.empty())
        cm << "  install(FILES \"" << cmake_escape_string_value(t.imported_implib) << "\" DESTINATION lib)\n";
      cm << "else()\n";
      cm << "  install(FILES \"" << cmake_escape_string_value(t.imported_location) << "\" DESTINATION lib)\n";
      cm << "endif()\n";
    } else {
      cm << "install(FILES \"" << cmake_escape_string_value(t.imported_location) << "\" DESTINATION lib)\n";
    }
  }

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

  // Ensure Visual Studio generators always emit an install target even for library-wrapper-only packages.
  cm << "install(CODE \"message(STATUS \\\"gz: install stage complete\\\")\")\n";

  const auto out_cmake = model.build_root / "CMakeLists.txt";
  std::ofstream f(out_cmake);
  if (!f) {
    return 5;
  }
  f << cm.str();
  std::cout << "Wrote " << to_posix_path_string(out_cmake) << std::endl;
  return 0;
}

}  // namespace gz
