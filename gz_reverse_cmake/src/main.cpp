// 静态解析 CMakeLists.txt 脚本子集, 生成 GroundZero 风格的 package.xml + 各 target.xml。
// 不运行 cmake configure; 不修改 --source 下已有文件。

#include "cmake_interpret.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
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
  /// L7: 预置的 codemodel / File API 风格 JSON (不执行 cmake)
  std::string file_api_json;
  std::string package_name;
  std::string package_version = "0.1.0";
  bool help = false;
  // --source 未在命令行出现时使用当前工作目录
  bool source_from_default = false;
  // --out 未在命令行出现: 使用 <source>/gz_reverse/ (与 Listfile 同根、便于按库管理)
  bool out_from_default = false;
  /// 解析时在与 stderr 同屏刷新一行进度 (含不确定总长度的条与当前 Listfile 路径)
  bool show_progress = true;
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
      << "                    要写到其它位置请显式传 --out 。包级/目标级 in=、from= 以**本工具写出的**各 xml 所在目录为起算(同 gz configure)。\n"
      << "\n"
      << "其它:\n"
      << "  --package-name    包名 (默认: project() 第一个参数, 否则 reversed_project)\n"
      << "  --package-version 版本 (默认 0.1.0)\n"
      << "  --no-progress     关闭解析时的单行进度 (默认开启: 在 stderr 显示当前 Listfile 与滑块式进度条)\n"
      << "\n"
      << "说明: 不调用 cmake。将 Listfile 解析为命令流(语句级浅层 AST)后做静态子集重解释:\n"
      << "      project, set, include_directories, add_subdirectory, add_executable,\n"
      << "      add_library(STATIC/SHARED/MODULE 等), target_sources, target_link_libraries,\n"
      << "      target_include_directories, target_compile_options, target_link_options, set_target_properties(COMPILE_FLAGS|LINK_FLAGS),\n"
      << "      configure_file(可映射为 <config_files>); 不执行 if/foreach 真值, function/macro 内对 add_* 与 configure_file 不解释;\n"
      << "      生成器表达式 $<> 在相关实参上跳过。\n"
      << "      ${CMAKE_BINARY_DIR} 等按 GroundZero 与 gz 相同, 从 <source>/.intermediate/build/… 自动推算(见注)。\n"
      << "      可选: --file-api <path>  用户预置的 File API / codemodel 回复 JSON, 仅作 target 名对照 (不运行 cmake)。\n"
      << "XML: <headers> 若存在先写, 再 <config_files>(若有), 再 <sources>; <sources> 为 .c/.cpp 等 + 非对外头; 不写未展开 ${...} 。\n";
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

/// 单行 \r 进度条的有效宽度(超过终端列宽会换行, 随后 \r 只回到最后一行, 会堆多行/粘成一长串)
static int stderr_status_line_width() {
#if defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO info{};
  const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
  if (h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &info)) {
    const int w = static_cast<int>(info.srWindow.Right) - static_cast<int>(info.srWindow.Left) + 1;
    if (w >= 20 && w <= 1024) return w;
  }
  return 80;
#else
  if (const char *c = std::getenv("COLUMNS")) {
    const int w = std::atoi(c);
    if (w >= 20 && w <= 1024) return w;
  }
  return 80;
#endif
}

