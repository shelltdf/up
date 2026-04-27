#pragma once

#import <Cocoa/Cocoa.h>

#include "gui_persist.hpp"

#include <atomic>
#include <filesystem>

namespace gz::gui::platform::cocoa {

struct CocoaState {
  std::atomic<bool> busy{false};
  gz::gui::persist::PersistedEnv persist{};
  std::filesystem::path settings_file;
  std::filesystem::path gz_exe;
  NSTextField* tf_cwd{};
  NSTextField* tf_build{};
  NSTextField* tf_install{};
  NSTextField* tf_extra{};
  NSTextField* tf_run{};
  NSTextField* tf_test{};
  NSTableView* table_scan{};
  NSMutableArray<NSString*>* scan_rows{};
  NSTextView* log{};
  NSTextField* status{};
};

CocoaState& cs();

}  // namespace gz::gui::platform::cocoa
