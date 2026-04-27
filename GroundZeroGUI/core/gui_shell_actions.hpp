#pragma once

#include "gui_os_queue.hpp"
#include "gui_persist.hpp"

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gz::gui::shell {

void trim_ascii_inplace(std::string& s);

bool append_install_dir_flag(std::string& args, const std::string& inst_edit, std::string& err);

// 失败时通过 log 写原因并返回空字符串。
std::string build_configure_args_line(const std::filesystem::path& gz_exe, const std::string& cwd_portable,
                                      const std::string& build_dir_field, const std::vector<std::string>& scan_roots,
                                      const persist::PersistedEnv& env, os::LogSink log_line);

// 占用 busy、校验 CWD 与 gz 可执行文件存在；失败时已复位 busy 并打日志。
std::optional<std::filesystem::path> try_acquire_run_context(const std::filesystem::path& gz_exe,
                                                            const std::string& cwd_portable, std::atomic<bool>& busy,
                                                            os::LogSink log_line);

// 要求调用前已通过 try_acquire_run_context 且 busy==true；线程结束后复位 busy 并执行 on_done_on_ui。
void run_gz_command_in_detached_thread(const std::filesystem::path& gz_exe, const std::filesystem::path& cwd_p,
                                       const std::string& args_no_exe, const std::string& extra_args_raw,
                                       std::atomic<bool>& busy, os::LogSink log_line, os::UiTask on_done_on_ui);

}  // namespace gz::gui::shell