/// 在 stderr 同一行刷新: 旋转符 + 条(确定 n/m 时按比例, 否则滑块) + 文件序号 + 路径 + 可选 `| 阶段` + 可选 `cur/total`
static void print_listfile_status(std::size_t n, const fs::path &listfile, const char *phase, std::size_t cur, std::size_t total) {
  constexpr int bar_w = 20;
  std::string bar(static_cast<std::size_t>(bar_w), '-');
  static const char *const kSpin = "|/-\\";
  const char c = kSpin[n % 4u];
  if (total > 0 && cur > 0) {
    const int filled = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(bar_w), (cur * static_cast<std::size_t>(bar_w) + total / 2) / total));
    for (int i = 0; i < bar_w; ++i) bar[static_cast<std::size_t>(i)] = (i < filled) ? '#' : '-';
  } else {
    const int seg = 5;
    const int pos = static_cast<int>((n - 1) % (bar_w - seg + 1));
    for (int i = 0; i < bar_w; ++i) {
      if (i >= pos && i < pos + seg) bar[static_cast<std::size_t>(i)] = '#';
    }
  }
  std::string p = path_to_posix(listfile);
  const std::size_t kMax = (phase && phase[0]) ? 32u : 44u;
  if (p.size() > kMax) p = "…" + p.substr(p.size() - (kMax - 3u));
  std::string ns = std::to_string(n);
  while (ns.size() < 3u) ns = " " + ns;
  std::string line;
  line.reserve(128u);
  line += c;
  line += " [";
  line += bar;
  line += "] ";
  line += ns;
  line += "  ";
  line += p;
  if (phase && phase[0]) {
    line += "  | ";
    line += phase;
    if (total > 0) {
      line += " ";
      line += std::to_string(cur);
      line += "/";
      line += std::to_string(total);
    }
  }
  int kWidth = stderr_status_line_width();
  if (kWidth < 40) kWidth = 40;
  if (kWidth > 200) kWidth = 200;
  if (line.size() > static_cast<std::size_t>(kWidth)) {
    if (kWidth > 3) {
      line.resize(static_cast<std::size_t>(kWidth - 3u));
      line += "…";
    } else
      line.resize(static_cast<std::size_t>(kWidth));
  }
  if (line.size() < static_cast<std::size_t>(kWidth)) line.append(static_cast<std::size_t>(kWidth) - line.size(), ' ');
  std::cerr << "\r" << line << std::flush;
}

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

