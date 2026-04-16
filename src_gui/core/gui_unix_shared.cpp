#include "gui_unix_shared.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
#include <sys/wait.h>

static void trim_in_place(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
}

static void trim_trailing_dir_seps(std::string& s) {
  while (!s.empty() && (s.back() == '\\' || s.back() == '/'))
    s.pop_back();
}

static bool path_equal_ci_utf8(const std::string& a, const std::string& b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z')
      ca = static_cast<char>(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z')
      cb = static_cast<char>(cb - 'A' + 'a');
    if (ca != cb)
      return false;
  }
  return true;
}

static std::string shell_single_quote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += '\'';
  return out;
}

namespace up::gui::unix_shared {

std::filesystem::path executable_parent_dir() {
  std::string exe;
#if defined(__APPLE__)
  char buf[4096];
  uint32_t sz = sizeof(buf);
  if (_NSGetExecutablePath(buf, &sz) != 0)
    return {};
  exe = buf;
#else
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0)
    return {};
  buf[static_cast<size_t>(n)] = '\0';
  exe.assign(buf, static_cast<size_t>(n));
#endif
  std::filesystem::path p(exe);
  if (p.has_parent_path())
    return p.parent_path();
  return {};
}

std::filesystem::path settings_path_near_executable() {
  return executable_parent_dir() / "up_gui_settings.txt";
}

std::string path_to_portable_utf8(std::string s) {
  trim_in_place(s);
  if (s.empty())
    return s;
  std::error_code ec;
  std::filesystem::path p(s);
  return p.lexically_normal().generic_string();
}

void normalize_persisted_paths(PersistedEnv& e) {
  e.android_sdk = path_to_portable_utf8(e.android_sdk);
  e.android_ndk = path_to_portable_utf8(e.android_ndk);
  e.emsdk = path_to_portable_utf8(e.emsdk);
  e.vcvars = path_to_portable_utf8(e.vcvars);
  e.vcvars64 = path_to_portable_utf8(e.vcvars64);
  e.vcvars32 = path_to_portable_utf8(e.vcvars32);
  e.browse_cwd = path_to_portable_utf8(e.browse_cwd);
  e.browse_scan = path_to_portable_utf8(e.browse_scan);
  e.browse_android_sdk = path_to_portable_utf8(e.browse_android_sdk);
  e.browse_android_ndk = path_to_portable_utf8(e.browse_android_ndk);
  e.browse_emsdk = path_to_portable_utf8(e.browse_emsdk);
}

bool load_settings(const std::filesystem::path& path, PersistedEnv& out) {
  out = PersistedEnv{};
  std::ifstream f(path);
  if (!f)
    return false;
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    std::string v = line.substr(pos + 1);
    trim_in_place(v);
    if (k == "local.build_system")
      out.build_system = v;
    else if (k == "local.compiler")
      out.compiler = v;
    else if (k == "android.sdk" && !v.empty())
      out.android_sdk = v;
    else if (k == "android.ndk" && !v.empty())
      out.android_ndk = v;
    else if (k == "emsdk.path" && !v.empty())
      out.emsdk = v;
    else if (k == "local.vcvars")
      out.vcvars = v;
    else if (k == "local.vcvars64" && !v.empty())
      out.vcvars64 = v;
    else if (k == "local.vcvars32" && !v.empty())
      out.vcvars32 = v;
    else if (k == "browse.cwd")
      out.browse_cwd = v;
    else if (k == "browse.scan")
      out.browse_scan = v;
    else if (k == "browse.android_sdk")
      out.browse_android_sdk = v;
    else if (k == "browse.android_ndk")
      out.browse_android_ndk = v;
    else if (k == "browse.emsdk")
      out.browse_emsdk = v;
    else if (k == "gui.ui_lang") {
      for (char& c : v) {
        if (c >= 'A' && c <= 'Z')
          c = static_cast<char>(c - 'A' + 'a');
      }
      out.ui_lang_zh = (v != "en");
    }
  }
  normalize_persisted_paths(out);
  return true;
}

