#pragma once

#include <string>

namespace gz::gui::platform::cocoa {

void log_line(const std::string& s);
void set_status(const char* s);

}  // namespace gz::gui::platform::cocoa
