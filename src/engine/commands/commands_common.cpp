#include "commands_common.hpp"

#include "paths.hpp"

#include <fstream>
#include <vector>

namespace up {

std::map<std::string, std::string> load_up_options(const std::filesystem::path& root, const std::string& arch) {
  std::map<std::string, std::string> out;
  const auto cache = root / arch / "up_cache.txt";
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
    if (k.rfind("UP_", 0) == 0)
      out[k] = v;
  }
  return out;
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
  const std::string generator = lower_ascii(option_or_compat(opts, "UP_CMAKE_GENERATOR", "", ""));
  if (generator.find("visual studio") != std::string::npos)
    toolchain = "msvc";
  else if (generator.find("clang") != std::string::npos)
    toolchain = "clang";
  else if (generator.find("mingw") != std::string::npos || generator.find("gcc") != std::string::npos)
    toolchain = "gcc";
  const std::string crt_mode = lower_ascii(option_or_compat(opts, "UP_TARGET_CRT", "UP_CRT", "dynamic_md"));
  return compose_arch_tag(system, cpu, build_system, toolchain, link_mode, config_mode, crt_mode);
}

std::string resolve_build_system_from_cache(const std::filesystem::path& cwd) {
  (void)cwd;
  return "cmake";
}

std::string resolve_arch_from_cache(const std::filesystem::path& cwd, const std::filesystem::path& build_root) {
  (void)cwd;
  const std::string host_arch = detect_arch_tag();
  const auto host_opts = load_up_options(build_root, host_arch);
  if (!host_opts.empty()) {
    const std::string full = arch_from_options(host_opts);
    if (!full.empty() && std::filesystem::exists(build_root / full / "up_cache.txt"))
      return full;
  }

  std::vector<std::pair<std::string, std::filesystem::file_time_type>> candidates;
  std::error_code ec;
  if (std::filesystem::exists(build_root, ec)) {
    for (std::filesystem::directory_iterator it(build_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
      if (ec)
        break;
      if (!it->is_directory())
        continue;
      const auto p = it->path() / "up_cache.txt";
      if (!std::filesystem::exists(p))
        continue;
      std::error_code tec;
      const auto t = std::filesystem::last_write_time(p, tec);
      if (tec)
        candidates.push_back({it->path().filename().string(), std::filesystem::file_time_type::min()});
      else
        candidates.push_back({it->path().filename().string(), t});
    }
  }
  if (candidates.size() == 1)
    return candidates.front().first;
  for (const auto& c : candidates) {
    if (c.first == host_arch)
      return c.first;
  }
  if (!candidates.empty()) {
    auto best = candidates.front();
    for (const auto& c : candidates) {
      if (c.second > best.second)
        best = c;
    }
    return best.first;
  }
  return host_arch;
}

}  // namespace up
