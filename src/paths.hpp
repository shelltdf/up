#pragma once

#include <filesystem>
#include <string>

namespace up {

std::filesystem::path default_cmake_build_root(const std::filesystem::path& cwd);
std::filesystem::path default_build_root(const std::filesystem::path& cwd, const std::string& build_system);
std::filesystem::path default_install_root(const std::filesystem::path& cwd);
std::filesystem::path default_pack_root(const std::filesystem::path& cwd);
std::string detect_arch_tag();
std::string detect_host_system_tag();
std::string detect_host_toolchain_tag();
std::string normalize_cpu_arch_tag(const std::string& cpu);
std::string compose_arch_tag(const std::string& system,
                             const std::string& cpu,
                             const std::string& build_system,
                             const std::string& toolchain,
                             const std::string& link_mode,
                             const std::string& config_mode,
                             const std::string& crt_mode = "");

}  // namespace up
