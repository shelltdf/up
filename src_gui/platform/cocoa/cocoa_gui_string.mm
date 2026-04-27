#import <Foundation/Foundation.h>

#include "platform/cocoa/cocoa_gui_string.hpp"

namespace up::gui::platform::cocoa {

std::string ns_to_utf8(NSString* s) {
  if (!s)
    return {};
  return std::string([s UTF8String]);
}

NSString* utf8_to_ns(const std::string& s) {
  return [NSString stringWithUTF8String:s.c_str()];
}

}  // namespace up::gui::platform::cocoa
