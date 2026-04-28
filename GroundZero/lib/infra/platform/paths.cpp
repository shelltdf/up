#include "paths.hpp"

#include <regex>

namespace gz {

namespace {

std::filesystem::path intermediate_root(const std::filesystem::path& cwd) {
  return cwd / ".intermediate";
}

}  // namespace

std::filesystem::path default_cmake_build_root(const std::filesystem::path& cwd) {
  return intermediate_root(cwd) / "build";
}

std::filesystem::path default_build_root(const std::filesystem::path& cwd, const std::string& build_system) {
  (void)build_system;
  return intermediate_root(cwd) / "build";
}

std::filesystem::path default_install_root(const std::filesystem::path& cwd) {
  return intermediate_root(cwd) / "install";
}

std::filesystem::path default_pack_root(const std::filesystem::path& cwd) {
  return intermediate_root(cwd) / "pack";
}

std::string detect_arch_tag() {
#if defined(_WIN64)
  return "x86_64";
#elif defined(_WIN32)
  return "x86";
#elif defined(__aarch64__)
  return "arm64";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__arm__)
  return "arm32";
#else
  return "unknown";
#endif
}

std::string detect_host_system_tag() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#elif defined(__ANDROID__)
  return "android";
#else
  return "unknown";
#endif
}

std::string detect_host_toolchain_tag() {
#if defined(_MSC_VER)
  return "msvc";
#elif defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#else
  return "unknown";
#endif
}

std::string normalize_cpu_arch_tag(const std::string& cpu) {
  std::string v = cpu;
  for (char& c : v) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  if (v.empty())
    return detect_arch_tag();
  if (v == "x86")
    return "x86";
  if (v == "x64" || v == "x86_64" || v == "amd64")
    return "x86_64";
  if (v == "arm64" || v == "aarch64")
    return "arm64";
  if (v == "arm" || v == "arm32")
    return "arm32";
  return v;
}

std::string compose_arch_tag(const std::string& system,
                             const std::string& cpu,
                             const std::string& build_system,
                             const std::string& toolchain,
                             const std::string& link_mode,
                             const std::string& config_mode,
                             const std::string& crt_mode) {
  std::string out = system + "_" + cpu + "_" + build_system + "_" + toolchain + "_" + link_mode;
  if (!crt_mode.empty())
    out += "_crt-" + crt_mode;
  out += "_" + config_mode;
  return out;
}

namespace {

bool system_cpu_from_prefix(const std::string& left, std::string& out_system, std::string& out_cpu) {
  static const char* k_sys[] = {"windows", "linux", "macos", "android", "darwin", "unknown", nullptr};
  for (int i = 0; k_sys[i]; ++i) {
    const std::string p = std::string(k_sys[i]) + "_";
    if (left.size() > p.size() && left.rfind(p, 0) == 0) {
      out_system = k_sys[i];
      out_cpu = left.substr(p.size());
      return !out_cpu.empty();
    }
  }
  return false;
}

}  // namespace

bool try_decompose_compose_arch_tag(const std::string& leaf,
                                    std::string& out_os,
                                    std::string& out_cpu,
                                    std::string& out_build_system,
                                    std::string& out_toolchain,
                                    std::string& out_link,
                                    std::string& out_config,
                                    std::string& out_crt) {
  out_os.clear();
  out_cpu.clear();
  out_build_system.clear();
  out_toolchain.clear();
  out_link.clear();
  out_config.clear();
  out_crt.clear();
  if (leaf.empty())
    return false;
  std::string s = leaf;
  if (s.size() >= 8) {
    static const char suf[] = "_release";
    if (s.rfind(suf) == s.size() - 8) {
      out_config = "release";
      s.resize(s.size() - 8);
    }
  }
  if (out_config.empty() && s.size() >= 6) {
    static const char suf[] = "_debug";
    if (s.rfind(suf) == s.size() - 6) {
      out_config = "debug";
      s.resize(s.size() - 6);
    }
  }
  if (out_config.empty())
    return false;

  std::string pre_toolchain, link, crt;
  {
    const std::regex re(R"(^(.+)_((?:static|dynamic))_crt-(.+)$)");
    std::smatch m;
    if (std::regex_match(s, m, re)) {
      pre_toolchain = m[1].str();
      link = m[2].str();
      crt = m[3].str();
    } else {
      const std::regex re2(R"(^(.+)_((?:static|dynamic))$)");
      std::smatch m2;
      if (std::regex_match(s, m2, re2)) {
        pre_toolchain = m2[1].str();
        link = m2[2].str();
        crt.clear();
      } else
        return false;
    }
  }

  out_link = std::move(link);
  out_crt = std::move(crt);
  const size_t pc = pre_toolchain.find("_cmake_");
  const size_t pn = pre_toolchain.find("_ninja_");
  if (pc != std::string::npos && (pn == std::string::npos || pc < pn)) {
    out_build_system = "cmake";
    out_toolchain = pre_toolchain.substr(pc + 7);
    if (out_toolchain.empty())
      return false;
    if (!system_cpu_from_prefix(pre_toolchain.substr(0, pc), out_os, out_cpu))
      return false;
    return true;
  }
  if (pn != std::string::npos) {
    out_build_system = "ninja";
    out_toolchain = pre_toolchain.substr(pn + 7);
    if (out_toolchain.empty())
      return false;
    if (!system_cpu_from_prefix(pre_toolchain.substr(0, pn), out_os, out_cpu))
      return false;
    return true;
  }
  return false;
}

}  // namespace gz
