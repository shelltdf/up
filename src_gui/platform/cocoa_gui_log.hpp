#pragma once

#include <string>

namespace up::gui::platform::cocoa {

void log_line(const std::string& s);
void set_status(const char* s);

}  // namespace up::gui::platform::cocoa
