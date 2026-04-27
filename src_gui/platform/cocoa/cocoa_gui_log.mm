#import <Cocoa/Cocoa.h>

#include "platform/cocoa/cocoa_gui_log.hpp"
#include "platform/cocoa/cocoa_gui_state.hpp"

#include <string>

namespace up::gui::platform::cocoa {

static void log_append(NSString* line) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (cs().log) {
      NSTextStorage* st = [cs().log textStorage];
      [st appendAttributedString:[[NSAttributedString alloc] initWithString:line]];
      [cs().log scrollRangeToVisible:NSMakeRange([[cs().log string] length], 0)];
    }
  });
}

void log_line(const std::string& s) {
  log_append([NSString stringWithUTF8String:(s + "\n").c_str()]);
}

void set_status(const char* s) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (cs().status)
      [cs().status setStringValue:[NSString stringWithUTF8String:s]];
  });
}

}  // namespace up::gui::platform::cocoa
