#include "build.hpp"
#include "commands_common.hpp"
#include "configure.hpp"
#include "lang.hpp"
#include "pack.hpp"
#include "paths.hpp"
#include "project.hpp"
#include "run.hpp"
#include "test.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

void print_usage() { up::lang::print_usage(std::cout); }

// A single directory name under .intermediate/build or .intermediate/install (no path separators).
bool intermediate_leaf_name_ok(const std::string& s) {
  if (s.empty() || s == "." || s == "..")
    return false;
  if (s.find("..") != std::string::npos)
    return false;
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
    if (c == '/' || c == '\\')
      return false;
#if defined(_WIN32)
    if (c == ':' || c == '<' || c == '>' || c == '|' || c == '?' || c == '*')
      return false;
#endif
  }
  return true;
}

std::filesystem::path build_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf) {
  return up::default_build_root(cwd, "cmake") / std::filesystem::u8path(leaf);
}

std::filesystem::path install_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf) {
  return up::default_install_root(cwd) / std::filesystem::u8path(leaf);
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);

  if (args.empty()) {
    print_usage();
    return 0;
  }

  const std::string cmd = args[0];
  if (cmd == "--help" || cmd == "-h" || cmd == "help") {
    print_usage();
    return 0;
  }
  if (cmd == "print-build-dir-name") {
    std::vector<std::string> opts;
    std::optional<std::string> cache_leaf;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--opt" && i + 1 < args.size()) {
        opts.push_back(args[i + 1]);
        ++i;
      } else if (args[i].rfind("--opt=", 0) == 0) {
        opts.push_back(args[i].substr(6));
      } else if (args[i] == "--build-dir-name" && i + 1 < args.size()) {
        cache_leaf = args[i + 1];
        ++i;
      }
    }
    if (cache_leaf.has_value() && !intermediate_leaf_name_ok(*cache_leaf)) {
      std::cerr << "print-build-dir-name: invalid --build-dir-name\n";
      return 2;
    }
    const std::string leaf = cache_leaf.value_or("default");
    std::map<std::string, std::string> merged = up::load_up_options_from_build_dir(build_dir_from_leaf(cwd, leaf));
    for (const auto& kv : opts) {
      const auto pos = kv.find('=');
      if (pos == std::string::npos || pos == 0)
        continue;
      const std::string k = kv.substr(0, pos);
      if (k.rfind("UP_", 0) != 0)
        continue;
      merged[k] = kv.substr(pos + 1);
    }
    std::cout << up::arch_from_options(merged) << std::endl;
    return 0;
  }
  if (cmd == "configure") {
    std::vector<std::string> scans;
    std::vector<std::string> opts;
    std::optional<std::string> build_dir_name;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--scan" && i + 1 < args.size()) {
        scans.push_back(args[i + 1]);
        ++i;
      } else if (args[i] == "--opt" && i + 1 < args.size()) {
        opts.push_back(args[i + 1]);
        ++i;
      } else if (args[i].rfind("--opt=", 0) == 0) {
        opts.push_back(args[i].substr(6));
      } else if (args[i] == "--build-dir-name" && i + 1 < args.size()) {
        build_dir_name = args[i + 1];
        ++i;
      }
    }
    if (build_dir_name.has_value() && !intermediate_leaf_name_ok(*build_dir_name)) {
      std::cerr << "configure: invalid --build-dir-name (use a single directory name under .intermediate/build)\n";
      return 2;
    }
    return up::cmd_configure(cwd, scans, opts, build_dir_name);
  }
  if (cmd == "build") {
    std::string leaf;
    bool have = false;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--build-dir-name" && i + 1 < args.size()) {
        leaf = args[i + 1];
        have = true;
        ++i;
      }
    }
    if (!have) {
      std::cerr << "build: missing required --build-dir-name <name>\n";
      return 2;
    }
    if (!intermediate_leaf_name_ok(leaf)) {
      std::cerr << "build: invalid --build-dir-name\n";
      return 2;
    }
    return up::cmd_build(cwd, build_dir_from_leaf(cwd, leaf));
  }
  if (cmd == "run") {
    std::string leaf;
    bool have_id = false;
    std::string target;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--install-dir-name" && i + 1 < args.size()) {
        leaf = args[i + 1];
        have_id = true;
        ++i;
      } else if (target.empty()) {
        target = args[i];
      }
    }
    if (!have_id) {
      std::cerr << "run: missing required --install-dir-name <name>\n";
      return 2;
    }
    if (!intermediate_leaf_name_ok(leaf)) {
      std::cerr << "run: invalid --install-dir-name\n";
      return 2;
    }
    if (target.empty()) {
      std::cerr << "run: missing target name\n";
      return 1;
    }
    return up::cmd_run(install_dir_from_leaf(cwd, leaf), target);
  }
  if (cmd == "test") {
    std::string leaf;
    bool have_id = false;
    std::string test_name;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--install-dir-name" && i + 1 < args.size()) {
        leaf = args[i + 1];
        have_id = true;
        ++i;
      } else if (test_name.empty()) {
        test_name = args[i];
      }
    }
    if (!have_id) {
      std::cerr << "test: missing required --install-dir-name <name>\n";
      return 2;
    }
    if (!intermediate_leaf_name_ok(leaf)) {
      std::cerr << "test: invalid --install-dir-name\n";
      return 2;
    }
    return up::cmd_test(install_dir_from_leaf(cwd, leaf), test_name);
  }
  if (cmd == "pack") {
    std::vector<std::filesystem::path> install_dirs;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--install-dir-name" && i + 1 < args.size()) {
        if (!intermediate_leaf_name_ok(args[i + 1])) {
          std::cerr << "pack: invalid --install-dir-name: " << args[i + 1] << "\n";
          return 2;
        }
        install_dirs.push_back(install_dir_from_leaf(cwd, args[i + 1]));
        ++i;
      }
    }
    return up::cmd_pack(cwd, install_dirs);
  }
  if (cmd == "project") {
    std::vector<std::string> pargs;
    for (size_t i = 1; i < args.size(); ++i)
      pargs.push_back(args[i]);
    return up::cmd_project(cwd, pargs);
  }

  std::cerr << "unknown command: " << cmd << "\n";
  print_usage();
  return 1;
}
