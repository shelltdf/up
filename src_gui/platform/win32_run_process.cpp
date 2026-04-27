#include "platform/win32_run_process.hpp"

#include "platform/win32_encoding.hpp"

#include <string>

namespace up::gui::platform::win32 {

void DispatchCompleteLines(std::string& pending, const std::function<void(const std::wstring&)>& on_chunk) {
  if (!on_chunk)
    return;
  for (;;) {
    const size_t eol = pending.find('\n');
    if (eol == std::string::npos)
      break;
    std::string line = pending.substr(0, eol + 1);
    pending.erase(0, eol + 1);
    const std::wstring chunk = Utf8ToWide(line);
    if (!chunk.empty())
      on_chunk(chunk);
  }
}

void DispatchTailChunk(std::string& pending, const std::function<void(const std::wstring&)>& on_chunk) {
  if (!on_chunk || pending.empty())
    return;
  const std::wstring tail = Utf8ToWide(pending);
  if (!tail.empty())
    on_chunk(tail);
  pending.clear();
}

}  // namespace up::gui::platform::win32
