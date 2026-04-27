#import <Cocoa/Cocoa.h>

#include "platform/cocoa/cocoa_gui_bridge.hpp"
#include "platform/cocoa/cocoa_gui_log.hpp"
#include "platform/cocoa/cocoa_gui_state.hpp"
#include "platform/cocoa/cocoa_gui_string.hpp"

#include "gui_persist.hpp"
#include "gui_shell_actions.hpp"

#include <string>

namespace up::gui::platform::cocoa {

namespace persist = up::gui::persist;
namespace shell = up::gui::shell;

std::vector<std::string> collect_scans() {
  std::vector<std::string> out;
  for (NSString* row in cs().scan_rows)
    if (row.length)
      out.push_back([row UTF8String]);
  return out;
}

void save_ui() {
  cs().persist.browse_cwd = persist::path_to_portable_utf8(ns_to_utf8([cs().tf_cwd stringValue]));
  persist::save_settings(cs().settings_file, cs().persist);
}

void run_up_async(const std::string& args_no_exe) {
  const std::string cwd = persist::path_to_portable_utf8(ns_to_utf8([cs().tf_cwd stringValue]));
  const auto maybe_cwd = shell::try_acquire_run_context(cs().up_exe, cwd, cs().busy, [](const std::string& s) { log_line(s); });
  if (!maybe_cwd)
    return;
  const std::string extra = ns_to_utf8([cs().tf_extra stringValue]);
  shell::run_up_command_in_detached_thread(
      cs().up_exe, *maybe_cwd, args_no_exe, extra, cs().busy, [](const std::string& s) { log_line(s); },
      []() { set_status("Ready"); });
  set_status("Running up…");
}

void upgui_apply_open_panel_directory(NSOpenPanel* p, NSString* primaryPath, const std::string& fallbackUtf8) {
  NSString* s = primaryPath;
  if (s.length == 0 && !fallbackUtf8.empty())
    s = utf8_to_ns(fallbackUtf8);
  if (s.length == 0)
    return;
  NSString* expanded = [s stringByExpandingTildeInPath];
  BOOL isDir = NO;
  if ([[NSFileManager defaultManager] fileExistsAtPath:expanded isDirectory:&isDir] && isDir)
    [p setDirectoryURL:[NSURL fileURLWithPath:expanded isDirectory:YES]];
}

}  // namespace up::gui::platform::cocoa
