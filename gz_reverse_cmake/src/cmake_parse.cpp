#include "cmake_parse.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

namespace {

// L1 解析字节进度: 单处维护节流/合成, 与「切分用 O(n) 下标」解耦, 减少散落的 last/区间 bug
struct L1ByteSink {
  const CmakeParseProgress *const cb{nullptr};
  const std::size_t total{0};
  std::size_t last{0};

  explicit L1ByteSink(const CmakeParseProgress *p, std::size_t total_b) : cb((p && *p) ? p : nullptr), total(total_b) {}

  std::size_t last_emitted() const { return last; }

  void bump_min(std::size_t file_b) { if (file_b > last) last = file_b; }

  void tick(std::size_t file_b, std::size_t line, const char *sub) { emit_with_span(file_b, line, sub, 0); }

  // 小/中等 Listfile 若仍用 2KiB 节流到「再下一格」需跨上千字节, stderr 上会出现长时间同一 m/n(如 6491/7105)的假象;
  // 大文件继续粗步以免刷屏. 到文件尾一帧始终发出.
  void emit_with_span(std::size_t file_b, std::size_t line, const char *sub, std::size_t) {
    if (!cb) return;
    if (file_b > total) file_b = total;
    if (file_b <= last) return;
    if (file_b < total) {
      std::size_t step;
      if (total <= 65536u) {
        // ≤64K: 全程 48 字一档(避免 2KiB 在 7K 级文件上长时间不刷新 m/n)
        step = 48u;
      } else {
        step = 2048u;
        if (last < total && (total - last) < 2100u) step = 64u;
        // 剩余可前进字节比档还小: 缩档, 避免「永远差一格」
        const std::size_t can_move = (last < total) ? (total - last) : 0u;
        if (can_move > 0u && can_move < step) step = std::max<std::size_t>(1u, can_move / 4u);
      }
      if (file_b - last < step) return;
    }
    (*cb)(file_b, total, 0, line, sub);
    last = file_b;
  }

