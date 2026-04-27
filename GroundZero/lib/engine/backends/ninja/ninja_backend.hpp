#pragma once

#include "core/backend_types.hpp"

#include <string>

namespace gz {

std::string build_ninja_install_command(const BuildBackendContext& ctx);
int write_ninja_file(const ConfigureGraphModel& model);

}  // namespace gz
