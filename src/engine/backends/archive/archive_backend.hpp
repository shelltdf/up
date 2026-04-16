#pragma once

#include "core/backend_types.hpp"

#include <filesystem>
#include <string>

namespace up {

std::filesystem::path archive_output_path(const PackBackendContext& ctx);
std::string build_archive_command(const PackBackendContext& ctx);

}  // namespace up
