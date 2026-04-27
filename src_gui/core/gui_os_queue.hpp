#pragma once

#include <functional>
#include <string>

namespace up::gui::os {

// 平台层实现：日志与 UI 任务须可从工作线程调用（内部投递到 UI 线程）。
using LogSink = std::function<void(const std::string&)>;
using UiTask = std::function<void()>;

}  // namespace up::gui::os
