#pragma once

#include <functional>
#include <string>

namespace up::gui::platform::win32 {

// UTF-8 累积缓冲按行切分并回调宽字符块（用于子进程 stdout 流式转 UI 日志）。
void DispatchCompleteLines(std::string& pending, const std::function<void(const std::wstring&)>& on_chunk);

// 将缓冲中剩余非完整行作为一块送出。
void DispatchTailChunk(std::string& pending, const std::function<void(const std::wstring&)>& on_chunk);

}  // namespace up::gui::platform::win32
