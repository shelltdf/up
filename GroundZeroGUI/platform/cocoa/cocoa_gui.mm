// macOS Cocoa 入口：主窗 cocoa_gui_window.mm；控制器 cocoa_gui_handlers.mm；状态 cocoa_gui_state.*；桥接 cocoa_gui_bridge.mm。

#import <Cocoa/Cocoa.h>

#include "platform/cocoa/cocoa_gui.hpp"
#include "platform/cocoa/cocoa_gui_window.hpp"

namespace gz::gui::platform::cocoa {

int run(int argc, char** argv) {
  (void)argc;
  (void)argv;
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    build_window();
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }
  return 0;
}

}  // namespace gz::gui::platform::cocoa
