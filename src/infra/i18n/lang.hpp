#pragma once

#include <iosfwd>

namespace up {
namespace lang {

bool zh();

void print_usage(std::ostream& os);
const char* configure_path_non_ascii();

}  // namespace lang
}  // namespace up
