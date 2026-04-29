// 静态解析 CMakeLists.txt 脚本子集, 生成 GroundZero 风格的 package.xml + 各 target.xml。
// 不运行 cmake configure; 不修改 --source 下已有文件。

#include "cmake_interpret.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

#if defined(_WIN32)
// 避免 cmd 默认非 UTF-8 时中文帮助/错误显示为乱码 (e.g. "缺少" -> "缂哄皯")
static void init_windows_console_utf8() {
  SetConsoleOutputCP(65001);
  SetConsoleCP(65001);
}
#endif

struct Options {
  fs::path source_dir;
  fs::path out_dir;
  std::string package_name;
  std::string package_version = "0.1.0";
  bool help = false;
  // --source 未在命令行出现时使用当前工作目录
  bool source_from_default = false;
  // --out 未在命令行出现: 使用 <source>/gz_reverse/ (与 Listfile 同根、便于按库管理)
  bool out_from_default = false;
};

static void print_usage() {
  std::cerr
      << "gz_reverse_cmake — 静态解析 CMakeLists.txt 子集, 生成 package.xml / target.xml 初稿\n"
      << "\n"
      << "用法:\n"
      << "  gz_reverse_cmake [ --source <path> ] [ --out <path> ] [选项]\n"
      << "  典型: cd 到含顶层 CMakeLists.txt 的目录后无参执行; --source=当前工作目录, --out=<--source>/gz_reverse/ 。\n"
      << "\n"
      << "参数:\n"
      << "  --source <path>   含顶层 CMakeLists.txt 的源目录; 省略 = 当前工作目录。\n"
      << "  --out <path>      输出根, 下建 <包名>/… ; 省略 = <--source>/gz_reverse/ (与源码根同树, 子目录名固定为 gz_reverse)。\n"
      << "                    要写到其它位置请显式传 --out 。\n"
      << "\n"
      << "其它:\n"
      << "  --package-name    包名 (默认: project() 第一个参数, 否则 reversed_project)\n"
      << "  --package-version 版本 (默认 0.1.0)\n"
      << "\n"
      << "说明: 不调用 cmake。将 Listfile 解析为命令流(语句级浅层 AST)后做静态子集重解释:\n"
      << "      project, set, include_directories, add_subdirectory, add_executable,\n"
      << "      add_library(STATIC/SHARED/MODULE 等), target_sources, target_link_libraries,\n"
      << "      target_include_directories; 不执行 if/foreach 真值, function/macro 内 add_* 忽略;\n"
      << "      生成器表达式 $<> 在相关实参上跳过。\n";
}

static bool is_skipped_utility_name(const std::string &name) {
  static const char *kSkip[] = {"all_build", "zero_check", "package",  "edit_cache", "rebuild_cache",
                                 "install",  "test",    "run_tests", "list_install_components"};
  std::string lower;
  for (char c : name) {
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  for (const char *p : kSkip) {
    if (lower == p) return true;
  }
  return false;
}

static std::string to_var_name(std::string s) {
  for (char &c : s) {
    if (c == '-' || c == ' ' || c == '.') c = '_';
  }
  return s;
}

static void xml_escape(std::string &s) {
  std::string r;
  r.reserve(s.size());
  for (char c : s) {
    if (c == '<')
      r += "&lt;";
    else if (c == '&')
      r += "&amp;";
    else if (c == '>')
      r += "&gt;";
    else
      r += c;
  }
  s = std::move(r);
}

static std::string path_to_posix(const fs::path &p) { return p.generic_string(); }

/// 将 --out 与 --source 设成**同一路径**时提醒 (package 会直接占在根下); 默认可在 <source>/gz_reverse/ 不告警
static void warn_out_equals_source(const fs::path &source, const fs::path &out_root) {
  std::error_code ec1, ec2;
  const fs::path a = fs::weakly_canonical(source, ec1);
  const fs::path b = fs::weakly_canonical(out_root, ec2);
  if (ec1 || ec2) return;
  if (a != b) return;
  std::cerr
      << "warning: --out is the same as --source. Package folders will be created as <out>/<name>/ next to top CMakeLists.txt. "
         "Omit --out to use the default: <source>/gz_reverse/ .\n"
         "警告: 输出根与源目录相同, 生成物会混在源根; 可省略 --out 以用默认的 <source>/gz_reverse/ 作输出根\n";
}

static std::string source_path_for_target_xml(const fs::path &tdir, const fs::path &abs_path, const fs::path & /*top*/) {
  std::error_code ec;
  fs::path p = abs_path;
  p = fs::weakly_canonical(p, ec);
  fs::path b = fs::weakly_canonical(tdir, ec);
  fs::path r = fs::relative(p, b, ec);
  if (ec) {
    std::cerr << "警告: 无法计算相对路径: " << path_to_posix(abs_path) << "\n";
    return path_to_posix(abs_path.generic_string());
  }
  return path_to_posix(r);
}

static int parse_args(int argc, char **argv, Options *o) {
  bool have_source = false, have_out = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      o->help = true;
      return 0;
    }
    if (a == "--source" && i + 1 < argc) {
      o->source_dir = fs::path(argv[++i]);
      have_source = true;
      continue;
    }
    if (a == "--out" && i + 1 < argc) {
      o->out_dir = fs::path(argv[++i]);
      have_out = true;
      continue;
    }
    if (a == "--package-name" && i + 1 < argc) {
      o->package_name = argv[++i];
      continue;
    }
    if (a == "--package-version" && i + 1 < argc) {
      o->package_version = argv[++i];
      continue;
    }
    std::cerr << "error: unknown argument: " << a << " (use --help)\n";
    return 2;
  }
  if (o->help) return 0;
  o->source_from_default = !have_source;
  o->out_from_default = !have_out;
  if (!have_source) o->source_dir = fs::current_path();
  {
    std::error_code ec;
    const fs::path abs_src = fs::absolute(o->source_dir);
    const fs::path can_src = fs::weakly_canonical(abs_src, ec);
    o->source_dir = ec ? abs_src : can_src;
  }
  if (!have_out) o->out_dir = o->source_dir / "gz_reverse";
  o->out_dir = fs::absolute(o->out_dir);
  {
    std::error_code ec;
    fs::path co = fs::weakly_canonical(o->out_dir, ec);
    o->out_dir = ec ? fs::absolute(o->out_dir) : co;
  }
  return 0;
}

