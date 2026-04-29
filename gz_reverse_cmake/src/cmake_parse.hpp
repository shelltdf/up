#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// CMake 语言完整语义含条件求值/宏/生成器等；本工具不实现解释器, 而采用
// **语句级轻量表示**: 每个 `identifier( ... )` 解析为一条「命令节点」, 全文件
// 构成有序命令序列, 在文档中可称为 **Listfile 命令流 / 浅层 AST**
// (statement-level AST), 再交给 cmake_interpret 重解释为包/目标模型.

struct CmakeCommand {
  std::string name;  // 命令名
  std::vector<std::string> args;
  /// Listfile 路径 (L1, UTF-8 可移植字符串)
  std::string file_path;
  /// 行号(命令起首 `identifier` 行)
  std::size_t line{0};
};

struct CmakeParseState {
  std::size_t i{0};
  std::string_view s;
  std::size_t line{1};

  void skip_bom() {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xefu && static_cast<unsigned char>(s[1]) == 0xbbu &&
        static_cast<unsigned char>(s[2]) == 0xbfu)
      i = 3;
  }

  void skip_line_comment();
  void skip_ws();
  void skip_bom_on_line();
  bool at_end() const { return i >= s.size(); }
};

struct CmakeParseResult {
  std::vector<CmakeCommand> commands;
  /// 无法识别为 `name(…)` 的杂散片段(如缺少括号) 等提示
  std::vector<std::string> parse_notes;
};

/// 从**内存**内容解析(不含 file_path, line 有)
std::vector<CmakeCommand> parse_cmake_script(std::string_view text);

/// L1: 与 `parse_cmake_script` 相同, 但为每条命令设置 `file_path` 为 `for_path` 的 portable 字符串
CmakeParseResult parse_cmake_listfile_text(std::string_view text, const std::string &file_path);

