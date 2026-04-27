#include "reverse_import_internal.hpp"
#include "reverse_import_common.hpp"

#include <filesystem>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace up {

void import_autotools(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, ImportedPackage& out,
                      std::vector<std::string>& warnings, std::string& error) {
  const std::filesystem::path mf = scan_root / "Makefile.am";
  const std::string text = reverse_import::read_file_text(mf, error);
  if (!error.empty())
    return;

  std::vector<std::string> bins;
  {
    std::smatch m;
    std::regex bin_re(R"(^\s*bin_PROGRAMS\s*=\s*(.+)\s*$)");
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
      if (std::regex_match(line, m, bin_re)) {
        std::istringstream ws(m[1].str());
        std::string w;
        while (ws >> w) {
          if (!w.empty() && w.back() == '\\')
            w.pop_back();
          if (!w.empty())
            bins.push_back(w);
        }
      }
    }
  }

  std::map<std::string, int> bucket_claims;
  for (const auto& prog : bins) {
    const std::string var = prog + "_SOURCES";
    std::regex src_re("^\\s*" + var + "\\s*=\\s*(.+)\\s*$");
    std::smatch m;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
      if (!std::regex_match(line, m, src_re))
        continue;
      std::vector<std::filesystem::path> abs;
      std::istringstream ws(m[1].str());
      std::string tok;
      while (ws >> tok) {
        if (!tok.empty() && tok.back() == '\\')
          tok.pop_back();
        if (tok.empty() || tok[0] == '$')
          continue;
        if (!reverse_import::looks_like_source_token(tok))
          continue;
        std::filesystem::path ap = scan_root / tok;
        std::error_code ec;
        ap = std::filesystem::weakly_canonical(ap, ec);
        if (!ec && std::filesystem::exists(ap))
          abs.push_back(ap);
      }
      const std::string tname = reverse_import::sanitize_id(prog);
      const std::string bucket =
          reverse_import::resolve_target_xml_bucket(write_root, scan_root, tname, abs, bucket_claims);
      reverse_import::push_target(out, write_root, bucket, tname, "executable", abs, warnings);
      break;
    }
  }

  if (out.targets.empty())
    warnings.push_back("Makefile.am: no bin_PROGRAMS + *_SOURCES parsed; falling back to source scan.");
  error.clear();
}

}  // namespace up
