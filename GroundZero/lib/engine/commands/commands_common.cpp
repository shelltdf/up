#include "commands_common.hpp"
#include "cli_verbose.hpp"

#include "paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace gz {

namespace {
bool g_cli_verbose = false;
}  // namespace

bool gz_source_path_is_cmake_binary_dir(const std::string& s) {
  const char* p = gz_cmake_binary_dir_source_prefix();
  const size_t n = std::strlen(p);
  return s.size() >= n && s.compare(0, n, p) == 0;
}

std::string gz_cmake_binary_dir_source_rel(const std::string& s) {
  const char* p = gz_cmake_binary_dir_source_prefix();
  const size_t n = std::strlen(p);
  if (s.size() < n || s.compare(0, n, p) != 0)
    return {};
  return s.substr(n);
}

void collect_gz_desc_xml_files(const std::filesystem::path& root, std::vector<std::filesystem::path>& package_files,
                               std::vector<std::filesystem::path>& target_files, const char* log_prefix) {
  if (!std::filesystem::exists(root))
    return;
  const char* tag = (log_prefix && log_prefix[0]) ? log_prefix : "gz";
  static const char* kSkipSubdirNames[] = {".git",        "node_modules", ".svn",        "__pycache__", ".vs",
                                            "CMakeFiles", "bower_components", "vendor", "dist",        "out",
                                            "vcpkg"};
  const size_t pkg0 = package_files.size();
  const size_t tgt0 = target_files.size();
  std::uint64_t step = 0;
  for (std::filesystem::recursive_directory_iterator it(
           root, std::filesystem::directory_options::skip_permission_denied);
       it != std::filesystem::recursive_directory_iterator(); ++it) {
    ++step;
    if (step == 1u || (step % 10000u) == 0u) {
      std::cerr << tag << ": scanning… step=" << step << " in " << to_posix_path_string(root) << "\n" << std::flush;
    }
    if (it->is_directory()) {
      const std::filesystem::path fn = it->path().filename();
      if (fn == ".intermediate") {
        it.disable_recursion_pending();
        continue;
      }
      for (const char* skip : kSkipSubdirNames) {
        if (fn == skip) {
          it.disable_recursion_pending();
          break;
        }
      }
      continue;
    }
    if (!it->is_regular_file())
      continue;
    const auto p = it->path();
    if (p.filename() == "package.xml")
      package_files.push_back(p);
    else if (p.filename() == "target.xml")
      target_files.push_back(p);
  }
  const size_t d_pkg = package_files.size() - pkg0;
  const size_t d_tgt = target_files.size() - tgt0;
  std::cerr << tag << ": done " << to_posix_path_string(root) << " (" << step << " step(s), +" << d_pkg
            << " package.xml, +" << d_tgt << " target.xml)\n"
            << std::flush;
}

bool gz_mergeable_option_key(const std::string& k) {
  if (k.empty())
    return false;
  static const std::set<std::string> meta = {
      "gz.cache.version", "cwd", "arch", "package", "generated_file", "scan_roots",
  };
  if (meta.count(k) != 0)
    return false;
  if (k.rfind("GZ_", 0) == 0)
    return true;
  static const std::regex id(R"rx(^[A-Za-z_][A-Za-z0-9_]*$)rx");
  return std::regex_match(k, id);
}

void set_cli_verbose(bool on) { g_cli_verbose = on; }

bool cli_verbose() { return g_cli_verbose; }

void cli_verbose_phase(const char* command, const char* phase) {
  if (!g_cli_verbose)
    return;
  std::cerr << command << ": [verbose] phase=" << phase << "\n" << std::flush;
}

std::map<std::string, std::string> load_gz_options_from_build_dir(const std::filesystem::path& build_dir) {
  std::map<std::string, std::string> out;
  const auto cache = build_dir / "gz_cache.txt";
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
    if (gz_mergeable_option_key(k))
      out[k] = v;
  }
  return out;
}

