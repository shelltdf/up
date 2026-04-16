#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include <filesystem>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace up {
namespace {

void collect_sources_flat(const std::filesystem::path& root, int max_depth, std::vector<std::filesystem::path>& out_files) {
  if (max_depth < 0)
    return;
  std::error_code ec;
  for (const auto& ent : std::filesystem::directory_iterator(root, ec)) {
    if (ec)
      break;
    if (ent.is_directory()) {
      const std::string nm = ent.path().filename().string();
      if (project_import::skip_dir_name(nm))
        continue;
      collect_sources_flat(ent.path(), max_depth - 1, out_files);
    } else if (ent.is_regular_file()) {
      const auto ext = ent.path().extension().string();
      if (project_import::is_src_ext(ext))
        out_files.push_back(ent.path());
    }
  }
}

bool file_contains_main(const std::filesystem::path& p) {
  std::string err;
  const std::string t = project_import::read_file_text(p, err);
  if (t.empty())
    return false;
  static const std::regex main_re(R"(\bmain\s*\()");
  return std::regex_search(t, main_re);
}

}  // namespace

void import_source_fallback(const std::filesystem::path& scan_root, const std::filesystem::path& write_root,
                            ImportedPackage& out, std::vector<std::string>& warnings, std::string& error) {
  std::vector<std::filesystem::path> files;
  collect_sources_flat(scan_root, 6, files);
  if (files.empty()) {
    error = "no .c/.cpp sources found under scan root";
    return;
  }
  int mains = 0;
  for (const auto& f : files) {
    if (file_contains_main(f))
      ++mains;
  }
  std::string ty = "static_library";
  if (mains == 1)
    ty = "executable";
  else if (mains > 1) {
    warnings.push_back("multiple translation units contain main(); using static_library — edit target.xml if needed.");
    ty = "static_library";
  }

  const std::string tname = project_import::sanitize_id(out.package_name);
  std::filesystem::path common = files[0].parent_path();
  for (size_t i = 1; i < files.size(); ++i) {
    const auto d = files[i].parent_path();
    while (!common.empty()) {
      const auto rp = project_import::try_relative(common, d);
      bool ok = false;
      if (rp) {
        ok = true;
        for (const auto& seg : *rp) {
          if (seg == "..") {
            ok = false;
            break;
          }
        }
      }
      if (ok)
        break;
      common = common.parent_path();
    }
  }
  std::map<std::string, int> bucket_claims;
  const std::string bucket =
      project_import::resolve_target_xml_bucket(write_root, common, tname, files, bucket_claims);
  project_import::push_target(out, write_root, bucket, tname, ty, files, warnings);
  if (out.targets.empty()) {
    error = "failed to build target from sources";
    return;
  }
  warnings.push_back("Heuristic import: CMake/autotools/qmake not matched or yielded no targets; bundled sources into one target.");
  error.clear();
}

}  // namespace up
