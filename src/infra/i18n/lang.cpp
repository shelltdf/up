#include "lang.hpp"

#include <cstdlib>
#include <cstring>
#include <ostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace up {
namespace lang {

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
    os << "up — 通用包/构建编排（原型）\n\n"
          "用法:\n"
          "  up configure [--build-dir-name <名>] [--scan <目录>]... [--opt KEY=VALUE]...\n"
          "    （<名> 为 .intermediate/build 下的子目录名；省略时默认为 default）\n"
          "  up build --build-dir-name <名>\n"
          "  up run --install-dir-name <名> <可执行目标名>  （<名> 为 .intermediate/install 下的子目录名）\n"
          "  up test --install-dir-name <名> [测试目标名]\n"
          "  up pack --install-dir-name <名>...  （可重复，多架构）\n"
          "  up project\n"
          "  up --help | -h | help\n";
  } else {
    os << "up — generic package / build orchestrator (prototype)\n\n"
          "Usage:\n"
          "  up configure [--build-dir-name <leaf>] [--scan <dir>]... [--opt KEY=VALUE]...\n"
          "    (<leaf> is a subdirectory name under .intermediate/build; default: default)\n"
          "  up build --build-dir-name <leaf>\n"
          "  up run --install-dir-name <leaf> <target_executable_name>  (<leaf> under .intermediate/install)\n"
          "  up test --install-dir-name <leaf> [test_target_name]\n"
          "  up pack --install-dir-name <leaf>...  (repeatable; multi-arch)\n"
          "  up project\n"
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