/// GZ `package.xml` / `target.xml` 的 `<file to="…"/>`：`to` 相对 **`.intermediate/generated/<arch 段>/<包名>/_package/`**（包级）或
/// **`.intermediate/generated/<arch 段>/<包名>/<目标名>/`**（目标级；`arch` 与 `gz_cache.txt` 的 `arch=` 一致）——见 `package-target-xml-spec` §2.6、§3.5。
/// 与 CMake 在 `CMAKE_CURRENT_BINARY_DIR` 下算出的**绝对/构建树**路径解耦，不要写入整条 `.intermediate/build/<叶>/...`。
/// 反解侧仅输出**生成文件名**；若需子目录布局请在包内手改 `to`（如 `include/foo.h`）。
static std::string config_to_for_gz(const fs::path &out_abs) {
  return out_abs.filename().generic_string();
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

static bool same_canonical_path(const fs::path &a, const fs::path &b) {
  std::error_code ec, ec2;
  const fs::path ca = fs::weakly_canonical(a, ec);
  const fs::path cb = fs::weakly_canonical(b, ec2);
  if (ec || ec2) return false;
  return ca == cb;
}

/// 与 `package.xml` 或本目标 `target.xml` 中已声明的 `config_files` **输出** 为同一文件时, 不写入 `<sources>`/`<headers>`,
/// 避免与 `gz configure` 对包级/目标级 `config_files` 自动并入编译列表重复.
static bool is_covered_by_config_files(const fs::path &sp, const TargetModel &tm, const std::vector<ConfigFilePathPair> &package_cf) {
  for (const auto &cf : package_cf) {
    if (same_canonical_path(cf.out_abs, sp)) return true;
  }
  for (const auto &cf : tm.config_files) {
    if (same_canonical_path(cf.out_abs, sp)) return true;
  }
  return false;
}

static bool is_strict_subpath(const fs::path &root, const fs::path &p) {
  std::error_code ec;
  const fs::path rel = fs::relative(p, root, ec);
  if (ec) return false;
  if (rel.empty()) return true;
  for (const auto &c : rel)
    if (c == fs::path("..")) return false;
  return true;
}

/// 包级 `in=`: 与 `gz configure` 的 `pkg_root / in` 一致, `pkg_root` = **package.xml 的父目录**.
/// `config_in_base` = 该父目录(本工具默认: `--out` 下本包输出目录, 与生成出的 package.xml 同目录。目标级 `in` 同理相对各 target 目录)
/// 模板 `in_abs` 须在 `--source` 下; 相对 `config_in_base` 的 `in` 可含 `..`.
static std::optional<std::string> package_config_in_for_package_xml(const fs::path &source_root, const fs::path &config_in_base,
                                                                       const fs::path &in_abs) {
  std::error_code es, ec, er, echk;
  const fs::path sr = fs::weakly_canonical(source_root, es);
  const fs::path b = fs::weakly_canonical(config_in_base, ec);
  const fs::path f = fs::weakly_canonical(in_abs, er);
  if (es || ec || er) {
    std::cerr << "警告: 无法规范化路径, 跳过 config_files in: " << path_to_posix(in_abs) << "\n";
    return std::nullopt;
  }
  if (!is_strict_subpath(sr, f)) {
    std::cerr << "警告: config 模板不在 --source 目录下, 不写入 package.xml(请手改 in): " << path_to_posix(f) << " (源根=" << path_to_posix(sr)
                << ")\n";
    return std::nullopt;
  }
  // config_in_base 常为本工具输出目录, 勿求其在 --source 下 (如 --out 在其它根).
  const fs::path rel = fs::relative(f, b, echk);
  if (echk) {
    std::cerr << "警告: 无法将模板路径相对 package 目录(请手改 in): f=" << path_to_posix(f) << " 基=" << path_to_posix(b) << "\n";
    return std::nullopt;
  }
  return path_to_posix(rel);
}

/// `<sources>`: `configure_file` 等仍映射到 `generated/…`；对落在 **GZ 估计的 build 根** 下的路径, 不再
/// 假写到 `generated/<包>/<目标>/…` (gz 不跑对方 add_custom_command). 反解将写 **与 build 根相对路径同形** 的
/// `--source` 下路径(如 `lib/foo.c`), 相对 `target.xml` 目录; **不会**再写成穿越到 `.intermediate/build/…`
/// 的相对路径(该处文件常由上游首次 configure 才生成, 在反解时尚不存在).
static std::string source_path_for_gz_remap(const fs::path &tdir, const fs::path &sp, const fs::path &source_root, const std::string &gen_arch,
                                            const std::string &pkg_name, const std::string &tname, const TargetModel &tm,
                                            const std::vector<ConfigFilePathPair> &package_cf) {
  std::error_code ec;
  const fs::path wc = fs::weakly_canonical(sp, ec);
  if (ec) return source_path_for_target_xml(tdir, sp, source_root);
  const fs::path gbase = source_root / ".intermediate" / "generated" / gen_arch;

  for (const auto &cf : package_cf) {
    if (!same_canonical_path(cf.out_abs, sp)) continue;
    const fs::path g = gbase / pkg_name / "_package" / config_to_for_gz(cf.out_abs);
    return source_path_for_target_xml(tdir, g, source_root);
  }
  for (const auto &cf : tm.config_files) {
    if (!same_canonical_path(cf.out_abs, sp)) continue;
    const fs::path g = gbase / pkg_name / tname / config_to_for_gz(cf.out_abs);
    return source_path_for_target_xml(tdir, g, source_root);
  }

  const fs::path top_parent = source_root;
  const fs::path broot = infer_gz_default_cmake_binary_root(top_parent);
  std::error_code ec2;
  const fs::path brc = fs::weakly_canonical(broot, ec2);
  if (ec2 || !is_strict_subpath(brc, wc)) return source_path_for_target_xml(tdir, sp, source_root);
  {
    const fs::path rel = fs::relative(wc, brc, ec2);
    if (ec2) return source_path_for_target_xml(tdir, sp, source_root);
    const fs::path try_src = (source_root / rel).lexically_normal();
    // 相对 tdir 一律按「源树根 + rel」写(若缺文件, 需维护者自备或先跑上游 cmake 把生成物抄回 rel)
    return source_path_for_target_xml(tdir, try_src, source_root);
  }
}

static bool string_has_cmake_deref(const std::string &s) { return s.find("${") != std::string::npos; }

static std::string ext_to_lower(const fs::path &p) {
  std::string e = p.extension().string();
  for (char &c : e) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  return e;
}

static bool is_compile_source_ext(const std::string &ext) {
  return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".m" || ext == ".mm" || ext == ".S" || ext == ".s" ||
         ext == ".ccm" || ext == ".c++" || ext == ".cppm";
}

static bool is_header_ext(const std::string &ext) {
  return ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx" || ext == ".h++";
}

static bool icase_eq_stem_to_pkg(const fs::path &file_path, const std::string &package_var_name) {
  if (package_var_name.empty()) return false;
  std::string stem = file_path.stem().string();
  for (char &c : stem) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  std::string pkg = package_var_name;
  for (char &c : pkg) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  return stem == pkg;
}

// <headers> 在 GZ 里表示**安装**到 include/ 的对外头, 与 compile -I 不同. 无 install() 时: 路径段含 include/, 或主头 <package>.h
static bool is_public_install_header_guess(const fs::path &abs, const fs::path &source_root, const std::string &package_var_name) {
  if (!is_header_ext(ext_to_lower(abs))) return false;
  std::error_code ec;
  const fs::path a = fs::absolute(abs);
  const fs::path s = fs::absolute(source_root);
  const fs::path rel = fs::relative(a, s, ec);
  if (ec) return false;
  for (const fs::path &comp : rel) {
    std::string c = comp.string();
    for (char &x : c) x = static_cast<char>(::tolower(static_cast<unsigned char>(x)));
    if (c == "include")
      return true;
  }
  if (icase_eq_stem_to_pkg(abs, package_var_name))
    return true;
  return false;
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
    if (a == "--file-api" && i + 1 < argc) {
      o->file_api_json = argv[++i];
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
    if (a == "--no-progress") {
      o->show_progress = false;
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
  std::cerr << "\n   CMAKE_BINARY_DIR 估计 = " << path_to_posix(infer_gz_default_cmake_binary_root(opt.source_dir))
            << "  (与 gz: <source>/.intermediate/build/<叶>)\n";
  const std::string gen_arch = infer_gz_generated_arch_segment(opt.source_dir);
  std::cerr << "   .intermediate/generated/... 的 arch 段 = " << gen_arch
            << "  (gz_cache 的 arch= 或 build 叶名, 与 gz configure 的 compose_arch_tag 一致)\n";
  warn_out_equals_source(opt.source_dir, opt.out_dir);

  try {
    fs::path top = opt.source_dir / "CMakeLists.txt";
    if (!fs::is_regular_file(top)) {
      std::cerr << "找不到: " << path_to_posix(top) << "\n";
      return 1;
    }
    fs::path fapi_path;
    const fs::path *fapi = nullptr;
    if (!opt.file_api_json.empty()) {
      fapi_path = fs::path(opt.file_api_json);
      fapi = &fapi_path;
    }
    ListfileProgress lprog;
    if (opt.show_progress) {
      lprog.on = [](std::size_t n, const fs::path &p) { print_listfile_status(n, p, nullptr, 0, 0); };
      lprog.on_intra = [](std::size_t n, const fs::path &p, const char *ph, std::size_t c, std::size_t t) {
        print_listfile_status(n, p, ph, c, t);
      };
    }
    InterpretResult ir = interpret_cmake_tree(opt.source_dir, top, fapi, opt.show_progress ? &lprog : nullptr);
    if (opt.show_progress) std::cerr << "\n";
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
    std::error_code ec_pbase;
    fs::path package_config_in_base = fs::weakly_canonical(pkg_root, ec_pbase);
    if (ec_pbase) package_config_in_base = pkg_root;
    std::cerr << "   包级 in= 相对 (gz: package.xml 父目录) = " << path_to_posix(package_config_in_base) << "  (= <out>/<包名>)\n";
    {
      std::ofstream f(pkg_root / "package.xml", std::ios::binary);
      f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<package name=\"" << pkg_name << "\" version=\"" << opt.package_version
        << "\">\n";
      if (!ir.package_config_files.empty()) {
        std::vector<std::pair<std::string, std::string>> pkg_cf_rows;
        for (const auto &cf : ir.package_config_files) {
          const std::optional<std::string> rel_opt =
              package_config_in_for_package_xml(opt.source_dir, package_config_in_base, cf.in_abs);
          if (!rel_opt.has_value()) continue;
          std::string rel_in = *rel_opt;
          if (string_has_cmake_deref(rel_in)) {
            std::cerr << "注: 跳过含未展开 ${...} 的 package config_files in (请手补): " << rel_in << "\n";
            continue;
          }
          const std::string to = config_to_for_gz(cf.out_abs);
          std::string ain = rel_in;
          std::string ato = to;
          xml_escape(ain);
          xml_escape(ato);
          pkg_cf_rows.emplace_back(std::move(ain), std::move(ato));
        }
        if (!pkg_cf_rows.empty()) {
          f << "  <config_files>\n";
          for (const auto &pr : pkg_cf_rows) f << "    <file in=\"" << pr.first << "\" to=\"" << pr.second << "\"/>\n";
          f << "  </config_files>\n";
        }
      }
      f << "</package>\n";
    }

    std::set<std::string> our_names;
    for (const auto &p : loaded) our_names.insert(p.first);

    size_t n_written = 0;
    for (const auto &ent : loaded) {
      const std::string &tname = ent.first;
      const TargetModel &tm = *ent.second;
      fs::path tdir = pkg_root / tname;
      fs::create_directories(tdir);

      std::vector<std::string> source_files, header_install_files;
      for (const fs::path &sp : tm.source_paths_abs) {
        if (string_has_cmake_deref(path_to_posix(sp))) {
          std::cerr << "注: 跳过含未展开 ${...} 的路径(请手补): " << path_to_posix(sp) << "\n";
          continue;
        }
        if (is_covered_by_config_files(sp, tm, ir.package_config_files)) continue;
        const std::string ext = ext_to_lower(sp);
        if (ext == ".obj" || ext == ".o") {
          std::cerr << "注: 跳过 .obj/.o(编译产物, 非 add_library 源; 请改列 .c/.cpp/.rc): " << path_to_posix(sp) << "\n";
          continue;
        }
        std::string rel = source_path_for_gz_remap(tdir, sp, opt.source_dir, gen_arch, pkg_name, tname, tm, ir.package_config_files);
        if (string_has_cmake_deref(rel)) {
          std::cerr << "注: 跳过(相对路径中含 ${...} 未展开): " << rel << "\n";
          continue;
        }
        if (is_header_ext(ext)) {
          if (is_public_install_header_guess(sp, opt.source_dir, pkg_name))
            header_install_files.push_back(std::move(rel));
          else
            source_files.push_back(std::move(rel));
        } else
          source_files.push_back(std::move(rel));
      }
      if (source_files.empty()) {
        std::cerr << "注: 目标 \"" << tname << "\" 在过滤后无 <sources> 可写(可能仅余含 ${} 的条目), 跳过\n";
        continue;
      }
      std::sort(source_files.begin(), source_files.end());
      source_files.erase(std::unique(source_files.begin(), source_files.end()), source_files.end());
      std::sort(header_install_files.begin(), header_install_files.end());
      header_install_files.erase(std::unique(header_install_files.begin(), header_install_files.end()), header_install_files.end());

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
      if (!header_install_files.empty()) {
        f << "  <headers>\n";
        for (const std::string &hf : header_install_files) {
          std::string a = hf;
          xml_escape(a);
          f << "    <file from=\"" << a << "\"/>\n";
        }
        f << "  </headers>\n";
      }
      if (!tm.config_files.empty()) {
        std::vector<std::pair<std::string, std::string>> tgt_cf_rows;
        for (const auto &cf : tm.config_files) {
          std::string rel_in = source_path_for_target_xml(tdir, cf.in_abs, opt.source_dir);
          if (string_has_cmake_deref(rel_in)) {
            std::cerr << "注: 目标 \"" << tname << "\": 跳过含 ${...} 的 config_files in: " << rel_in << "\n";
            continue;
          }
          const std::string to = config_to_for_gz(cf.out_abs);
          std::string ain = rel_in;
          std::string ato = to;
          xml_escape(ain);
          xml_escape(ato);
          tgt_cf_rows.emplace_back(std::move(ain), std::move(ato));
        }
        if (!tgt_cf_rows.empty()) {
          f << "  <config_files>\n";
          for (const auto &pr : tgt_cf_rows) f << "    <file in=\"" << pr.first << "\" to=\"" << pr.second << "\"/>\n";
          f << "  </config_files>\n";
        }
      }
      if (!tm.compile_flags.empty()) {
        f << "  <compile_flags>\n";
        for (const std::string &a : tm.compile_flags) {
          std::string ac = a;
          xml_escape(ac);
          f << "    <arg>" << ac << "</arg>\n";
        }
        f << "  </compile_flags>\n";
      }
      if (!tm.link_flags.empty()) {
        f << "  <link_flags>\n";
        for (const std::string &a : tm.link_flags) {
          std::string ac = a;
          xml_escape(ac);
          f << "    <arg>" << ac << "</arg>\n";
        }
        f << "  </link_flags>\n";
      }
      f << "  <sources>\n";
      for (const std::string &rs : source_files) {
        std::string fp = rs;
        xml_escape(fp);
        f << "    <file>" << fp << "</file>\n";
      }
      f << "  </sources>\n";
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
    for (const std::string &m : ir.file_api_merge_notes) std::cerr << "注: " << m << "\n";
    if (!ir.file_api_target_names.empty()) {
      std::cerr << "注: L7 file_api 提取了 " << ir.file_api_target_names.size() << " 个 target 名 (对照用)\n";
    }
    return 0;
  } catch (const std::exception &e) {
    if (opt.show_progress) std::cerr << "\n";
    std::cerr << "异常: " << e.what() << "\n";
    return 1;
  }
}
