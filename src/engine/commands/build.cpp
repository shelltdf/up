#include "build.hpp"
#include "paths.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace up {

namespace {

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

}  // namespace

int cmd_build(const std::filesystem::path& cwd) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto opts = load_up_options(build_root, arch);
  const auto src_dir = build_root / arch;
  const auto inst = default_install_root(cwd) / arch;
  const std::string build_system =
      option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake");
  if (!equals_ci(build_system, "cmake") && !equals_ci(build_system, "ninja")) {
    std::cerr << "build: unsupported UP_TARGET_BUILD_SYSTEM=" << build_system << " (expected cmake/ninja)\n";
    return 3;
  }
  std::filesystem::create_directories(inst);
  const auto bin_dir = src_dir / "out";
  std::filesystem::create_directories(bin_dir);
  if (equals_ci(build_system, "ninja")) {
    if (!std::filesystem::exists(src_dir / "out" / "build.ninja")) {
      std::cerr << "build: run `up configure` first (missing " << (src_dir / "out" / "build.ninja") << ")\n";
      return 2;
    }
    std::ostringstream ninja_cmd;
    ninja_cmd << "ninja -C \"" << (src_dir / "out").string() << "\" install";
    std::cout << ninja_cmd.str() << "\n";
    const int code = std::system(ninja_cmd.str().c_str());
    if (code != 0) {
      std::cerr << "build: ninja failed with code " << code << "\n";
      return static_cast<unsigned>(code) > 255u ? 1 : code;
    }
    return 0;
  }
  if (!std::filesystem::exists(src_dir / "CMakeLists.txt")) {
    std::cerr << "build: run `up configure` first (missing " << (src_dir / "CMakeLists.txt") << ")\n";
    return 2;
  }
  const std::string cmake_generator = option_or(opts, "UP_CMAKE_GENERATOR", "");
  const bool use_debug = equals_ci(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"), "ON");
  const std::string config_name = use_debug ? "Debug" : "Release";

  std::ostringstream cmd;
  cmd << "cmake -S \"" << src_dir.string() << "\" -B \"" << bin_dir.string() << "\" -DCMAKE_INSTALL_PREFIX=\""
      << inst.string() << "\"";
  if (equals_ci(build_system, "ninja")) {
    cmd << " -G Ninja";
  } else if (!cmake_generator.empty()) {
    cmd << " -G \"" << cmake_generator << "\"";
  }

  for (const auto& kv : opts) {
    cmd << " -D" << kv.first << "=\"" << kv.second << "\"";
  }

  bool multi_config = contains_ci(cmake_generator, "visual studio") || contains_ci(cmake_generator, "multi-config");
#if defined(_WIN32)
  if (equals_ci(build_system, "cmake") && cmake_generator.empty())
    multi_config = true;  // default VS generator on Windows
#endif
  if (!multi_config)
    cmd << " -DCMAKE_BUILD_TYPE=" << config_name;

  cmd << " && cmake --build \"" << bin_dir.string() << "\"";
  if (multi_config)
    cmd << " --config " << config_name;
  cmd << " --target install";

  std::cout << cmd.str() << "\n";
  const int code = std::system(cmd.str().c_str());
  if (code != 0) {
    std::cerr << "build: cmake failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  return 0;
}

int cmd_run(const std::filesystem::path& cwd, const std::string& target_name) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto exe_dir = default_install_root(cwd) / arch / "bin";
  std::filesystem::path exe = exe_dir / target_name;
#if defined(_WIN32)
  if (exe.extension().empty())
    exe.replace_filename(exe.filename().string() + ".exe");
#endif
  if (!std::filesystem::exists(exe)) {
    std::cerr << "run: executable not found: " << exe << "\n";
    return 2;
  }
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "\"" << exe.string() << "\"";
#else
  cmd << "\"" << exe.string() << "\"";
#endif
  return std::system(cmd.str().c_str());
}

int cmd_test(const std::filesystem::path& cwd, const std::string& test_name) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto opts = load_up_options(build_root, arch);
  const std::string build_system =
      option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake");
  if (equals_ci(build_system, "ninja")) {
    const auto install_bin = default_install_root(cwd) / arch / "bin";
    if (!std::filesystem::exists(install_bin)) {
      std::cerr << "test: install tree missing; run `up build` first.\n";
      return 2;
    }
    std::vector<std::filesystem::path> tests;
    for (const auto& e : std::filesystem::directory_iterator(install_bin)) {
      if (!e.is_regular_file())
        continue;
      auto p = e.path();
#if defined(_WIN32)
      if (!equals_ci(p.extension().string(), ".exe"))
        continue;
#endif
      const std::string stem = p.stem().string();
      if (!test_name.empty()) {
        if (!equals_ci(stem, test_name))
          continue;
      } else if (!contains_ci(stem, "test")) {
        continue;
      }
      tests.push_back(p);
    }
    if (tests.empty()) {
      std::cerr << "test: no matching test executables in " << install_bin << "\n";
      return 2;
    }
    for (const auto& t : tests) {
      std::ostringstream cmd;
      cmd << "\"" << t.string() << "\"";
      const int code = std::system(cmd.str().c_str());
      if (code != 0)
        return static_cast<unsigned>(code) > 255u ? 1 : code;
    }
    return 0;
  }
  const bool use_debug = equals_ci(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"), "ON");
  const std::string config_name = use_debug ? "Debug" : "Release";
  const auto bin_dir = build_root / arch / "out";
  if (!std::filesystem::exists(bin_dir)) {
    std::cerr << "test: build tree missing; run `up build` first.\n";
    return 2;
  }
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "ctest --test-dir \"" << bin_dir.string() << "\" -C " << config_name << " --output-on-failure";
#else
  cmd << "ctest --test-dir \"" << bin_dir.string() << "\" --output-on-failure";
#endif
  if (!test_name.empty())
    cmd << " -R \"^" << test_name << "$\"";
  return std::system(cmd.str().c_str());
}

int cmd_pack(const std::filesystem::path& cwd) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto src = default_install_root(cwd) / arch;
  const auto dst_dir = default_pack_root(cwd) / arch;
  if (!std::filesystem::exists(src)) {
    std::cerr << "pack: install tree missing; run `up build` first (expected " << src << ")\n";
    return 2;
  }
  std::error_code ec;
  std::filesystem::create_directories(dst_dir, ec);
  if (ec) {
    std::cerr << "pack: cannot create " << dst_dir << ": " << ec.message() << "\n";
    return 3;
  }

#if defined(_WIN32)
  const auto archive = dst_dir / ("up-" + arch + ".zip");
  if (std::filesystem::exists(archive))
    std::filesystem::remove(archive, ec);
  std::ostringstream cmd;
  cmd << "powershell -NoProfile -Command \"Compress-Archive -Path '"
      << (src / "*").string() << "' -DestinationPath '" << archive.string() << "' -Force\"";
#else
  const auto archive = dst_dir / ("up-" + arch + ".tar.gz");
  if (std::filesystem::exists(archive))
    std::filesystem::remove(archive, ec);
  std::ostringstream cmd;
  cmd << "tar -czf \"" << archive.string() << "\" -C \"" << src.string() << "\" .";
#endif

  const int code = std::system(cmd.str().c_str());
  if (code != 0) {
    std::cerr << "pack: archive command failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  std::cout << "pack: " << src << " -> " << archive << "\n";
  return 0;
}

int cmd_project(const std::filesystem::path& cwd) {
  std::cout << "project: placeholder — migrate existing sources in " << cwd
            << " to package.xml + per-target subdirs with target.xml.\n";
  return 0;
}

}  // namespace up