bool save_settings(const std::filesystem::path& path, const PersistedEnv& in) {
  PersistedEnv e = in;
  normalize_persisted_paths(e);
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f)
    return false;
  f << "local.build_system=" << e.build_system << "\n";
  f << "local.compiler=" << e.compiler << "\n";
  f << "android.sdk=" << e.android_sdk << "\n";
  f << "android.ndk=" << e.android_ndk << "\n";
  f << "emsdk.path=" << e.emsdk << "\n";
  f << "local.vcvars=" << e.vcvars << "\n";
  f << "local.vcvars64=" << e.vcvars64 << "\n";
  f << "local.vcvars32=" << e.vcvars32 << "\n";
  f << "browse.cwd=" << e.browse_cwd << "\n";
  f << "browse.scan=" << e.browse_scan << "\n";
  f << "browse.android_sdk=" << e.browse_android_sdk << "\n";
  f << "browse.android_ndk=" << e.browse_android_ndk << "\n";
  f << "browse.emsdk=" << e.browse_emsdk << "\n";
  f << "gui.ui_lang=" << (e.ui_lang_zh ? "zh" : "en") << "\n";
  return true;
}

void append_configure_env_opts(const PersistedEnv& e, std::string& args_utf8) {
  auto append_opt = [&](const char* k, const std::string& v) {
    if (v.empty())
      return;
    const std::string kv = std::string(k) + "=" + v;
    args_utf8 += " --opt ";
    args_utf8 += shell_single_quote(kv);
  };
  append_opt("UP_TARGET_BUILD_SYSTEM", e.build_system);
  append_opt("UP_TARGET_COMPILER", e.compiler);
  append_opt("UP_ANDROID_SDK", e.android_sdk);
  append_opt("UP_ANDROID_NDK", e.android_ndk);
  append_opt("UP_EMSDK_PATH", e.emsdk);
}

void append_scan_args_utf8(std::string& args, const std::vector<std::string>& scan_dirs, const std::string& cwd) {
  std::string cwd_norm = cwd;
  trim_trailing_dir_seps(cwd_norm);
  for (const auto& d : scan_dirs) {
    if (d.empty())
      continue;
    std::string dn = d;
    trim_trailing_dir_seps(dn);
    if (!cwd_norm.empty() && path_equal_ci_utf8(dn, cwd_norm))
      continue;
    args += " --scan ";
    args += shell_single_quote(d);
  }
}

bool run_shell_in_dir(const std::filesystem::path& cwd, const std::string& shell_cmdline, std::string& combined_out,
                      int& exit_code) {
  combined_out.clear();
  exit_code = -1;
  const std::string cwd_s = cwd.empty() ? "." : cwd.generic_string();
  const std::string full = "cd " + shell_single_quote(cwd_s) + " && " + shell_cmdline;
  FILE* pipe = popen(full.c_str(), "r");
  if (!pipe)
    return false;
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe))
    combined_out += buf;
  const int st = pclose(pipe);
  if (WIFEXITED(st))
    exit_code = WEXITSTATUS(st);
  else if (WIFSIGNALED(st))
    exit_code = 128 + WTERMSIG(st);
  return true;
}

std::string intermediate_leaf_from_build_dir_field(const std::string& field) {
  std::string s = field;
  trim_in_place(s);
  if (s.empty())
    return {};
  std::filesystem::path p(s);
  std::string leaf = p.filename().generic_string();
  if (leaf.empty() || leaf == "." || leaf == "..")
    return {};
  return leaf;
}

bool query_print_build_dir_name(const std::filesystem::path& up_exe, const std::filesystem::path& cwd,
                                const std::string& build_dir_field, const PersistedEnv& env, std::string& out_leaf,
                                std::string& err) {
  out_leaf.clear();
  err.clear();
  std::string leaf = intermediate_leaf_from_build_dir_field(build_dir_field);
  if (leaf.empty())
    leaf = "default";
  std::string opt_part;
  append_configure_env_opts(env, opt_part);
  const std::string up_s = up_exe.generic_string();
  std::string cmd = shell_single_quote(up_s) + " print-build-dir-name --build-dir-name " + shell_single_quote(leaf);
  cmd += opt_part;
  std::string out;
  int code = -1;
  if (!run_shell_in_dir(cwd, cmd, out, code)) {
    err = "failed to run print-build-dir-name";
    return false;
  }
  // trim trailing newlines
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  trim_in_place(out);
  const auto nl = out.find_first_of("\r\n");
  if (nl != std::string::npos)
    out.resize(nl);
  trim_in_place(out);
  if (code != 0 || out.empty()) {
    err = "print-build-dir-name failed (exit " + std::to_string(code) + ")";
    if (!out.empty())
      err += ": " + out;
    return false;
  }
  out_leaf = std::move(out);
  return true;
}

}  // namespace up::gui::unix_shared
