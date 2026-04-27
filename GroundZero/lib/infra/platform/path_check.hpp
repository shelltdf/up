#pragma once

#include <filesystem>

namespace gz {

// 设计：路径仅支持 ASCII 可移植子集；含非 ASCII 时在 configure 阶段报错（见 mindmap / DESIGN）。
bool path_has_non_ascii(const std::filesystem::path& path);

}  // namespace gz
