#pragma once

#import <Foundation/Foundation.h>

#include <string>

namespace up::gui::platform::cocoa {

std::string ns_to_utf8(NSString* s);
NSString* utf8_to_ns(const std::string& s);

}  // namespace up::gui::platform::cocoa
