#pragma once

// 将 parse_cmake_script 产生的「命令流」(语句级浅层 AST) 做 **静态、可证伪子集** 的
// 语义重解释, 与真实 cmake configure 不保证一致 — 见 02-physical/gz-reverse-cmake/spec.md

#include "cmake_parse.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct TargetModel {
  std::string name;
  std::string kind;  // executable | static_library | shared_library
  std::vector<std::filesystem::path> source_paths_abs;
  std::set<std::string> link_to;
  std::vector<std::filesystem::path> include_dir_abs;
};

struct InterpretResult {
  std::string project_name;
  std::unordered_map<std::string, TargetModel> targets;
  std::vector<std::string> errors;
};

InterpretResult interpret_cmake_tree(const std::filesystem::path &source_root,
                                     const std::filesystem::path &top_cmake_lists);
