#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace up {

void import_meson_basic(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, ImportedPackage& out,
                        std::vector<std::string>& warnings, std::string& error) {
  (void)write_root;
  (void)out;
  const std::filesystem::path mf = scan_root / "meson.build";
  const std::string text = project_import::read_file_text(mf, error);
  if (!error.empty())
    return;
  std::regex ex_re(R"(\bexecutable\s*\(\s*['\"]([^'\"]+)['\"])");
  std::smatch m;
  std::string::const_iterator it(text.cbegin());
  while (std::regex_search(it, text.cend(), m, ex_re)) {
    const std::string name = m[1].str();
    warnings.push_back("meson: detected executable `" + name + "` — sources not extracted (heuristic); using source fallback.");
    it = m.suffix().first;
  }
  error.clear();
}

}  // namespace up
