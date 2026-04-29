#include "cmake_parse.hpp"

#include <cctype>

void CmakeParseState::skip_line_comment() {
  if (i < s.size() && s[i] == '#') {
    while (i < s.size() && s[i] != '\n' && s[i] != '\r') i++;
  }
}

void CmakeParseState::skip_bom_on_line() {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
  skip_line_comment();
}

void CmakeParseState::skip_ws() {
  while (i < s.size()) {
    if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') {
      if (s[i] == '\n') line++;
      i++;
    } else if (s[i] == '#') {
      skip_line_comment();
    } else
      break;
  }
}

static void skip_quoted(CmakeParseState &st) {
  // "..." 与转义; 与 CMake 比为子集: \" 与 \\ 在引号内
  if (st.i >= st.s.size() || st.s[st.i] != '"') return;
  st.i++;
  while (st.i < st.s.size()) {
    char c = static_cast<char>(st.s[st.i]);
    if (c == '\\' && st.i + 1 < st.s.size()) {
      st.i += 2;
      continue;
    }
    if (c == '"') {
      st.i++;
      return;
    }
    if (c == '\n') st.line++;
    st.i++;
  }
}

static void skip_bracket(CmakeParseState &st) {
  // [=[  风格 ]=]  子集: 只处理 [[  ... ]]  与 = 数量一致
  if (st.i + 1 < st.s.size() && st.s[st.i] == '[') {
    std::size_t eq = 0, j = st.i + 1;
    while (j < st.s.size() && st.s[j] == '=') {
      eq++;
      j++;
    }
    if (j < st.s.size() && st.s[j] == '[') {
      st.i = j + 1;
      while (st.i < st.s.size()) {
        if (st.s[st.i] == '\n') st.line++;
        if (st.s[st.i] == ']') {
          std::size_t e2 = 0, k = st.i + 1;
          while (k < st.s.size() && st.s[k] == '=') e2++, k++;
          if (e2 == eq && k < st.s.size() && st.s[k] == ']') {
            st.i = k + 1;
            return;
          }
        }
        st.i++;
      }
    }
  }
}

/// 在括号体内扫到与开括号匹配的 ')', 深度从 depth 起算(已在 '(' 后时 depth=1)
static void skip_balanced_parens(CmakeParseState &st, int depth) {
  while (st.i < st.s.size() && depth > 0) {
    if (st.s[st.i] == '"') {
      skip_quoted(st);
      continue;
    }
    if (st.s[st.i] == '#') {
      st.skip_line_comment();
      continue;
    }
    if (st.s[st.i] == '(') {
      depth++;
      st.i++;
      continue;
    }
    if (st.s[st.i] == ')') {
      depth--;
      st.i++;
      continue;
    }
    if (st.s[st.i] == '[') {
      skip_bracket(st);
      continue;
    }
    if (st.s[st.i] == '\n') st.line++;
    st.i++;
  }
}

static bool read_ident(std::string_view t, std::size_t &p, std::string &out) {
  if (p >= t.size()) return false;
  char c0 = static_cast<char>(t[p]);
  if (!(std::isalpha(static_cast<unsigned char>(c0)) || c0 == '_')) return false;
  out.clear();
  while (p < t.size()) {
    char c = static_cast<char>(t[p]);
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':') {
      out += c;
      p++;
    } else
      break;
  }
  return !out.empty();
}

/// 分割一轮括号体成为参数(简化 CMake: 引号/括号/; 在引号内)
void split_cmake_arguments(std::string_view body, std::vector<std::string> &out) {
  out.clear();
  if (body.empty()) return;
  while (!body.empty()) {
    // trim leading
    std::size_t p = 0;
    while (p < body.size() && (body[p] == ' ' || body[p] == '\t' || body[p] == '\n' || body[p] == '\r')) p++;
    if (p >= body.size()) break;
    if (body[p] == '#') {
      while (p < body.size() && body[p] != '\n' && body[p] != '\r') p++;
      if (p >= body.size()) break;
      body = body.substr(p);
      continue;
    }
    std::string one;
    if (body[p] == '"') {
      p++;
      while (p < body.size()) {
        if (body[p] == '\\' && p + 1 < body.size()) {
          one += static_cast<char>(body[p + 1]);
          p += 2;
          continue;
        }
        if (body[p] == '"') {
          p++;
          break;
        }
        one += static_cast<char>(body[p++]);
      }
    } else if (body[p] == '(') {
      int d = 1;
      p++;
      std::string inner = "(";
      while (p < body.size() && d > 0) {
        if (body[p] == '"') {
          inner += '"';
          p++;
          while (p < body.size() && body[p] != '"') {
            if (body[p] == '\\' && p + 1 < body.size()) {
              inner += static_cast<char>(body[p + 1]);
              p += 2;
            } else {
              inner += static_cast<char>(body[p++]);
            }
          }
          if (p < body.size() && body[p] == '"') {
            inner += '"';
            p++;
          }
          continue;
        }
        if (body[p] == '(') d++;
        if (body[p] == ')') d--;
        inner += static_cast<char>(body[p++]);
      }
      one = std::move(inner);
    } else {
      // unquoted: 读到空白或 (  注意 ;
      while (p < body.size() && !std::isspace(static_cast<unsigned char>(body[p]))) {
        if (body[p] == '#') {
          p--;
          break;
        }
        if (body[p] == '"') {
          p--;
          break;  // 下个 token
        }
        if (body[p] == '(') {
          p--;
          break;  // (  作为新 token(极少见) — 与 CMake 不完全一致, 可接受
        }
        one += static_cast<char>(body[p++]);
      }
    }
    out.push_back(std::move(one));
    if (p >= body.size()) break;
    while (p < body.size() && (body[p] == ' ' || body[p] == '\t' || body[p] == '\n' || body[p] == '\r')) p++;
    if (p >= body.size()) break;
    if (body[p] == '#') {
      while (p < body.size() && body[p] != '\n' && body[p] != '\r') p++;
      if (p >= body.size()) break;
    }
    body = body.substr(p);
  }
}

std::vector<CmakeCommand> parse_cmake_script(std::string_view t) {
  CmakeParseState st;
  st.s = t;
  st.skip_bom();
  std::vector<CmakeCommand> out;
  while (true) {
    st.skip_ws();
    if (st.at_end()) break;
    std::string name;
    std::size_t line0 = st.line;
    if (!read_ident(st.s, st.i, name)) {
      st.i++;
      continue;
    }
    st.skip_ws();
    if (st.at_end() || st.s[st.i] != '(') {
      st.skip_ws();
      continue;
    }
    st.i++;  //(
    std::size_t body_start = st.i;
    skip_balanced_parens(st, 1);
    // st.i 指向 ')' 之后; 参数体为 [body_start, st.i-1) 不含 ')'
    if (st.i <= body_start) break;
    std::string_view bodyv = st.s.substr(body_start, (st.i - 1) - body_start);
    CmakeCommand cmd;
    cmd.name = name;
    cmd.line = line0;
    if (!bodyv.empty()) split_cmake_arguments(bodyv, cmd.args);
    out.push_back(std::move(cmd));
  }
  return out;
}

