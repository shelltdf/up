#include "cli_dispatch.hpp"

#include "build.hpp"
#include "cli_paths.hpp"
#include "cli_verbose.hpp"
#include "commands_common.hpp"
#include "configure.hpp"
#include "lang.hpp"
#include "list.hpp"
#include "pack.hpp"
#include "paths.hpp"
#include "run.hpp"
#include "spec.hpp"
#if UP_ENABLE_PROJECT
#include "project.hpp"
#endif
#include "test.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void print_usage() { up::lang::print_usage(std::cout); }

void init_console_utf8_best_effort() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
}

bool env_verbose_enabled() {
  const char* e = std::getenv("UP_VERBOSE");
  if (!e || e[0] == '\0')
    return false;
  const std::string s = up::lower_ascii(e);
  return s == "1" || s == "true" || s == "yes" || s == "on";
}

}  // namespace

int run_up_cli(int argc, char** argv) {
  init_console_utf8_best_effort();
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);

  bool verbose_flag = env_verbose_enabled();
  {
    std::vector<std::string> filtered;
    filtered.reserve(args.size());
    for (auto& a : args) {
      if (a == "--verbose" || a == "-v") {
        verbose_flag = true;
        continue;
      }
      filtered.push_back(std::move(a));
    }
    args = std::move(filtered);
  }
  up::set_cli_verbose(verbose_flag);

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
    if (cache_leaf.has_value() && !up::exe::intermediate_leaf_name_ok(*cache_leaf)) {
      std::cerr << "print-build-dir-name: invalid --build-dir-name\n";
      return 2;
    }
    const std::string leaf = cache_leaf.value_or("default");
    std::map<std::string, std::string> merged =
        up::load_up_options_from_build_dir(up::exe::build_dir_from_leaf(cwd, leaf));
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
    if (build_dir_name.has_value() && !up::exe::intermediate_leaf_name_ok(*build_dir_name)) {
      std::cerr << "configure: invalid --build-dir-name (use a single directory name under .intermediate/build)\n";
      return 2;
    }
    up::ConfigureRequest cr;
    cr.cwd = cwd;
    cr.scan_roots = std::move(scans);
    cr.opt_kvs = std::move(opts);
    cr.build_dir_name_override = std::move(build_dir_name);
    return up::run_configure(cr);
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
    if (!up::exe::intermediate_leaf_name_ok(leaf)) {
      std::cerr << "build: invalid --build-dir-name\n";
      return 2;
    }
    return up::cmd_build(cwd, up::exe::build_dir_from_leaf(cwd, leaf));
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
    if (!up::exe::intermediate_leaf_name_ok(leaf)) {
      std::cerr << "run: invalid --install-dir-name\n";
      return 2;
    }
    if (target.empty()) {
      std::cerr << "run: missing target name\n";
      return 1;
    }
    return up::cmd_run(up::exe::install_dir_from_leaf(cwd, leaf), target);
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
    if (!up::exe::intermediate_leaf_name_ok(leaf)) {
      std::cerr << "test: invalid --install-dir-name\n";
      return 2;
    }
    return up::cmd_test(up::exe::install_dir_from_leaf(cwd, leaf), test_name);
  }
  if (cmd == "spec") {
    std::vector<std::string> sargs;
    for (size_t i = 1; i < args.size(); ++i)
      sargs.push_back(args[i]);
    return up::cmd_spec(sargs);
  }
  if (cmd == "list") {
    std::vector<std::string> largs;
    for (size_t i = 1; i < args.size(); ++i)
      largs.push_back(args[i]);
    up::ListRequest lr;
    int pr = up::parse_list_cli_args(cwd, largs, lr);
    if (pr != 0)
      return pr;
    return up::run_list(lr);
  }
  if (cmd == "pack") {
    std::vector<std::filesystem::path> install_dirs;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--install-dir-name" && i + 1 < args.size()) {
        if (!up::exe::intermediate_leaf_name_ok(args[i + 1])) {
          std::cerr << "pack: invalid --install-dir-name: " << args[i + 1] << "\n";
          return 2;
        }
        install_dirs.push_back(up::exe::install_dir_from_leaf(cwd, args[i + 1]));
        ++i;
      }
    }
    return up::cmd_pack(cwd, install_dirs);
  }
#if UP_ENABLE_PROJECT
  if (cmd == "project") {
    std::vector<std::string> pargs;
    for (size_t i = 1; i < args.size(); ++i)
      pargs.push_back(args[i]);
    std::error_code ec_proj;
    const auto cwd_for_project = std::filesystem::current_path(ec_proj);
    return up::cmd_project(ec_proj ? cwd : cwd_for_project, pargs);
  }
#else
  if (cmd == "project") {
    std::cerr << "up: subcommand \"project\" is disabled in this build. Rebuild with -DUP_ENABLE_PROJECT=ON.\n";
    return 2;
  }
#endif

  std::cerr << "unknown command: " << cmd << "\n";
  print_usage();
  return 1;
}
