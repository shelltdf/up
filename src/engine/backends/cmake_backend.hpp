#pragma once

#include "backend_types.hpp"

#include <string>

namespace up {

std::string build_cmake_build_command(const BuildBackendContext& ctx);
std::string build_cmake_configure_command(const ConfigureBackendContext& ctx);
int write_cmake_lists(const ConfigureGraphModel& model);

}  // namespace up
