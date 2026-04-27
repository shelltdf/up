#include "list.hpp"

#include "dom_model.hpp"
#include "path_check.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>
#include <set>
#include <vector>

namespace gz {
namespace {

void collect_desc_files(const std::filesystem::path& root,
                        std::vector<std::filesystem::path>& packages,
                        std::vector<std::filesystem::path>& targets) {
  if (!std::filesystem::exists(root))
    return;
  for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied), end;
       it != end; ++it) {
    if (it->is_directory() && it->path().filename() == ".intermediate") {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file())
      continue;
    const auto p = it->path();
    if (p.filename() == "package.xml")
      packages.push_back(p);
    else if (p.filename() == "target.xml")
      targets.push_back(p);
  }
}

bool require_ascii_path(const std::filesystem::path& p) {
  if (path_has_non_ascii(p)) {
    std::cerr << "list: path contains non-ASCII characters (not supported): " << to_posix_path_string(p) << "\n";
    return false;
  }
  return true;
}

}  // namespace

int parse_list_cli_args(const std::filesystem::path& cwd, const std::vector<std::string>& args, ListRequest& out) {
  out = ListRequest{};
  out.cwd = cwd;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--xml" && i + 1 < args.size()) {
      out.xml_out = std::filesystem::path(args[i + 1]);
      ++i;
    } else if (args[i] == "--json" && i + 1 < args.size()) {
      out.json_out = std::filesystem::path(args[i + 1]);
      ++i;
    } else if (args[i] == "--quiet") {
      out.quiet = true;
    } else if (args[i] == "--scan" && i + 1 < args.size()) {
      out.roots.push_back(std::filesystem::path(args[i + 1]).lexically_normal());
      ++i;
    } else if (args[i] == "--format" && i + 1 < args.size()) {
      out.format = args[i + 1];
      ++i;
    } else {
      std::cerr << "list: unknown option: " << args[i] << "\n";
      return 2;
    }
  }
  if (out.format != "tree" && out.format != "json" && out.format != "xml") {
    std::cerr << "list: invalid --format value (expected tree|json|xml)\n";
    return 2;
  }
  if (out.format == "xml" && !out.json_out.empty() && !out.quiet) {
    std::cerr << "list: warning: stdout uses XML while --json writes file output\n";
  }
  if (out.format == "json" && !out.xml_out.empty() && !out.quiet) {
    std::cerr << "list: warning: stdout uses JSON while --xml writes file output\n";
  }
  if (out.roots.empty()) {
    out.roots.push_back(cwd);
  } else {
    bool has_cwd = false;
    std::error_code ec;
    const auto cwd_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(cwd), ec);
    for (const auto& r : out.roots) {
      std::error_code rc;
      const auto r_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(r), rc);
      if (!ec && !rc && r_abs == cwd_abs) {
        has_cwd = true;
        break;
      }
    }
    if (!has_cwd)
      out.roots.push_back(cwd);
  }
  return 0;
}

int run_list(const ListRequest& req) {
  const std::filesystem::path& cwd = req.cwd;
  std::vector<std::filesystem::path> package_files;
  std::vector<std::filesystem::path> target_files;
  for (const auto& r : req.roots)
    collect_desc_files(r, package_files, target_files);
  {
    auto dedup = [](std::vector<std::filesystem::path>& v) {
      std::set<std::string> seen;
      std::vector<std::filesystem::path> out;
      out.reserve(v.size());
      for (const auto& p : v) {
        const std::string key = std::filesystem::absolute(p).lexically_normal().generic_string();
        if (seen.insert(key).second)
          out.push_back(p);
      }
      v = std::move(out);
    };
    dedup(package_files);
    dedup(target_files);
  }
  if (package_files.empty() && target_files.empty()) {
    std::cerr << "list: no package.xml or target.xml found.\n";
    return 2;
  }
  for (const auto& p : package_files) {
    if (!require_ascii_path(p))
      return 6;
  }
  for (const auto& p : target_files) {
    if (!require_ascii_path(p))
      return 6;
  }

  DomDocument doc;
  std::string err;
  BuildDomOptions opt;
  opt.cwd = cwd;
  opt.package_files = std::move(package_files);
  opt.target_files = std::move(target_files);
  if (!DomDocument::build(opt, doc, err)) {
    std::cerr << "list: " << err << "\n";
    return 3;
  }

  std::filesystem::path xml_out = req.xml_out;
  std::filesystem::path json_out = req.json_out;
  const bool stdout_json = req.format == "json";
  const bool stdout_xml = req.format == "xml";
  const bool stdout_tree = !stdout_json && !stdout_xml;
  if (stdout_tree && !req.quiet)
    doc.print_tree(std::cout);
  if (!xml_out.empty()) {
    if (xml_out.is_relative())
      xml_out = std::filesystem::absolute(cwd / xml_out).lexically_normal();
    if (!doc.write_xml_file(xml_out, err)) {
      std::cerr << "list: " << err << "\n";
      return 4;
    }
    if (!req.quiet)
      std::cout << "dom xml exported: " << to_posix_path_string(xml_out) << "\n";
  }
  if (!json_out.empty()) {
    if (json_out.is_relative())
      json_out = std::filesystem::absolute(cwd / json_out).lexically_normal();
    if (!doc.write_json_file(json_out, err)) {
      std::cerr << "list: " << err << "\n";
      return 4;
    }
    if (!req.quiet)
      std::cout << "dom json exported: " << to_posix_path_string(json_out) << "\n";
  }
  if (stdout_json) {
    if (!doc.write_json(std::cout)) {
      std::cerr << "list: failed to print dom json\n";
      return 4;
    }
  }
  if (stdout_xml && !req.quiet) {
    if (!doc.write_xml(std::cout)) {
      std::cerr << "list: failed to print dom xml\n";
      return 4;
    }
  }
  return 0;
}

int cmd_list(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
  ListRequest req;
  const int pr = parse_list_cli_args(cwd, args, req);
  if (pr != 0)
    return pr;
  return run_list(req);
}

}  // namespace gz
