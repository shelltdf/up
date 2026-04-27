#include "paths.hpp"

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

}  // namespace gz
