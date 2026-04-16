#include "build.hpp"
#include "configure.hpp"
#include "lang.hpp"
#include "pack.hpp"
#include "project.hpp"
#include "run.hpp"
#include "test.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage() { up::lang::print_usage(std::cout); }

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
  if (cmd == "configure") {
    std::vector<std::string> scans;
    std::vector<std::string> opts;
    for (size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--scan" && i + 1 < args.size()) {
        scans.push_back(args[i + 1]);
        ++i;
      } else if (args[i] == "--opt" && i + 1 < args.size()) {
        opts.push_back(args[i + 1]);
        ++i;
      } else if (args[i].rfind("--opt=", 0) == 0) {
        opts.push_back(args[i].substr(6));
      }
    }
    return up::cmd_configure(cwd, scans, opts);
  }
  if (cmd == "build")
    return up::cmd_build(cwd);
  if (cmd == "run") {
    if (args.size() < 2) {
      std::cerr << "run: missing target name\n";
      return 1;
    }
    return up::cmd_run(cwd, args[1]);
  }
  if (cmd == "test")
    return up::cmd_test(cwd, args.size() > 1 ? args[1] : "");
  if (cmd == "pack")
    return up::cmd_pack(cwd);
  if (cmd == "project")
    return up::cmd_project(cwd);

  std::cerr << "unknown command: " << cmd << "\n";
  print_usage();
  return 1;
}
