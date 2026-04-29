#pragma once

#include <string>
#include <vector>

/// L7: 从用户预置的 File API / codemodel 风格 JSON 文本中**提取** `target` 对象内出现的 `"name"`
/// (不运行 cmake, 不保证与当前扫描树一致, 无第三方 JSON 库, 为尽力解析).
std::vector<std::string> ingest_target_names_from_codemodel_json(const std::string &json_text);
