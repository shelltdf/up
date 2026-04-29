#pragma once

// 将 parse_cmake_script 产生的「命令流」(语句级浅层 AST) 做 **静态、可证伪子集** 的
// 语义重解释, 与真实 cmake configure 不保证一致 — 见 02-physical/gz-reverse-cmake/spec.md

#include "cmake_parse.hpp"

#include <filesystem>
#include <functional>
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
  /// target_compile_options / set_target_properties(COMPILE_FLAGS) 等（约简为一串 argv 片段，顺序见 interpret）
  std::vector<std::string> compile_flags;
  /// target_link_options / set_target_properties(LINK_FLAGS)
  std::vector<std::string> link_flags;
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

/// 解析每个 **新** `CMakeLists.txt` 时调用 (add_subdirectory 展开, 同一路径不重复), 第一个参数为自 1 起的累计序号
struct ListfileProgress {
  std::function<void(std::size_t one_based, const std::filesystem::path &listfile)> on;
  /// 同一 `CMakeLists.txt` 内阶段/条命令进展: `cur/total` 在 `total==0` 时表示**不定长** (仅看 `phase`)
  std::function<void(std::size_t one_based, const std::filesystem::path &listfile, const char *phase, std::size_t cur, std::size_t total)>
      on_intra;
  std::size_t count{0};
  void emit(const std::filesystem::path &listfile) {
    if (!on) return;
    on(++count, listfile);
  }
  void emit_intra(const std::filesystem::path &listfile, const char *phase, std::size_t cur, std::size_t total) {
    if (!on_intra) return;
    on_intra(count, listfile, phase, cur, total);
  }
};

/// 若 `file_api_json_path` 非空, L7: 从该**用户预置** JSON 中尽力提取 target 名(不运行 cmake).
/// `listfile_progress` 非空时: `on` 每 **新** `CMakeLists.txt` 一次; `on_intra` 在同文件内 (读/解析、include/宏 轮次、if 压平、预扫/主扫 命令序等) 见实现
InterpretResult interpret_cmake_tree(const std::filesystem::path &source_root, const std::filesystem::path &top_cmake_lists,
                                     const std::filesystem::path *file_api_json_path = nullptr, ListfileProgress *listfile_progress = nullptr);
