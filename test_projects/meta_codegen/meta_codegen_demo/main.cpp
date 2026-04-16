#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct CheckFile {
  std::string label;
  fs::path path;
  std::string expected;
};

std::string read_all(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
  fs::path root;
  if (argc > 1 && argv[1] != nullptr) {
    root = argv[1];
  } else {
    root = fs::current_path();
    if (!fs::exists(root / "samples")) {
      for (fs::path p = root; !p.empty() && p != p.parent_path(); p = p.parent_path()) {
        if (fs::exists(p / "samples") && fs::exists(p / "package.xml")) {
          root = p;
          break;
        }
      }
    }
  }

  const fs::path samples = root / "samples";
  const std::vector<CheckFile> files = {
      {"moc", samples / "widget.meta.cpp", "WidgetWindow_meta_class"},
      {"uic_h", samples / "ui_main_panel.h", "Ui_MainPanel"},
      {"uic_cpp", samples / "ui_main_panel.cpp", "ui_name"},
      {"rc_h", samples / "rc_app.h", "resource_symbol"},
      {"rc_cpp", samples / "rc_app.cpp", "resource_text"},
  };

  bool ok = true;
  std::cout << "[meta_codegen_demo] samples=" << samples.string() << "\n";
  for (const auto& f : files) {
    if (!fs::exists(f.path)) {
      std::cout << "[meta_codegen_demo] missing " << f.label << ": " << f.path.string() << "\n";
      ok = false;
      continue;
    }
    const std::string content = read_all(f.path);
    const bool has_expected = content.find(f.expected) != std::string::npos;
    std::cout << "[meta_codegen_demo] " << f.label << " size=" << content.size()
              << " contains(\"" << f.expected << "\")=" << (has_expected ? "true" : "false") << "\n";
    if (!has_expected) {
      ok = false;
    }
  }

  return ok ? 0 : 2;
}