std::map<std::string, std::string> load_gz_options(const std::filesystem::path& root, const std::string& arch) {
  if (arch.empty())
    return load_gz_options_from_build_dir(root);
  return load_gz_options_from_build_dir(root / std::filesystem::u8path(arch));
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

namespace {

unsigned parse_jobs_string(const std::string& s_raw) {
  std::string s = s_raw;
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  if (s.empty())
    return 0;
  unsigned v = 0;
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
    if (c < '0' || c > '9')
      return 0;
    v = v * 10u + static_cast<unsigned>(c - '0');
    if (v > 512u)
      return 512u;
  }
  return v > 0 ? v : 0u;
}

}  // namespace

namespace {

#if defined(_WIN32)
unsigned os_preferred_logical_processor_count() {
  const DWORD active = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (active > 0 && active <= 65536u)
    return static_cast<unsigned>(active);
  SYSTEM_INFO si{};
  GetNativeSystemInfo(&si);
  if (si.dwNumberOfProcessors > 0 && si.dwNumberOfProcessors <= 65536u)
    return static_cast<unsigned>(si.dwNumberOfProcessors);
  return 0u;
}
#elif defined(__linux__)
unsigned os_preferred_logical_processor_count() {
  const long n = sysconf(_SC_NPROCESSORS_ONLN);
  if (n > 0)
    return static_cast<unsigned>((std::min)(n, 512L));
  return 0u;
}
#elif defined(__APPLE__)
unsigned os_preferred_logical_processor_count() {
  int n = 0;
  size_t sz = sizeof(n);
  if (sysctlbyname("hw.logicalcpu", &n, &sz, nullptr, 0) == 0 && n > 0)
    return static_cast<unsigned>((std::min)(n, 512));
  n = 0;
  sz = sizeof(n);
  if (sysctlbyname("hw.ncpu", &n, &sz, nullptr, 0) == 0 && n > 0)
    return static_cast<unsigned>((std::min)(n, 512));
  return 0u;
}
#else
unsigned os_preferred_logical_processor_count() { return 0u; }
#endif

}  // namespace

unsigned default_parallel_jobs_hardware() {
  unsigned n = os_preferred_logical_processor_count();
  if (n == 0)
    n = static_cast<unsigned>(std::thread::hardware_concurrency());
  if (n == 0)
    n = 4u;
  return std::max(1u, std::min(n, 512u));
}

unsigned parallel_jobs_for_build(const std::map<std::string, std::string>& opts) {
  for (const char* key : {"GZ_BUILD_PARALLEL", "GZ_BUILD_JOBS"}) {
    const auto it = opts.find(key);
    if (it != opts.end()) {
      if (unsigned p = parse_jobs_string(it->second))
        return std::max(1u, std::min(p, 512u));
    }
  }
  return default_parallel_jobs_hardware();
}

void ensure_default_build_parallel_options(std::map<std::string, std::string>& opts) {
  for (const char* key : {"GZ_BUILD_PARALLEL", "GZ_BUILD_JOBS"}) {
    const auto it = opts.find(key);
    if (it != opts.end() && parse_jobs_string(it->second) != 0)
      return;
  }
  const std::string v = std::to_string(default_parallel_jobs_hardware());
  opts["GZ_BUILD_PARALLEL"] = v;
  opts["GZ_BUILD_JOBS"] = v;
}

