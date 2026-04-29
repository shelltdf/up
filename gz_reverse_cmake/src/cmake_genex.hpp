#pragma once

#include <string>

namespace gz_cm {

/// L6: 尽力展开 `$<…>` 中与 `gz_reverse_cmake` 相关的常见形态；无法识别则**保留**原样。
void apply_genex_best_effort(std::string *s);

}  // namespace gz_cm
