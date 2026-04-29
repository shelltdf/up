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
  std::string name;               // 命令名
  std::vector<std::string> args;  // 括号内按 CMake 规则切分的实参(未做 ${} 完全展开)
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

/// 从单文件内容解析出命令流(不解释 if/foreach, 不展开 macro 与 function 体; function/macro 内可扫描见补充说明)
std::vector<CmakeCommand> parse_cmake_script(std::string_view text);

