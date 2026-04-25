#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include <cctype>
#include <filesystem>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace up {

void import_qmake(const std::filesystem::path& pro_file, const std::filesystem::path& scan_root,
                  const std::filesystem::path& write_root, ImportedPackage& out, std::vector<std::string>& warnings,
                  std::string& error) {
  (void)scan_root;
  const std::string text = project_import::read_file_text(pro_file, error);
  if (!error.empty())
    return;

  std::string target = pro_file.stem().string();
  std::string tmpl = "app";
  std::vector<std::filesystem::path> abs;

  std::istringstream iss(text);
  std::string line;
  while (std::getline(iss, line)) {
    auto trim = [](std::string s) {
      size_t a = 0;
      while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
      while (s.size() > a && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
      return s;
    };
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;
    std::smatch m;
    if (std::regex_match(line, m, std::regex(R"(TARGET\s*=\s*(.+))", std::regex::icase)))
      target = trim(m[1].str());
    else if (std::regex_match(line, m, std::regex(R"(TEMPLATE\s*=\s*(.+))", std::regex::icase)))
      tmpl = trim(m[1].str());
    else if (std::regex_match(line, m, std::regex(R"(SOURCES\s*\+?=\s*(.+))", std::regex::icase)) ||
             std::regex_match(line, m, std::regex(R"(SOURCES\s*=\s*(.+))", std::regex::icase))) {
      std::istringstream ws(m[1].str());
      std::string tok;
      while (ws >> tok) {
        tok = trim(tok);
        if (tok.empty() || tok[0] == '$')
          continue;
        if (!project_import::looks_like_source_token(tok))
          continue;
        std::filesystem::path ap = pro_file.parent_path() / tok;
        std::error_code ec;
        ap = std::filesystem::weakly_canonical(ap, ec);
        if (!ec && std::filesystem::exists(ap))
          abs.push_back(ap);
      }
    }
  }

  std::string ty = "executable";
  {
    std::string u = tmpl;
    for (char& c : u)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (u == "lib" || u == "subdirs")
      ty = "static_library";
  }

  const std::string tname = project_import::sanitize_id(target);
  std::map<std::string, int> bucket_claims;
  const std::string bucket =
      project_import::resolve_target_xml_bucket(write_root, pro_file.parent_path(), tname, abs, bucket_claims);
  project_import::push_target(out, write_root, bucket, tname, ty, abs, warnings);
  if (out.targets.empty()) {
    warnings.push_back("qmake: no SOURCES found; falling back.");
    error.clear();
  } else
    error.clear();
}

}  // namespace up
