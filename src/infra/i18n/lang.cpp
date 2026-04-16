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
          "  up configure [--scan <目录>]... [--opt KEY=VALUE]...\n"
          "  up build\n"
          "  up run <可执行目标名>\n"
          "  up test [测试目标名]\n"
          "  up pack\n"
          "  up project\n";
  } else {
    os << "up — generic package / build orchestrator (prototype)\n\n"
          "Usage:\n"
          "  up configure [--scan <dir>]... [--opt KEY=VALUE]...\n"
          "  up build\n"
          "  up run <target_executable_name>\n"
          "  up test [test_target_name]\n"
          "  up pack\n"
          "  up project\n";
  }
}

const char* configure_path_non_ascii() {
  if (zh())
    return "configure: 路径含非 ASCII 字符，按设计不支持（请改用仅英文路径）: ";
  return "configure: path contains non-ASCII characters (not supported; use ASCII-only paths): ";
}

}  // namespace lang
}  // namespace up
