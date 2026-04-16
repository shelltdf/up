#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace up::gui::unix_shared {

// 与 up-gui Win32 同目录的 up_gui_settings.txt（UTF-8 key=value）。
std::filesystem::path settings_path_near_executable();

// 可执行文件所在目录（不含文件名）。
std::filesystem::path executable_parent_dir();

// 与 Win32 相同的「环境 → configure --opt」键（不含选项表里的 UP_*，由界面其它入口维护）。
struct PersistedEnv {
  std::string build_system = "cmake";
  std::string compiler = "msvc";
  std::string android_sdk;
  std::string android_ndk;
  std::string emsdk;
  std::string vcvars;
  std::string vcvars64;
  std::string vcvars32;
  std::string browse_cwd;
  std::string browse_scan;
  std::string browse_android_sdk;
  std::string browse_android_ndk;
  std::string browse_emsdk;
  bool ui_lang_zh = true;
};

void normalize_persisted_paths(PersistedEnv& e);
bool load_settings(const std::filesystem::path& path, PersistedEnv& out);
bool save_settings(const std::filesystem::path& path, const PersistedEnv& e);

// 追加与 Win32 AppendConfigureEnvArgs 一致的 --opt 片段（前导空格无，由调用方加空格衔接）。
void append_configure_env_opts(const PersistedEnv& e, std::string& args_utf8);

// 与 gui_core append_scan_args 等价（UTF-8），cwd 与某 scan 相同时跳过。
void append_scan_args_utf8(std::string& args, const std::vector<std::string>& scan_dirs, const std::string& cwd);

std::string path_to_portable_utf8(std::string s);

// POSIX shell 单引号包裹（含内部 ' 转义）。
std::string shell_single_quote(const std::string& s);

// 在 cwd 下执行 shell 命令行（已含可执行路径等），合并 stdout/stderr，返回是否 wait 成功；exit_code 为 -1 表示未知。
bool run_shell_in_dir(const std::filesystem::path& cwd, const std::string& shell_cmdline, std::string& combined_out,
                      int& exit_code);

// 从「Build Dir」编辑框语义（路径或 leaf）得到 .intermediate/build 下的目录名；空则 "default"。
std::string intermediate_leaf_from_build_dir_field(const std::string& field);

// 调用 `up print-build-dir-name`；失败返回 false 并写 err。
bool query_print_build_dir_name(const std::filesystem::path& up_exe, const std::filesystem::path& cwd,
                                const std::string& build_dir_field, const PersistedEnv& env, std::string& out_leaf,
                                std::string& err);

}  // namespace up::gui::unix_shared