std::string arch_from_options(const std::map<std::string, std::string>& opts) {
  const std::string cpu = arch_from_target_cpu(option_or_compat(opts, "GZ_TARGET_CPU_ARCH", "GZ_CPU_ARCH", detect_arch_tag()));
  const std::string system = lower_ascii(option_or_compat(opts, "GZ_TARGET_SYSTEM", "GZ_SYSTEM", detect_host_system_tag()));
  const std::string dyn = lower_ascii(option_or_compat(opts, "GZ_TARGET_DYNAMIC_LIBRARY", "GZ_DYNAMIC_LIBRARY", "OFF"));
  const std::string link_mode = (dyn == "on" || dyn == "1" || dyn == "true") ? "dynamic" : "static";
  const std::string dbg = lower_ascii(option_or_compat(opts, "GZ_TARGET_DEBUG", "GZ_DEBUG", "OFF"));
  const std::string config_mode = (dbg == "on" || dbg == "1" || dbg == "true") ? "debug" : "release";
  const std::string build_system = lower_ascii(option_or_compat(opts, "GZ_TARGET_BUILD_SYSTEM", "GZ_BUILD_SYSTEM", "cmake"));
  std::string toolchain = detect_host_toolchain_tag();
  const std::string generator = lower_ascii(option_or(opts, "GZ_CMAKE_GENERATOR", std::string()));
  if (generator.find("visual studio") != std::string::npos)
    toolchain = "msvc";
  else if (generator.find("clang") != std::string::npos)
    toolchain = "clang";
  else if (generator.find("mingw") != std::string::npos || generator.find("gcc") != std::string::npos)
    toolchain = "gcc";
  const std::string crt_mode = lower_ascii(option_or_compat(opts, "GZ_TARGET_CRT", "GZ_CRT", "dynamic_md"));
  return compose_arch_tag(system, cpu, build_system, toolchain, link_mode, config_mode, crt_mode);
}

bool gz_path_is_same_or_under(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  std::error_code ec;
  const std::filesystem::path rc = std::filesystem::weakly_canonical(std::filesystem::absolute(root), ec);
  const std::filesystem::path cc = std::filesystem::weakly_canonical(std::filesystem::absolute(candidate), ec);
  if (ec || rc.empty())
    return false;
  const std::filesystem::path rel = std::filesystem::relative(cc, rc, ec);
  if (ec)
    return false;
  if (rel.empty() || rel == std::filesystem::path("."))
    return true;
  for (const auto& seg : rel) {
    if (seg == "..")
      return false;
  }
  return true;
}

void gz_filter_scan_roots_skip_under_intermediate(const std::filesystem::path& cwd,
                                                  std::vector<std::filesystem::path>& roots,
                                                  const char* tool_label_for_warnings) {
  std::error_code ec_anchor;
  const std::filesystem::path anchor =
      std::filesystem::weakly_canonical(std::filesystem::absolute(cwd / ".intermediate"), ec_anchor);
  if (ec_anchor || anchor.empty())
    return;

  std::vector<std::filesystem::path> kept;
  kept.reserve(roots.size());
  for (const auto& r : roots) {
    std::error_code er;
    const std::filesystem::path ra = std::filesystem::weakly_canonical(std::filesystem::absolute(r), er);
    if (er)
      continue;
    if (gz_path_is_same_or_under(anchor, ra)) {
      std::cerr << tool_label_for_warnings << ": warning: skipping --scan root under .intermediate: "
                << to_posix_path_string(r) << "\n";
      continue;
    }
    kept.push_back(r);
  }
  if (kept.empty()) {
    std::cerr << tool_label_for_warnings << ": warning: all --scan roots were under .intermediate; using cwd only.\n";
    kept.push_back(cwd);
  }
  roots = std::move(kept);
}

void gz_dedupe_scan_roots_subsumed(std::vector<std::filesystem::path>& roots) {
  if (roots.size() <= 1)
    return;
  std::vector<std::filesystem::path> can;
  can.reserve(roots.size());
  for (const auto& r : roots) {
    std::error_code er;
    can.push_back(std::filesystem::weakly_canonical(std::filesystem::absolute(r), er));
  }
  std::vector<bool> drop(roots.size(), false);
  for (size_t i = 0; i < roots.size(); ++i) {
    for (size_t j = 0; j < roots.size(); ++j) {
      if (i == j) continue;
      if (can[i] == can[j] && j < i) {
        drop[i] = true;
        break;
      }
      if (i != j && can[i] != can[j] && gz_path_is_same_or_under(roots[j], roots[i])) {
        drop[i] = true;
        break;
      }
    }
  }
  std::vector<std::filesystem::path> out;
  out.reserve(roots.size());
  for (size_t k = 0; k < roots.size(); ++k) {
    if (!drop[k])
      out.push_back(roots[k]);
  }
  roots = std::move(out);
}

}  // namespace gz
