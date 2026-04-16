#pragma once

#include "backend_types.hpp"

#include <string>

namespace up {

std::string build_ninja_install_command(const BuildBackendContext& ctx);

}  // namespace up
