#pragma once

#include <filesystem>
#include <string>

namespace gz {

/** When `no_emit_redistribution_xml` is true, skip writing `<install>/gz-redist/`. Otherwise emit unless `GZ_EMIT_REDIST_XML` is explicitly falsy. */
int cmd_build(const std::filesystem::path& cwd, const std::filesystem::path& build_dir, bool no_emit_redistribution_xml);

}  // namespace gz
