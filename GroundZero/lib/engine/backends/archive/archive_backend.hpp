#pragma once

#include "core/backend_types.hpp"

#include <filesystem>
#include <string>

namespace gz {

std::filesystem::path archive_output_path(const PackBackendContext& ctx);
std::string build_archive_command(const PackBackendContext& ctx);
std::string build_cpack_command(const PackBackendContext& ctx);

}  // namespace gz
