#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace up {

struct ListRequest {
  std::filesystem::path cwd;
  std::filesystem::path xml_out;
  std::filesystem::path json_out;
  std::vector<std::filesystem::path> roots;
  bool quiet = false;
  std::string format = "tree";
};

// Parses CLI tokens after `up list` (not argv[0]). On success returns 0 and fills `out` (including `out.cwd`).
int parse_list_cli_args(const std::filesystem::path& cwd, const std::vector<std::string>& args, ListRequest& out);

int run_list(const ListRequest& req);

int cmd_list(const std::filesystem::path& cwd, const std::vector<std::string>& args);

}  // namespace up
