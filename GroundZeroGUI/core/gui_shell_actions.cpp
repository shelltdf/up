#include "gui_shell_actions.hpp"

#include <thread>

namespace gz::gui::shell {

void trim_ascii_inplace(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
}

bool append_install_dir_flag(std::string& args, const std::string& inst_edit, std::string& err) {
  std::string inst = persist::path_to_portable_utf8(inst_edit);
  trim_ascii_inplace(inst);
  if (inst.empty()) {
    err = "Install Dir empty";
    return false;
  }
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(inst);
  if (leaf.empty()) {
    err = "Invalid install dir leaf";
    return false;
  }
  args += " --install-dir-name ";
  args += persist::shell_single_quote(leaf);
  return true;
}

std::string build_configure_args_line(const std::filesystem::path& gz_exe, const std::string& cwd_portable,
                                        const std::string& build_dir_field, const std::vector<std::string>& scan_roots,
                                        const persist::PersistedEnv& env, os::LogSink log_line) {
  if (cwd_portable.empty()) {
    log_line("[error] Set CWD first.");
    return {};
  }
  std::string leaf;
  std::string qerr;
  if (!persist::query_print_build_dir_name(gz_exe, std::filesystem::path(cwd_portable), build_dir_field, env, leaf,
                                             qerr)) {
    log_line("[error] print-build-dir-name: " + qerr);
    return {};
  }
  std::string args = "configure";
  persist::append_scan_args_utf8(args, scan_roots, cwd_portable);
  persist::append_configure_env_opts(env, args);
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  return args;
}

std::optional<std::filesystem::path> try_acquire_run_context(const std::filesystem::path& gz_exe,
                                                              const std::string& cwd_portable, std::atomic<bool>& busy,
                                                              os::LogSink log_line) {
  if (busy.exchange(true)) {
    log_line("[busy] previous run still in progress");
    busy = false;
    return std::nullopt;
  }
  if (cwd_portable.empty()) {
    log_line("[error] Set CWD first.");
    busy = false;
    return std::nullopt;
  }
  if (!std::filesystem::exists(gz_exe)) {
    log_line("[error] `gz` not found next to this executable.");
    busy = false;
    return std::nullopt;
  }
  return std::filesystem::path(cwd_portable);
}

void run_gz_command_in_detached_thread(const std::filesystem::path& gz_exe, const std::filesystem::path& cwd_p,
                                       const std::string& args_no_exe, const std::string& extra_args_raw,
                                       std::atomic<bool>& busy, os::LogSink log_line, os::UiTask on_done_on_ui) {
  std::string extra = extra_args_raw;
  trim_ascii_inplace(extra);
  std::thread([gz_exe, cwd_p, args_no_exe, extra, &busy, log_line, on_done_on_ui]() {
    std::string cmd = persist::shell_single_quote(gz_exe.generic_string()) + " " + args_no_exe;
    if (!extra.empty())
      cmd += " " + extra;
    std::string out;
    int code = -1;
    const bool ok = persist::run_shell_in_dir(cwd_p, cmd, out, code);
    if (!ok)
      log_line("[error] failed to spawn shell");
    else {
      if (!out.empty())
        log_line(out);
      log_line("[exit " + std::to_string(code) + "]");
    }
    busy = false;
    if (on_done_on_ui)
      on_done_on_ui();
  }).detach();
}

}  // namespace gz::gui::shell
