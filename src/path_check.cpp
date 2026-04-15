#include "path_check.hpp"

namespace up {

bool path_has_non_ascii(const std::filesystem::path& path) {
#ifdef _WIN32
  const std::wstring ws = path.wstring();
  for (wchar_t c : ws) {
    if (static_cast<unsigned int>(c) > 127u)
      return true;
  }
  return false;
#else
  const std::string s = path.generic_string();
  for (unsigned char c : s) {
    if (c > 127u)
      return true;
  }
  return false;
#endif
}

}  // namespace up
