#pragma once

#include "core/backend_types.hpp"

#include <string>

namespace gz {

std::string build_cmake_build_command(const BuildBackendContext& ctx);
std::string build_cmake_configure_command(const ConfigureBackendContext& ctx);
int write_cmake_lists(const ConfigureGraphModel& model);

}  // namespace gz
