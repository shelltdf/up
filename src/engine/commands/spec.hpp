#pragma once

#include <string>
#include <vector>

namespace up {

// Prints embedded English spec for package.xml / target.xml (stdout). Ignores args for now.
int cmd_spec(const std::vector<std::string>& args);

}  // namespace up
