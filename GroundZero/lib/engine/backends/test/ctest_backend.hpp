#pragma once

#include "core/backend_types.hpp"

#include <string>

namespace gz {

std::string build_ctest_command(const TestBackendContext& ctx);

}  // namespace gz