  void absorb_at_least(std::size_t file_b) { if (file_b > last) last = file_b; }
};

/// 切分进度: 与 L1 配平**独立**的 lastp. 小文件: 体坐标每 32 格尝试一次、相对 lastp 进 24 再上报 ——
/// 避免 64*格 + 48*步 叠出「~90 体字节 m/n 不动」的**假死**观感(行尾长 string()/嵌套时常见).
static void emit_during_split(const CmakeParseProgress *cb, std::size_t total_b, std::size_t &lastp, std::size_t file_b, std::size_t line, const char *sub) {
  if (!cb || !*cb) return;
  if (file_b > total_b) file_b = total_b;
  if (file_b <= lastp) return;
  const std::size_t kStep = (total_b <= 65536u) ? 24u : 256u;
  if (file_b - lastp < kStep) return;
  (*cb)(file_b, total_b, 0, line, sub);
  lastp = file_b;
}

// 热循环: 大文件 512 体字节; ≤64K 为**每 32 体**尝试(与 24*步*配合), 仍禁每体一次 callback
static void maybe_report_split(const CmakeParseProgress *cb, std::size_t total_b, std::size_t &lastp, std::size_t file_off0, std::size_t line, const char *sub,
                               std::size_t pos_in_body) {
  if (!cb || !*cb) return;
  if (total_b > 65536u) {
    if ((pos_in_body & 0x1ffu) != 0u) return;
  } else {
    if ((pos_in_body & 0x1fu) != 0u) return;
  }
  emit_during_split(cb, total_b, lastp, file_off0 + pos_in_body, line, sub);
}

// 实参/段结束: 不节流(单 token 多一次, 不叠成数千次)
static void split_checkpoint(const CmakeParseProgress *cb, std::size_t total_b, std::size_t &lastp, std::size_t file_abs, std::size_t line, const char *sub) {
  if (!cb || !*cb) return;
  if (file_abs > total_b) file_abs = total_b;
  if (file_abs <= lastp) return;
  (*cb)(file_abs, total_b, 0, line, sub);
  lastp = file_abs;
}

// 配平 `name(`…`)`: 单下标 st.i, Mode ∈ {N,Q,H,B}, 与旧多函数实现等价

static void skip_balanced_parens(CmakeParseState &st, int depth, L1ByteSink *sink, const char *paren_caption, std::size_t command_line) {
  const char *const cap = (paren_caption && paren_caption[0]) ? paren_caption : "配平'()'";
  enum class M : unsigned char { N, Q, H, B };
  M m = M::N;
  std::size_t b_eq = 0;

  while (st.i < st.s.size() && depth > 0) {
    if (m == M::N) {
      if (sink) sink->tick(st.i, command_line, cap);
      if (st.s[st.i] == '"') {
        st.i++;
        m = M::Q;
        continue;
      }
      if (st.s[st.i] == '#') {
        m = M::H;
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
        const std::size_t start_b = st.i;
        if (st.i + 1u < st.s.size()) {
          std::size_t eq = 0, j = st.i + 1u;
          while (j < st.s.size() && st.s[j] == '=') {
            eq++;
            j++;
          }
          if (j < st.s.size() && st.s[j] == '[') {
            st.i = j + 1u;
            b_eq = eq;
            m = M::B;
            continue;
          }
        }
        if (st.i == start_b) st.i++;
        continue;
      }
      if (st.s[st.i] == '\n') st.line++;
      st.i++;
    } else if (m == M::Q) {
      if (sink && cap) sink->tick(st.i, command_line, cap);
      const char c = static_cast<char>(st.s[st.i]);
      if (c == '\\' && st.i + 1u < st.s.size()) {
        st.i += 2u;
        continue;
      }
      if (c == '"') {
        st.i++;
        m = M::N;
        continue;
      }
      if (c == '\n') st.line++;
      st.i++;
    } else if (m == M::H) {
      if (sink && cap) sink->tick(st.i, command_line, cap);
      if (st.s[st.i] == '\n' || st.s[st.i] == '\r') {
        m = M::N;
        if (st.s[st.i] == '\n') st.line++;
        st.i++;
        continue;
      }
      st.i++;
    } else {
      if (sink && cap) sink->tick(st.i, command_line, cap);
      if (st.s[st.i] == '\n') st.line++;
      if (st.s[st.i] == ']') {
        std::size_t e2 = 0, k = st.i + 1u;
        while (k < st.s.size() && st.s[k] == '=') e2++, k++;
        if (e2 == b_eq && k < st.s.size() && st.s[k] == ']') {
          st.i = k + 1u;
          m = M::N;
          continue;
        }
      }
      st.i++;
    }
  }
}

// 切分: 不 substr 大段, 不每字上 stderr; 双引号/括号/裸 token 三径 + 段末 checkpoint
static void split_cmake_arguments_impl(std::string_view body, std::vector<std::string> &out, const CmakeParseProgress *cb, std::size_t total_b,
                                        std::size_t file_off0, std::size_t line, const char *action_sub, L1ByteSink *sync_sink) {
  out.clear();
  if (body.empty()) return;
  const std::size_t m = body.size();
  std::size_t lastp = file_off0;

  std::size_t pos = 0;
  const auto on_pos = [&](std::size_t p) {
    if (total_b > 65536u && (p & 0x1ffu) != 0u) return;
    emit_during_split(cb, total_b, lastp, file_off0 + p, line, action_sub);
  };

  while (pos < m) {
    while (pos < m && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r')) pos++;
    if (pos >= m) break;
    if (body[pos] == '#') {
      while (pos < m && body[pos] != '\n' && body[pos] != '\r') {
        maybe_report_split(cb, total_b, lastp, file_off0, line, action_sub, pos);
        pos++;
      }
      split_checkpoint(cb, total_b, lastp, std::min(file_off0 + pos, total_b), line, action_sub);
      if (pos >= m) break;
      continue;
    }
    std::string one;
    if (body[pos] == '"') {
      pos++;
      if (pos < m) one.reserve(m - pos + 1u);
      while (pos < m) {
        maybe_report_split(cb, total_b, lastp, file_off0, line, action_sub, pos);
        if (body[pos] == '\\' && pos + 1 < m) {
          one += static_cast<char>(body[pos + 1]);
          pos += 2;
          continue;
        }
        if (body[pos] == '"') {
          pos++;
          break;
        }
        one += static_cast<char>(body[pos++]);
      }
      if (m > 0) split_checkpoint(cb, total_b, lastp, std::min(file_off0 + pos, total_b), line, action_sub);
    } else if (body[pos] == '(') {
      int d = 1;
      pos++;
      one.reserve(m);
      one += '(';
      while (pos < m && d > 0) {
        maybe_report_split(cb, total_b, lastp, file_off0, line, action_sub, pos);
        if (body[pos] == '"') {
          one += '"';
          pos++;
          if (pos < m) one.reserve(one.size() + m - pos + 1u);
          while (pos < m && body[pos] != '"') {
            if (body[pos] == '\\' && pos + 1 < m) {
              one += static_cast<char>(body[pos + 1]);
              pos += 2;
            } else
              one += static_cast<char>(body[pos++]);
          }
          if (pos < m && body[pos] == '"') {
            one += '"';
            pos++;
          }
          continue;
        }
        if (body[pos] == '(') d++;
        if (body[pos] == ')') d--;
        one += static_cast<char>(body[pos++]);
      }
      if (m > 0) split_checkpoint(cb, total_b, lastp, std::min(file_off0 + pos, total_b), line, action_sub);
    } else {
      if (pos < m) one.reserve(m - pos);
      // 遇 `#`/`"`/`(` 时勿 `pos--`: 当前已停在该字符、未写入 one; 曾误用 `pos--` 会回退重读并可能死循环(裸 `x(` 且 pos=1→0)
      while (pos < m && !std::isspace(static_cast<unsigned char>(body[pos]))) {
        if (body[pos] == '#' || body[pos] == '"' || body[pos] == '(') {
          break;
        }
        one += static_cast<char>(body[pos++]);
        maybe_report_split(cb, total_b, lastp, file_off0, line, action_sub, pos);
      }
      if (m > 0) split_checkpoint(cb, total_b, lastp, std::min(file_off0 + pos, total_b), line, action_sub);
    }
    out.push_back(std::move(one));
    if (pos >= m) break;
    while (pos < m && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r')) pos++;
    if (pos >= m) break;
    if (body[pos] == '#') {
      while (pos < m && body[pos] != '\n' && body[pos] != '\r') pos++;
      if (pos >= m) break;
    }
    on_pos(pos);
  }

  if (m > 0 && cb && *cb) {
    const std::size_t end_abs = std::min(file_off0 + m, total_b);
    if (end_abs > lastp) {
      (*cb)(end_abs, total_b, 0, line, action_sub);
      lastp = end_abs;
    }
  }
  if (sync_sink) sync_sink->absorb_at_least(lastp);
}

}  // namespace

void split_cmake_arguments(std::string_view body, std::vector<std::string> &out) { split_cmake_arguments_impl(body, out, nullptr, 0, 0, 0, nullptr, nullptr); }

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

CmakeParseResult parse_cmake_listfile_text(std::string_view t, const std::string &file_path, const CmakeParseProgress *progress) {
  CmakeParseState st;
  st.s = t;
  st.skip_bom();
  CmakeParseResult res;
  const std::size_t total_b = t.size();
  L1ByteSink sink(progress, total_b);

  if (progress && *progress) (*progress)(st.i, total_b, 0, st.line, "扫描");

  while (true) {
    st.skip_ws();
    if (st.at_end()) break;
    std::string name;
    const std::size_t line0 = st.line;
    if (!read_ident(st.s, st.i, name)) {
      if (!file_path.empty() && st.i < st.s.size() && res.parse_notes.size() < 200u) {
        res.parse_notes.push_back(file_path + ":" + std::to_string(st.line) + ": expected command name, skipped byte");
      }
      st.i++;
      continue;
    }
    st.skip_ws();
    if (st.at_end() || st.s[st.i] != '(') {
      if (!file_path.empty()) res.parse_notes.push_back(file_path + ":" + std::to_string(line0) + ": expected '(' after " + name);
      st.i++;
      continue;
    }
    st.i++;
    const std::size_t body_start = st.i;

    std::string paren_caption;
    if (progress && *progress) {
      paren_caption = "配平 ";
      paren_caption += name;
    }
    skip_balanced_parens(st, 1, (progress && *progress) ? &sink : nullptr, paren_caption.empty() ? nullptr : paren_caption.c_str(), line0);
    if (st.i <= body_start) break;

    const std::string_view bodyv = st.s.substr(body_start, (st.i - 1) - body_start);
    CmakeCommand cmd;
    cmd.name = std::move(name);
    cmd.file_path = file_path;
    cmd.line = line0;

    const std::size_t progress_byte_before = sink.last_emitted();
    if (!bodyv.empty()) {
      if (progress && *progress) {
        std::string split_caption = "切分 ";
        split_caption += cmd.name;
        split_cmake_arguments_impl(bodyv, cmd.args, progress, total_b, body_start, line0, split_caption.c_str(), &sink);
      } else
        split_cmake_arguments_impl(bodyv, cmd.args, nullptr, 0, 0, 0, nullptr, nullptr);
    }
    if (progress && *progress) sink.bump_min(st.i);
    res.commands.push_back(std::move(cmd));
    if (progress && *progress) {
      const std::size_t ncmd = res.commands.size();
      const std::size_t th = 256u * 1024u;
      const bool first = ncmd == 1u;
      const bool every = (ncmd % 200u) == 0u;
      const bool big = (st.i > progress_byte_before + th);
      if (first || every || big) {
        (*progress)(st.i, total_b, ncmd, line0, nullptr);
        sink.bump_min(st.i);
      }
    }
  }
  if (progress && *progress) (*progress)(total_b, total_b, res.commands.size(), 0, "词法/解析完成");
  return res;
}

std::vector<CmakeCommand> parse_cmake_script(std::string_view t) { return parse_cmake_listfile_text(t, "", nullptr).commands; }
