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

/// `configure_file` 解析结果：模板与期望输出在磁盘上的绝对路径 (尽可能 weakly_canonical)
struct ConfigFilePathPair {
  std::filesystem::path in_abs;
  std::filesystem::path out_abs;
};

struct TargetModel {
  std::string name;
  std::string kind;  // executable | static_library | shared_library
  std::vector<std::filesystem::path> source_paths_abs;
  std::set<std::string> link_to;
  std::vector<std::filesystem::path> include_dir_abs;
  /// 归属本 target 的 configure_file(…) 对 (in/out 已相对 Listfile 目录解算为绝对)
  std::vector<ConfigFilePathPair> config_files;
};

struct InterpretResult {
  std::string project_name;
  std::unordered_map<std::string, TargetModel> targets;
  /// Listfile 顶层/尚未遇到 add_* 时的 configure_file，用于 package.xml
  std::vector<ConfigFilePathPair> package_config_files;
  std::vector<std::string> errors;
  /// L1: 词法/解析提示
  std::vector<std::string> parse_diagnostics;
  /// L7: 自外部 codemodel JSON(用户预置) 提取的 target 名(尽力而为)
  std::vector<std::string> file_api_target_names;
  /// L7: 与 `targets` 键 仅用于对照的注记
  std::vector<std::string> file_api_merge_notes;
};

/// 与 `gz configure` 一致：`<源根>/.intermediate/build/<叶子>`。缺省叶子名 **`default`**；若
/// `…/build/` 下**恰有一个**子目录且没有名为 `default` 的目录，则使用该子目录名（与已跑过 configure 的叶一致）。
std::filesystem::path infer_gz_default_cmake_binary_root(const std::filesystem::path &top_cmake_parent_path);

/// 与 `gz configure` 的 `generated` 子路径对齐：先读 `build/…/gz_cache.txt` 的 `arch=`，否则用 **build 叶**目录名，再否则 **`default`**。
std::string infer_gz_generated_arch_segment(const std::filesystem::path &top_cmake_parent_path);

/// 若 `file_api_json_path` 非空, L7: 从该**用户预置** JSON 中尽力提取 target 名(不运行 cmake).
InterpretResult interpret_cmake_tree(const std::filesystem::path &source_root, const std::filesystem::path &top_cmake_lists,
                                    const std::filesystem::path *file_api_json_path = nullptr);
