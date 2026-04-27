#include "lang.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace up {
namespace lang {

#ifdef _WIN32
bool try_write_utf8_to_console(std::ostream& os, const char* utf8_text) {
  if (!utf8_text)
    return false;
  HANDLE h = INVALID_HANDLE_VALUE;
  if (&os == &std::cout)
    h = GetStdHandle(STD_OUTPUT_HANDLE);
  else if (&os == &std::cerr)
    h = GetStdHandle(STD_ERROR_HANDLE);
  if (h == INVALID_HANDLE_VALUE || h == nullptr)
    return false;
  DWORD mode = 0;
  if (!GetConsoleMode(h, &mode))
    return false;  // redirected to file/pipe: keep regular stream behavior.

  const int wide_n = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, nullptr, 0);
  if (wide_n <= 1)
    return false;
  std::wstring w(static_cast<size_t>(wide_n), L'\0');
  const int converted = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, w.data(), wide_n);
  if (converted <= 0)
    return false;
  if (!w.empty() && w.back() == L'\0')
    w.pop_back();
  DWORD written = 0;
  if (!WriteConsoleW(h, w.c_str(), static_cast<DWORD>(w.size()), &written, nullptr))
    return false;
  return true;
}
#endif

bool zh() {
#ifdef _WIN32
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
#else
  const char* loc = std::getenv("LANG");
  if (!loc)
    return false;
  return std::strstr(loc, "zh") != nullptr || std::strstr(loc, "ZH") != nullptr;
#endif
}

void print_usage(std::ostream& os) {
  if (zh()) {
    const char* zh_usage = "up — 通用包/构建编排（原型）\n\n"
                           "用法:\n"
                           "  up [--verbose|-v] <子命令> ...   （或设 UP_VERBOSE=1；阶段信息输出到 stderr）\n"
                           "  up spec   （stdout 输出内嵌英文 package.xml / target.xml 规范，无仓库 .md 时供 AI 使用）\n"
                           "  up list [--format tree|json|xml] [--xml <文件路径>] [--json <文件路径>] [--quiet]\n"
                           "    （默认 tree；可输出到控制台或导出 XML/JSON）\n"
                           "  up configure [--build-dir-name <名>] [--scan <目录>]... [--opt KEY=VALUE]...\n"
                           "    （<名> 为 .intermediate/build 下的子目录名；省略时默认为 default）\n"
                           "  up build --build-dir-name <名>\n"
                           "  up run --install-dir-name <名> <可执行目标名>  （<名> 为 .intermediate/install 下的子目录名）\n"
                           "  up test --install-dir-name <名> [测试目标名]\n"
                           "  up pack --install-dir-name <名>...  （可重复，多架构）\n"
                           "  up --help | -h | help\n";
#ifdef _WIN32
    if (try_write_utf8_to_console(os, zh_usage))
      return;
#endif
    os << zh_usage;
  } else {
    os << "up — generic package / build orchestrator (prototype)\n\n"
          "Usage:\n"
          "  up [--verbose|-v] <subcommand> ...   (or UP_VERBOSE=1; phase lines go to stderr)\n"
          "  up spec   (print embedded English package.xml / target.xml rules to stdout for AI/tools; no .md needed)\n"
          "  up list [--format tree|json|xml] [--xml <file>] [--json <file>] [--quiet]\n"
          "    (default tree; print to stdout and/or export XML/JSON)\n"
          "  up configure [--build-dir-name <leaf>] [--scan <dir>]... [--opt KEY=VALUE]...\n"
          "    (<leaf> is a subdirectory name under .intermediate/build; default: default)\n"
          "  up build --build-dir-name <leaf>\n"
          "  up run --install-dir-name <leaf> <target_executable_name>  (<leaf> under .intermediate/install)\n"
          "  up test --install-dir-name <leaf> [test_target_name]\n"
          "  up pack --install-dir-name <leaf>...  (repeatable; multi-arch)\n"
          "  up --help | -h | help\n";
  }
}

const char* configure_path_non_ascii() {
  if (zh())
    return "configure: 路径含非 ASCII 字符，按设计不支持（请改用仅英文路径）: ";
  return "configure: path contains non-ASCII characters (not supported; use ASCII-only paths): ";
}

}  // namespace lang
}  // namespace up
