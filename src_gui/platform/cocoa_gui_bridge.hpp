#pragma once

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

namespace up::gui::platform::cocoa {

std::vector<std::string> collect_scans();
void save_ui();
void run_up_async(const std::string& args_no_exe);
void upgui_apply_open_panel_directory(NSOpenPanel* p, NSString* primaryPath, const std::string& fallbackUtf8);

}  // namespace up::gui::platform::cocoa
