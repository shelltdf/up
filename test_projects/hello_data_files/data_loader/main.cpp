#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct FileCheck {
  std::string name;
  fs::path path;
};

std::string first_line_of(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::string line;
  std::getline(in, line);
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return line;
}

fs::path resolve_data_root(const fs::path& exe_path) {
  // Installed run: <prefix>/bin/data_loader.exe -> <prefix>/assets/hello_data_files
  fs::path data_root = exe_path.parent_path().parent_path() / "assets" / "hello_data_files";
  if (fs::exists(data_root)) {
    return data_root;
  }
  // Backward compatibility with previous include-based demo install path.
  data_root = exe_path.parent_path().parent_path() / "include" / "hello_data_files";
  if (fs::exists(data_root)) {
    return data_root;
  }

  // CTest run from build tree: .../.intermediate/build/<arch>/out/Release/data_loader.exe
  // -> .../.intermediate/install/<arch>/assets/hello_data_files
  for (fs::path p = exe_path.parent_path(); !p.empty(); p = p.parent_path()) {
    if (p.filename() == "out" && !p.parent_path().empty()) {
      const fs::path arch_dir = p.parent_path();
      const fs::path build_dir = arch_dir.parent_path();
      if (!build_dir.empty() && build_dir.filename() == "build") {
        data_root = build_dir.parent_path() / "install" / arch_dir.filename() / "assets" / "hello_data_files";
        if (fs::exists(data_root)) {
          return data_root;
        }
        data_root = build_dir.parent_path() / "install" / arch_dir.filename() / "include" / "hello_data_files";
        if (fs::exists(data_root)) {
          return data_root;
        }
      }
    }
    if (p == p.parent_path()) {
      break;
    }
  }
  return data_root;
}

int main(int argc, char** argv) {
  const fs::path exe_path = (argc > 0 && argv[0] != nullptr) ? fs::absolute(argv[0]) : fs::current_path();
  const fs::path data_root = resolve_data_root(exe_path);

  std::vector<FileCheck> files = {
      {"xml", data_root / "sample.xml"},
      {"json", data_root / "sample.json"},
      {"svg", data_root / "logo.svg"},
  };

  std::cout << "[data_loader] data_root=" << data_root.string() << "\n";

  bool ok = true;
  for (const auto& f : files) {
    const bool exists = fs::exists(f.path);
    std::cout << "[data_loader] " << f.name << " exists=" << (exists ? "true" : "false");
    if (!exists) {
      ok = false;
      std::cout << " path=" << f.path.string() << "\n";
      continue;
    }

    const auto size = fs::file_size(f.path);
    const auto head = first_line_of(f.path);
    std::cout << " size=" << size << " first_line=\"" << head << "\"\n";
  }

  return ok ? 0 : 2;
}
