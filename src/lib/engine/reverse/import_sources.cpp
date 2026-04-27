#include "reverse_import_internal.hpp"
#include "reverse_import_common.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace up {
namespace {

void collect_sources_flat(const std::filesystem::path& root, int max_depth, std::vector<std::filesystem::path>& out_files) {
  if (max_depth < 0)
    return;
  std::error_code ec;
  std::filesystem::directory_iterator it(root, ec), end;
  if (ec)
    return;
  for (; it != end; it.increment(ec)) {
    if (ec)
      break;
    const auto ent = *it;
    std::error_code ec2;
    if (ent.is_directory(ec2)) {
      if (ec2)
        continue;
      std::string nm;
      try {
        nm = ent.path().filename().string();
      } catch (...) {
        // Skip entries whose native path cannot be converted to std::string safely.
        continue;
      }
      if (reverse_import::skip_dir_name(nm))
        continue;
      collect_sources_flat(ent.path(), max_depth - 1, out_files);
    } else if (ent.is_regular_file(ec2)) {
      if (ec2)
        continue;
      std::string ext;
      try {
        ext = ent.path().extension().string();
      } catch (...) {
        continue;
      }
      if (reverse_import::is_src_ext(ext))
        out_files.push_back(ent.path());
    }
  }
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
  // Keep fallback resilient for large/unusual source trees: avoid content scanning heuristics.
  const std::string ty = "static_library";

  const std::string tname = reverse_import::sanitize_id(out.package_name);
  std::filesystem::path common = files[0].parent_path();
  for (size_t i = 1; i < files.size(); ++i) {
    const auto d = files[i].parent_path();
    while (!common.empty()) {
      const auto rp = reverse_import::try_relative(common, d);
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
      reverse_import::resolve_target_xml_bucket(write_root, common, tname, files, bucket_claims);
  reverse_import::push_target(out, write_root, bucket, tname, ty, files, warnings);
  if (out.targets.empty()) {
    error = "failed to build target from sources";
    return;
  }
  warnings.push_back(
      "Heuristic import: CMake/autotools/qmake not matched or yielded no targets; bundled sources into one static_library target.");
  error.clear();
}

}  // namespace up