int main(int argc, char **argv) {
#if defined(_WIN32)
  init_windows_console_utf8();
#endif
  Options opt;
  int pr = parse_args(argc, argv, &opt);
  if (pr != 0) {
    if (opt.help) {
      print_usage();
      return 0;
    }
    return pr;
  }
  if (opt.help) {
    print_usage();
    return 0;
  }
  // 始终打印最终解析结果, 避免只写「未指定 --out」时用户误以为未识别 --source
  std::cerr << "注: 扫描源目录 (--source) = " << path_to_posix(opt.source_dir);
  if (opt.source_from_default) std::cerr << "  (未在命令行写 --source, 为当前工作目录)";
  std::cerr << "\n   输出根目录 (--out)  = " << path_to_posix(opt.out_dir);
  if (opt.out_from_default) std::cerr << "  (未在命令行写 --out, 默认: <source>/gz_reverse/)";
  std::cerr << "\n";
  warn_out_equals_source(opt.source_dir, opt.out_dir);

  try {
    fs::path top = opt.source_dir / "CMakeLists.txt";
    if (!fs::is_regular_file(top)) {
      std::cerr << "找不到: " << path_to_posix(top) << "\n";
      return 1;
    }
    InterpretResult ir = interpret_cmake_tree(opt.source_dir, top);
    std::string pkg_name = opt.package_name;
    if (pkg_name.empty()) pkg_name = ir.project_name;
    if (pkg_name.empty()) pkg_name = "reversed_project";
    pkg_name = to_var_name(pkg_name);

    std::vector<std::pair<std::string, const TargetModel *>> loaded;
    for (const auto &kv : ir.targets) {
      if (is_skipped_utility_name(kv.first)) continue;
      if (kv.second.kind != "executable" && kv.second.kind != "static_library" && kv.second.kind != "shared_library")
        continue;
      if (kv.second.source_paths_abs.empty()) continue;
      loaded.push_back({kv.first, &kv.second});
    }
    if (loaded.empty()) {
      std::cerr << "没有可导出的可执行/库目标 (或均在 function 内/未识别)\n";
      return 1;
    }

    fs::path pkg_root = opt.out_dir / pkg_name;
    fs::create_directories(pkg_root);
    {
      std::ofstream f(pkg_root / "package.xml", std::ios::binary);
      f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<package name=\"" << pkg_name << "\" version=\"" << opt.package_version
        << "\">\n</package>\n";
    }

    std::set<std::string> our_names;
    for (const auto &p : loaded) our_names.insert(p.first);

    size_t n_written = 0;
    for (const auto &ent : loaded) {
      const std::string &tname = ent.first;
      const TargetModel &tm = *ent.second;
      fs::path tdir = pkg_root / tname;
      fs::create_directories(tdir);

      std::vector<std::string> rel_sources;
      for (const fs::path &sp : tm.source_paths_abs) {
        rel_sources.push_back(source_path_for_target_xml(tdir, sp, opt.source_dir));
      }
      std::sort(rel_sources.begin(), rel_sources.end());
      rel_sources.erase(std::unique(rel_sources.begin(), rel_sources.end()), rel_sources.end());

      std::set<std::string> rel_includes;
      for (const fs::path &ip : tm.include_dir_abs) {
        std::string r = source_path_for_target_xml(tdir, ip, opt.source_dir);
        if (r.empty() || r == ".")
          rel_includes.insert(".");
        else
          rel_includes.insert(r);
      }

      std::vector<std::string> dep_names;
      for (const std::string &d : tm.link_to) {
        if (d == tname) continue;
        if (our_names.count(d) == 0) continue;
        dep_names.push_back(d);
      }
      std::sort(dep_names.begin(), dep_names.end());
      dep_names.erase(std::unique(dep_names.begin(), dep_names.end()), dep_names.end());

      std::ofstream f(tdir / "target.xml", std::ios::binary);
      f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<target name=\"" << tname << "\" type=\"" << tm.kind << "\">\n";
      f << "  <sources>\n";
      for (const std::string &rs : rel_sources) {
        std::string fp = rs;
        xml_escape(fp);
        f << "    <file>" << fp << "</file>\n";
      }
      f << "  </sources>\n";
      if (!rel_includes.empty()) {
        f << "  <headers>\n";
        for (const std::string &inc : rel_includes) {
          std::string a = inc;
          xml_escape(a);
          f << "    <dir from=\"" << a << "\"/>\n";
        }
        f << "  </headers>\n";
      }
      for (const std::string &d : dep_names) {
        std::string dcopy = d;
        xml_escape(dcopy);
        f << "  <dependency name=\"" << dcopy << "\" visibility=\"private\"/>\n";
      }
      f << "</target>\n";
      ++n_written;
    }
    std::cout << "已写入: " << path_to_posix(pkg_root) << " (package.xml + " << n_written << " 个 target 目录)\n";
    for (const std::string &w : ir.errors) std::cerr << "注: " << w << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "异常: " << e.what() << "\n";
    return 1;
  }
}
