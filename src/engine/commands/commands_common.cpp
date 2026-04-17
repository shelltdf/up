#include "commands_common.hpp"
#include "cli_verbose.hpp"

#include "paths.hpp"

#include <fstream>
#include <iostream>

namespace up {

namespace {
bool g_cli_verbose = false;
}  // namespace

void set_cli_verbose(bool on) { g_cli_verbose = on; }

bool cli_verbose() { return g_cli_verbose; }

void cli_verbose_phase(const char* command, const char* phase) {
  if (!g_cli_verbose)
    return;
  std::cerr << command << ": [verbose] phase=" << phase << "\n" << std::flush;
}

std::map<std::string, std::string> load_up_options_from_build_dir(const std::filesystem::path& build_dir) {
  std::map<std::string, std::string> out;
  const auto cache = build_dir / "up_cache.txt";
  std::ifstream f(cache);
  if (!f)
    return out;
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    const std::string v = line.substr(pos + 1);
    if (k.rfind("UP_", 0) == 0 || k.rfind("UPSTREAM_", 0) == 0)
      out[k] = v;
  }
  return out;
}

std::map<std::string, std::string> load_up_options(const std::filesystem::path& root, const std::string& arch) {
  if (arch.empty())
    return load_up_options_from_build_dir(root);
  return load_up_options_from_build_dir(root / std::filesystem::u8path(arch));
}

std::string read_plain_cache_value(const std::filesystem::path& cache_file, const std::string& key) {
  std::ifstream f(cache_file);
  if (!f)
    return {};
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    if (k == key)
      return line.substr(pos + 1);
  }
  return {};
}

std::string option_or(const std::map<std::string, std::string>& opts, const std::string& key, const std::string& defv) {
  const auto it = opts.find(key);
  return it == opts.end() ? defv : it->second;
}

std::string option_or_compat(const std::map<std::string, std::string>& opts,
                             const std::string& preferred,
                             const std::string& legacy,
                             const std::string& defv) {
  const auto a = opts.find(preferred);
  if (a != opts.end())
    return a->second;
  const auto b = opts.find(legacy);
  if (b != opts.end())
    return b->second;
  return defv;
}

bool equals_ci(std::string a, std::string b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] >= 'A' && a[i] <= 'Z')
      a[i] = static_cast<char>(a[i] - 'A' + 'a');
    if (b[i] >= 'A' && b[i] <= 'Z')
      b[i] = static_cast<char>(b[i] - 'A' + 'a');
  }
  return a == b;
}

bool contains_ci(std::string haystack, std::string needle) {
  for (char& c : haystack)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  for (char& c : needle)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return haystack.find(needle) != std::string::npos;
}

std::string arch_from_target_cpu(std::string v) {
  return normalize_cpu_arch_tag(v);
}

std::string lower_ascii(std::string v) {
  for (char& c : v)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return v;
}

std::string arch_from_options(const std::map<std::string, std::string>& opts) {
  const std::string cpu = arch_from_target_cpu(option_or_compat(opts, "UP_TARGET_CPU_ARCH", "UP_CPU_ARCH", detect_arch_tag()));
  const std::string system = lower_ascii(option_or_compat(opts, "UP_TARGET_SYSTEM", "UP_SYSTEM", detect_host_system_tag()));
  const std::string dyn = lower_ascii(option_or_compat(opts, "UP_TARGET_DYNAMIC_LIBRARY", "UP_DYNAMIC_LIBRARY", "OFF"));
  const std::string link_mode = (dyn == "on" || dyn == "1" || dyn == "true") ? "dynamic" : "static";
  const std::string dbg = lower_ascii(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"));
  const std::string config_mode = (dbg == "on" || dbg == "1" || dbg == "true") ? "debug" : "release";
  const std::string build_system = lower_ascii(option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake"));
  std::string toolchain = detect_host_toolchain_tag();
  const std::string generator = lower_ascii(option_or(opts, "UP_CMAKE_GENERATOR", std::string()));
  if (generator.find("visual studio") != std::string::npos)
    toolchain = "msvc";
  else if (generator.find("clang") != std::string::npos)
    toolchain = "clang";
  else if (generator.find("mingw") != std::string::npos || generator.find("gcc") != std::string::npos)
    toolchain = "gcc";
  const std::string crt_mode = lower_ascii(option_or_compat(opts, "UP_TARGET_CRT", "UP_CRT", "dynamic_md"));
  return compose_arch_tag(system, cpu, build_system, toolchain, link_mode, config_mode, crt_mode);
}

}  // namespace up
