#include "simple_xml.hpp"

#include "paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <ostream>
#include <regex>
#include <sstream>

namespace gz {

namespace {

std::string read_all(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open: " + to_posix_path_string(path);
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Minimal XML 1.0 entity decoding for double-quoted attribute values (e.g. `#include &lt;stdint.h&gt;`).
static void xml_unescape_attr_value(std::string& s) {
  for (;;) {
    bool changed = false;
    for (size_t i = 0; i < s.size();) {
      if (s[i] != '&') {
        ++i;
        continue;
      }
      if (s.compare(i, 4, "&lt;") == 0) {
        s.replace(i, 4, "<");
        i += 1;
        changed = true;
        continue;
      }
      if (s.compare(i, 4, "&gt;") == 0) {
        s.replace(i, 4, ">");
        i += 1;
        changed = true;
        continue;
      }
      if (s.compare(i, 6, "&quot;") == 0) {
        s.replace(i, 6, "\"");
        i += 1;
        changed = true;
        continue;
      }
      if (s.compare(i, 5, "&apos;") == 0) {
        s.replace(i, 5, "'");
        i += 1;
        changed = true;
        continue;
      }
      if (s.compare(i, 5, "&amp;") == 0) {
        s.replace(i, 5, "&");
        i += 1;
        changed = true;
        continue;
      }
      ++i;
    }
    if (!changed)
      break;
  }
}

bool attr_string(const std::string& xml, const char* name, std::string& out) {
  std::regex re(std::string(R"rx(\b)rx") + name + R"rx(\s*=\s*"([^"]*)")rx");
  std::smatch m;
  if (!std::regex_search(xml, m, re))
    return false;
  out = m[1].str();
  xml_unescape_attr_value(out);
  return true;
}

std::string trim_copy(std::string s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

// s[open_lt] is '<' of a void element. Returns index one past the closing '>' of `.../>` (or `... />`).
// Respects only double-quoted attributes; '/' inside values (e.g. "a/b") is ignored.
static size_t self_closing_void_end(const std::string& s, size_t open_lt) {
  if (open_lt >= s.size() || s[open_lt] != '<')
    return std::string::npos;
  bool in_dq = false;
  for (size_t i = open_lt + 1; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '"') {
      in_dq = !in_dq;
      continue;
    }
    if (in_dq)
      continue;
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '>')
      return i + 2;
  }
  return std::string::npos;
}

// True if s[open_lt] is '<' and the element is a self-closing `tag .../>` (not `tag>text</tag>`: after the name, not `>`).
static bool void_tag_starts(const std::string& s, size_t open_lt, const char* tag) {
  if (open_lt >= s.size() || s[open_lt] != '<')
    return false;
  size_t t = open_lt + 1;
  while (t < s.size() && (s[t] == ' ' || s[t] == '\t' || s[t] == '\n' || s[t] == '\r' || s[t] == '\f' || s[t] == '\v'))
    ++t;
  const size_t tlen = std::strlen(tag);
  if (s.size() - t < tlen)
    return false;
  for (size_t i = 0; i < tlen; ++i) {
    if (s[t + i] != tag[i])
      return false;
  }
  t += tlen;
  if (t < s.size() && (std::isalnum(static_cast<unsigned char>(s[t])) != 0 || s[t] == '_' || s[t] == '-'))
    return false;
  while (t < s.size() && (s[t] == ' ' || s[t] == '\t' || s[t] == '\n' || s[t] == '\r' || s[t] == '\f' || s[t] == '\v'))
    ++t;
  if (t < s.size() && s[t] == '>')
    return false; // e.g. <file> path </file> — not `.../>`
  return true;
}

static bool is_comment_open(const std::string& s, size_t i) {
  return i + 3 < s.size() && s[i] == '<' && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-';
}
static size_t after_comment(const std::string& s, size_t i) {
  const size_t e = s.find("-->", i + 4);
  if (e == std::string::npos)
    return s.size();
  return e + 3;
}

void parse_gz_binary_layout_attrs(const std::string& head, GzBinaryLayout& b) {
  attr_string(head, "os", b.os);
  attr_string(head, "cpu", b.cpu);
  attr_string(head, "build_system", b.build_system);
  attr_string(head, "toolchain", b.toolchain);
  attr_string(head, "link", b.link);
  attr_string(head, "config", b.config);
  attr_string(head, "crt", b.crt);
  attr_string(head, "arch", b.arch_legacy);
  b.os = trim_copy(b.os);
  b.cpu = trim_copy(b.cpu);
  b.build_system = trim_copy(b.build_system);
  b.toolchain = trim_copy(b.toolchain);
  b.link = trim_copy(b.link);
  b.config = trim_copy(b.config);
  b.crt = trim_copy(b.crt);
  b.arch_legacy = trim_copy(b.arch_legacy);
}

void normalize_gz_binary_layout_in_place(GzBinaryLayout& b) {
  (void)b;
}

void maybe_enrich_layout_from_legacy_arch(GzBinaryLayout& b) {
  if (!b.os.empty() || b.arch_legacy.empty())
    return;
  std::string os, cpu, bs, tc, link, conf, crt;
  if (!try_decompose_compose_arch_tag(b.arch_legacy, os, cpu, bs, tc, link, conf, crt))
    return;
  b.os = std::move(os);
  b.cpu = std::move(cpu);
  b.build_system = std::move(bs);
  b.toolchain = std::move(tc);
  b.link = std::move(link);
  b.config = std::move(conf);
  b.crt = std::move(crt);
  b.arch_legacy.clear();
}

bool is_supported_script_trigger(const std::string& trigger) {
  return trigger == "manual" || trigger == "configure" || trigger == "sources.preprocess" || trigger == "sources.postprocess" ||
         trigger == "headers.preprocess" || trigger == "headers.postprocess" || trigger == "assets.preprocess" ||
         trigger == "assets.postprocess";
}

// Normalized: private | public | interface. On invalid attribute sets `error` and returns "".
std::string parse_target_dependency_visibility(const std::string& frag, std::string& error) {
  std::smatch vm;
  if (!std::regex_search(frag, vm, std::regex(R"rx(\bvisibility\s*=\s*"([^"]*)")rx")))
    return "private";
  std::string raw = trim_copy(vm[1].str());
  std::string v;
  v.reserve(raw.size());
  for (unsigned char uc : raw)
    v.push_back(static_cast<char>(std::tolower(uc)));
  if (v == "private" || v == "public" || v == "interface")
    return v;
  error = "target.xml <dependency>: invalid visibility \"" + raw + "\" (expected private|public|interface)";
  return {};
}

std::string xml_escape_text(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
    switch (c) {
      case '&':
        o += "&amp;";
        break;
      case '<':
        o += "&lt;";
        break;
      case '>':
        o += "&gt;";
        break;
      case '"':
        o += "&quot;";
        break;
      default:
        o += c;
    }
  }
  return o;
}

void write_gz_binary_layout_attrs(std::ostream& out, const GzBinaryLayout& b) {
  if (!b.os.empty())
    out << " os=\"" << xml_escape_text(b.os) << "\"";
  if (!b.cpu.empty())
    out << " cpu=\"" << xml_escape_text(b.cpu) << "\"";
  if (!b.build_system.empty())
    out << " build_system=\"" << xml_escape_text(b.build_system) << "\"";
  if (!b.toolchain.empty())
    out << " toolchain=\"" << xml_escape_text(b.toolchain) << "\"";
  if (!b.link.empty())
    out << " link=\"" << xml_escape_text(b.link) << "\"";
  if (!b.config.empty())
    out << " config=\"" << xml_escape_text(b.config) << "\"";
  if (!b.crt.empty())
    out << " crt=\"" << xml_escape_text(b.crt) << "\"";
}

bool parse_vars_body(const std::string& body,
                     std::vector<std::pair<std::string, std::string>>& out,
                     std::vector<ScriptEntry>& scripts,
                     std::string& error) {
  // Do not use a single-line regex for void `<var .../>` — a greedy `[^>]+` (or a broken alternation) can
  // consume the closing `"/` and match zero `var` rows, leaving package `<vars>` empty. Scan with
  // quote-aware `self_closing_void_end` and parse attributes from the full tag fragment.
  for (size_t p = 0; p < body.size();) {
    const size_t lt = body.find('<', p);
    if (lt == std::string::npos)
      break;
    if (is_comment_open(body, lt)) {
      p = after_comment(body, lt);
      continue;
    }
    if (!void_tag_starts(body, lt, "var")) {
      p = lt + 1;
      continue;
    }
    const size_t end = self_closing_void_end(body, lt);
    if (end == std::string::npos) {
      error = "unclosed <var .../> in <vars> block (missing `/>`?)";
      return false;
    }
    const std::string frag = body.substr(lt, end - lt);
    std::string n;
    if (!attr_string(frag, "name", n)) {
      error = "<var> requires name=\"...\"";
      return false;
    }
    n = trim_copy(n);
    std::string v;
    if (!attr_string(frag, "value", v))
      v.clear();
    else
      v = trim_copy(v);
    std::string var_type;
    if (attr_string(frag, "type", var_type))
      var_type = trim_copy(var_type);
    if (n.empty()) {
      error = "<var> name cannot be empty";
      return false;
    }
    if (var_type == "script") {
      ScriptEntry se;
      se.name = n;
      if (!attr_string(frag, "trigger", se.trigger))
        se.trigger = "manual";
      if (!attr_string(frag, "script_type", se.script_type)) {
        if (!attr_string(frag, "lang", se.script_type))
          se.script_type = "lua";
      }
      se.trigger = trim_copy(se.trigger);
      se.script_type = trim_copy(se.script_type);
      if (se.trigger.empty() || !is_supported_script_trigger(se.trigger)) {
        error = "<var type=\"script\"> trigger=\"" + se.trigger +
                "\" is not supported (expected manual|configure|sources.preprocess|sources.postprocess|"
                "headers.preprocess|headers.postprocess|assets.preprocess|assets.postprocess)";
        return false;
      }
      se.source = std::move(v);
      scripts.push_back(std::move(se));
    } else {
      out.emplace_back(std::move(n), std::move(v));
    }
    p = end;
  }
  return true;
}

bool parse_defines_body(const std::string& body, std::vector<DefineEntry>& out, std::string& error, const char* ctx) {
  static const std::regex define_name_ok(R"rx(^[A-Za-z_][A-Za-z0-9_]*$)rx");
  static const std::regex define_value_ok(R"rx(^[A-Za-z0-9_.+\-/]*$)rx");
  for (size_t p = 0; p < body.size();) {
    const size_t lt = body.find('<', p);
    if (lt == std::string::npos)
      break;
    if (is_comment_open(body, lt)) {
      p = after_comment(body, lt);
      continue;
    }
    if (!void_tag_starts(body, lt, "define")) {
      p = lt + 1;
      continue;
    }
    const size_t end = self_closing_void_end(body, lt);
    if (end == std::string::npos) {
      error = std::string(ctx) + ": unclosed <define .../> (missing `/>`?)";
      return false;
    }
    const std::string frag = body.substr(lt, end - lt);
    DefineEntry de;
    if (!attr_string(frag, "name", de.name)) {
      error = std::string(ctx) + ": <define> requires name=\"...\" attribute";
      return false;
    }
    de.name = trim_copy(de.name);
    if (!attr_string(frag, "value", de.value))
      de.value.clear();
    else
      de.value = trim_copy(de.value);
    if (de.name.empty()) {
      error = std::string(ctx) + ": <define> name cannot be empty";
      return false;
    }
    if (!std::regex_match(de.name, define_name_ok)) {
      error = std::string(ctx) + ": <define> name must be a C identifier: " + de.name;
      return false;
    }
    if (!de.value.empty() && !std::regex_match(de.value, define_value_ok)) {
      error = std::string(ctx) + ": <define> value may only use letters, digits, and ._+-/ (no spaces): " + de.name;
      return false;
    }
    out.push_back(std::move(de));
    p = end;
  }
  return true;
}

bool parse_flag_arg_body(const std::string& body, std::vector<std::string>& out, std::string& /*error*/) {
  std::regex arg_re(R"rx(<\s*arg\s*>\s*([^<]*?)\s*</\s*arg\s*>)rx");
  for (std::sregex_iterator it(body.begin(), body.end(), arg_re), end; it != end; ++it) {
    const std::string t = trim_copy((*it)[1].str());
    if (!t.empty())
      out.push_back(t);
  }
  return true;
}

bool parse_config_files_body(const std::string& body, std::vector<ConfigFileEntry>& out, std::string& error) {
  for (size_t p = 0; p < body.size();) {
    const size_t lt = body.find('<', p);
    if (lt == std::string::npos)
      break;
    if (is_comment_open(body, lt)) {
      p = after_comment(body, lt);
      continue;
    }
    if (!void_tag_starts(body, lt, "file")) {
      p = lt + 1;
      continue;
    }
    const size_t end = self_closing_void_end(body, lt);
    if (end == std::string::npos) {
      error = "<config_files> unclosed <file .../> (missing `/>`?)";
      return false;
    }
    const std::string frag = body.substr(lt, end - lt);
    ConfigFileEntry e;
    if (!attr_string(frag, "in", e.in)) {
      error = "<config_files> <file> requires in=\"...\"";
      return false;
    }
    e.in = trim_copy(e.in);
    if (!attr_string(frag, "to", e.to)) {
      error = "<config_files> <file> requires to=\"...\" (safe relative path under generated/<pkg>/_package/ or "
              "generated/<pkg>/<target>/)";
      return false;
    }
    e.to = trim_copy(e.to);
    if (e.in.empty() || e.to.empty()) {
      error = "<config_files> in= and to= cannot be empty";
      return false;
    }
    out.push_back(std::move(e));
    p = end;
  }
  return true;
}

void parse_stage_commands(const std::string& body, std::string& pre, std::string& post) {
  std::smatch m;
  if (std::regex_search(body, m, std::regex(R"rx(<\s*preprocess\s+[^>]*command\s*=\s*"([^"]+)"[^>]*/>)rx")))
    pre = trim_copy(m[1].str());
  else
    pre.clear();
  if (std::regex_search(body, m, std::regex(R"rx(<\s*postprocess\s+[^>]*command\s*=\s*"([^"]+)"[^>]*/>)rx")))
    post = trim_copy(m[1].str());
  else
    post.clear();
}

/** Every `<tag>...</tag>` pair in document order; `fn(body, err)` appends to model. Tag must be ASCII `[A-Za-z_][A-Za-z0-9_-]*`. */
template <typename Fn>
bool for_each_balanced_children(const std::string& raw, const char* tag, Fn&& fn, std::string& error) {
  const std::string open = std::string("<") + tag;
  const std::string close = std::string("</") + tag + ">";
  const size_t open_len = open.size();
  size_t pos = 0;
  while (true) {
    const size_t open_at = raw.find(open, pos);
    if (open_at == std::string::npos)
      break;
    const size_t after_name = open_at + open_len;
    if (after_name < raw.size()) {
      const unsigned char uc = static_cast<unsigned char>(raw[after_name]);
      if (std::isalnum(uc) || uc == '_' || uc == '-' || uc == ':') {
        pos = open_at + 1;
        continue;
      }
    }
    const size_t gt = raw.find('>', open_at);
    if (gt == std::string::npos) {
      error = std::string("invalid <") + tag + "> (missing `>`)";
      return false;
    }
    const size_t close_at = raw.find(close, gt + 1);
    if (close_at == std::string::npos) {
      error = std::string("unclosed <") + tag + "> (expected " + close + ")";
      return false;
    }
    const std::string body = raw.substr(gt + 1, close_at - gt - 1);
    if (!fn(body, error))
      return false;
    pos = close_at + close.size();
  }
  return true;
}

bool append_sources_from_body(const std::string& body, TargetDesc& out, std::string& error) {
  struct SourcePending {
    size_t pos;
    TargetDesc::SourceEntry se;
  };
  std::vector<SourcePending> ordered;
  ordered.reserve(8);
  for (size_t p = 0; p < body.size();) {
    const size_t lt = body.find('<', p);
    if (lt == std::string::npos)
      break;
    if (is_comment_open(body, lt)) {
      p = after_comment(body, lt);
      continue;
    }
    const char* kind = nullptr;
    if (void_tag_starts(body, lt, "file"))
      kind = "file";
    else if (void_tag_starts(body, lt, "glob"))
      kind = "glob";
    if (!kind) {
      p = lt + 1;
      continue;
    }
    const size_t end = self_closing_void_end(body, lt);
    if (end == std::string::npos) {
      error = "sources: unclosed <" + std::string(kind) + " .../> (missing `/>`?)";
      return false;
    }
    const std::string frag = body.substr(lt, end - lt);
    TargetDesc::SourceEntry se;
    se.kind = kind;
    if (!attr_string(frag, "from", se.from)) {
      error = "sources <" + se.kind + " .../> requires from=\"...\"";
      return false;
    }
    se.from = trim_copy(se.from);
    if (!attr_string(frag, "when", se.when))
      se.when.clear();
    else
      se.when = trim_copy(se.when);
    if (se.from.empty()) {
      error = "sources entry from cannot be empty";
      return false;
    }
    ordered.push_back({lt, std::move(se)});
    p = end;
  }
  std::regex src_re(R"rx(<\s*(file|glob)\s*([^>]*)>([\s\S]*?)</\s*\1\s*>)rx");
  for (std::sregex_iterator it(body.begin(), body.end(), src_re), end; it != end; ++it) {
    TargetDesc::SourceEntry se;
    se.kind = trim_copy((*it)[1].str());
    const std::string attrs = (*it)[2].str();
    std::string inner = (*it)[3].str();
    parse_stage_commands(inner, se.preprocess_command, se.postprocess_command);
    inner = std::regex_replace(inner, std::regex(R"rx(<\s*preprocess\s+[^>]*/>)rx"), "");
    inner = std::regex_replace(inner, std::regex(R"rx(<\s*postprocess\s+[^>]*/>)rx"), "");
    inner = trim_copy(inner);
    if (!attr_string(attrs, "from", se.from))
      se.from = inner;
    se.from = trim_copy(se.from);
    if (!attr_string(attrs, "when", se.when))
      se.when.clear();
    else
      se.when = trim_copy(se.when);
    if (se.from.empty()) {
      error = "sources entry requires file path or from attribute";
      return false;
    }
    ordered.push_back({static_cast<size_t>(std::distance(body.begin(), (*it)[0].first)), std::move(se)});
  }
  std::sort(ordered.begin(), ordered.end(), [](const SourcePending& a, const SourcePending& b) { return a.pos < b.pos; });
  for (auto& sp : ordered) {
    out.source_entries.push_back(std::move(sp.se));
    if (out.source_entries.back().kind == "file")
      out.sources.push_back(out.source_entries.back().from);
  }
  return true;
}

bool append_headers_from_body(const std::string& body, TargetDesc& out, std::string& error) {
  std::regex item_re(R"rx(<\s*([A-Za-z_][A-Za-z0-9_]*)\s*([^>]*?)(?:/>|>([\s\S]*?)</\s*\1\s*>))rx");
  for (std::sregex_iterator it(body.begin(), body.end(), item_re), end; it != end; ++it) {
    TargetDesc::IncludeEntry inc;
    inc.kind = trim_copy((*it)[1].str());
    if (!(inc.kind == "dir" || inc.kind == "file" || inc.kind == "glob")) {
      error = "unsupported <headers> entry: " + inc.kind + " (expected dir/file/glob)";
      return false;
    }
    const std::string attrs = (*it)[2].str();
    if (!attr_string(attrs, "from", inc.from)) {
      error = "<headers> entry requires from attribute";
      return false;
    }
    inc.from = trim_copy(inc.from);
    if (inc.from.empty()) {
      error = "<headers> entry from attribute cannot be empty";
      return false;
    }
    if (!attr_string(attrs, "to", inc.to))
      inc.to.clear();
    else
      inc.to = trim_copy(inc.to);
    parse_stage_commands((*it)[3].str(), inc.preprocess_command, inc.postprocess_command);
    if (!attr_string(attrs, "when", inc.when))
      inc.when.clear();
    else
      inc.when = trim_copy(inc.when);
    out.includes.push_back(std::move(inc));
  }
  std::regex old_style_re(R"rx(<\s*dir\s*>\s*[^<]+\s*</\s*dir\s*>)rx");
  if (std::regex_search(body, old_style_re)) {
    error = "old nested <dir>path</dir> syntax under <headers> is not supported; use <dir from=\"...\" to=\"...\"/>";
    return false;
  }
  return true;
}

bool append_assets_from_body(const std::string& body, TargetDesc& out, std::string& error) {
  std::regex item_re(R"rx(<\s*([A-Za-z_][A-Za-z0-9_]*)\s*([^>]*?)(?:/>|>([\s\S]*?)</\s*\1\s*>))rx");
  for (std::sregex_iterator it(body.begin(), body.end(), item_re), end; it != end; ++it) {
    TargetDesc::AssetEntry ae;
    ae.kind = trim_copy((*it)[1].str());
    if (!(ae.kind == "dir" || ae.kind == "file" || ae.kind == "glob")) {
      error = "unsupported assets entry: " + ae.kind + " (expected dir/file/glob)";
      return false;
    }
    const std::string attrs = (*it)[2].str();
    if (!attr_string(attrs, "from", ae.from)) {
      error = "assets entry requires from attribute";
      return false;
    }
    ae.from = trim_copy(ae.from);
    if (!attr_string(attrs, "to", ae.to))
      ae.to.clear();
    else
      ae.to = trim_copy(ae.to);
    parse_stage_commands((*it)[3].str(), ae.preprocess_command, ae.postprocess_command);
    out.assets.push_back(std::move(ae));
  }
  return true;
}

void parse_all_prebuilt_void_tags(const std::string& raw, TargetDesc& out) {
  const std::string open = "<prebuilt";
  size_t scan = 0;
  while (true) {
    const size_t po = raw.find(open, scan);
    if (po == std::string::npos)
      break;
    const size_t after = po + open.size();
    if (after < raw.size()) {
      const unsigned char uc = static_cast<unsigned char>(raw[after]);
      if (std::isalnum(uc) || uc == '_' || uc == '-' || uc == ':') {
        scan = po + 1;
        continue;
      }
    }
    const size_t gt = raw.find('>', po);
    if (gt == std::string::npos)
      break;
    const std::string head = raw.substr(po, gt - po + 1);
    TargetDesc::PrebuiltDesc pb;
    attr_string(head, "import_lib", pb.import_lib);
    attr_string(head, "location", pb.location);
    attr_string(head, "dll", pb.dll);
    pb.import_lib = trim_copy(pb.import_lib);
    pb.location = trim_copy(pb.location);
    pb.dll = trim_copy(pb.dll);
    parse_gz_binary_layout_attrs(head, pb.layout);
    normalize_gz_binary_layout_in_place(pb.layout);
    maybe_enrich_layout_from_legacy_arch(pb.layout);
    if (!pb.import_lib.empty() || !pb.location.empty() || !pb.dll.empty() || !pb.layout.empty())
      out.prebuilt = std::move(pb);
    scan = gt + 1;
  }
}

}  // namespace

static std::string extract_cmake_prelude_body(std::string body) {
  auto trim_edges = [](std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    s = s.substr(a, b - a);
  };
  trim_edges(body);
  if (body.size() >= 9 && body.compare(0, 9, "<![CDATA[") == 0) {
    const size_t cend = body.find("]]>");
    if (cend != std::string::npos && cend > 9)
      body = body.substr(9, cend - 9);
  }
  trim_edges(body);
  return body;
}

bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error) {
  const std::string raw = read_all(path, error);
  if (raw.empty() && !error.empty())
    return false;
  const size_t pkg_pos = raw.find("<package");
  if (pkg_pos == std::string::npos) {
    error = "missing <package> root";
    return false;
  }
  const size_t pkg_gt = raw.find('>', pkg_pos);
  const std::string pkg_head =
      raw.substr(pkg_pos, pkg_gt == std::string::npos ? std::string::npos : pkg_gt - pkg_pos + 1);
  if (!attr_string(pkg_head, "name", out.name)) {
    error = "package name attribute required";
    return false;
  }
  if (!attr_string(pkg_head, "version", out.version))
    out.version = "0.0.0";

  std::regex dep_re(R"rx(<dependency\s+[^>]*name\s*=\s*"([^"]*)"[^>]*/>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), dep_re), end; it != end; ++it) {
    const std::string dep_name = (*it)[1].str();
    bool optional = false;
    const std::string frag = (*it)[0].str();
    std::smatch om;
    if (std::regex_search(frag, om, std::regex(R"rx(optional\s*=\s*"([^"]*)")rx"))) {
      const std::string v = om[1].str();
      optional = (v == "1" || v == "true" || v == "yes");
    }
    out.dependencies.emplace_back(dep_name, optional);
  }

  if (!for_each_balanced_children(
          raw, "vars",
          [&](const std::string& body, std::string& err) { return parse_vars_body(body, out.vars, out.scripts, err); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "defines",
          [&](const std::string& body, std::string& err) { return parse_defines_body(body, out.defines, err, "package.xml"); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "config_files",
          [&](const std::string& body, std::string& err) { return parse_config_files_body(body, out.config_files, err); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "cmake_prelude",
          [&](const std::string& body, std::string&) {
            const std::string piece = extract_cmake_prelude_body(body);
            if (piece.empty())
              return true;
            if (!out.cmake_prelude.empty())
              out.cmake_prelude += "\n\n";
            out.cmake_prelude += piece;
            return true;
          },
          error))
    return false;

  error.clear();
  return true;
}

bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error) {
  const std::string raw = read_all(path, error);
  if (raw.empty() && !error.empty())
    return false;
  const size_t tgt_pos = raw.find("<target");
  if (tgt_pos == std::string::npos) {
    error = "missing <target> root";
    return false;
  }
  const size_t tgt_gt = raw.find('>', tgt_pos);
  const std::string tgt_head =
      raw.substr(tgt_pos, tgt_gt == std::string::npos ? std::string::npos : tgt_gt - tgt_pos + 1);
  if (!attr_string(tgt_head, "name", out.name)) {
    error = "target name attribute required";
    return false;
  }
  if (!attr_string(tgt_head, "type", out.type))
    out.type = "executable";

  bool any_sources_block = false;
  if (!for_each_balanced_children(
          raw, "sources",
          [&](const std::string& body, std::string& err) {
            any_sources_block = true;
            return append_sources_from_body(body, out, err);
          },
          error))
    return false;
  if (!any_sources_block) {
    std::regex file_re(R"rx(<file\s*>\s*([^<]+)\s*</file\s*>)rx");
    for (std::sregex_iterator it(raw.begin(), raw.end(), file_re), end; it != end; ++it) {
      out.sources.push_back((*it)[1].str());
      out.source_entries.push_back({"file", trim_copy((*it)[1].str()), "", "", ""});
    }
  }

  std::regex dep_re(R"rx(<dependency\s+[^>]*name\s*=\s*"([^"]*)"[^>]*/>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), dep_re), end; it != end; ++it) {
    const std::string frag = (*it)[0].str();
    TargetDesc::DependencyEntry de;
    de.name = (*it)[1].str();
    de.visibility = parse_target_dependency_visibility(frag, error);
    if (!error.empty())
      return false;
    out.dependencies.push_back(std::move(de));
  }

  if (!for_each_balanced_children(
          raw, "defines",
          [&](const std::string& body, std::string& err) { return parse_defines_body(body, out.defines, err, "target.xml"); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "compile_flags",
          [&](const std::string& body, std::string& err) { return parse_flag_arg_body(body, out.compile_flags, err); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "link_flags",
          [&](const std::string& body, std::string& err) { return parse_flag_arg_body(body, out.link_flags, err); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "vars",
          [&](const std::string& body, std::string& err) { return parse_vars_body(body, out.vars, out.scripts, err); },
          error))
    return false;
  if (!for_each_balanced_children(
          raw, "config_files",
          [&](const std::string& body, std::string& err) { return parse_config_files_body(body, out.config_files, err); },
          error))
    return false;

  if (raw.find("<includes") != std::string::npos) {
    error = "target.xml no longer supports <includes>; use <headers>...</headers>";
    return false;
  }
  if (!for_each_balanced_children(
          raw, "headers",
          [&](const std::string& body, std::string& err) { return append_headers_from_body(body, out, err); }, error))
    return false;

  if (!for_each_balanced_children(
          raw, "assets",
          [&](const std::string& body, std::string& err) { return append_assets_from_body(body, out, err); }, error))
    return false;

  parse_all_prebuilt_void_tags(raw, out);

  error.clear();
  return true;
}

bool write_package_xml(std::ostream& out, const PackageDesc& pkg) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<package name=\"" << xml_escape_text(pkg.name) << "\" version=\"" << xml_escape_text(pkg.version) << "\">\n";
  for (const auto& d : pkg.dependencies) {
    out << "  <dependency name=\"" << xml_escape_text(d.first) << "\" optional=\""
        << (d.second ? "true" : "false") << "\"/>\n";
  }
  if (!pkg.vars.empty() || !pkg.scripts.empty()) {
    out << "  <vars>\n";
    for (const auto& v : pkg.vars) {
      out << "    <var name=\"" << xml_escape_text(v.first) << "\"";
      if (!v.second.empty())
        out << " value=\"" << xml_escape_text(v.second) << "\"";
      out << "/>\n";
    }
    for (const auto& s : pkg.scripts) {
      out << "    <var name=\"" << xml_escape_text(s.name) << "\" type=\"script\"";
      out << " script_type=\"" << xml_escape_text(s.script_type.empty() ? "lua" : s.script_type) << "\"";
      if (!s.trigger.empty())
        out << " trigger=\"" << xml_escape_text(s.trigger) << "\"";
      if (!s.source.empty())
        out << " value=\"" << xml_escape_text(s.source) << "\"";
      out << "/>\n";
    }
    out << "  </vars>\n";
  }
  if (!pkg.defines.empty()) {
    out << "  <defines>\n";
    for (const auto& d : pkg.defines) {
      out << "    <define name=\"" << xml_escape_text(d.name) << "\"";
      if (!d.value.empty())
        out << " value=\"" << xml_escape_text(d.value) << "\"";
      out << "/>\n";
    }
    out << "  </defines>\n";
  }
  if (!pkg.config_files.empty()) {
    out << "  <config_files>\n";
    for (const auto& cf : pkg.config_files)
      out << "    <file in=\"" << xml_escape_text(cf.in) << "\" to=\"" << xml_escape_text(cf.to) << "\"/>\n";
    out << "  </config_files>\n";
  }
  if (!pkg.cmake_prelude.empty()) {
    out << "  <cmake_prelude><![CDATA[" << pkg.cmake_prelude << "]]></cmake_prelude>\n";
  }
  out << "</package>\n";
  return static_cast<bool>(out);
}

bool write_target_xml(std::ostream& out, const TargetDesc& desc) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<target name=\"" << xml_escape_text(desc.name) << "\" type=\"" << xml_escape_text(desc.type) << "\">\n";
  const bool skip_sources =
      desc.type == "asset_bundle" || desc.type == "prebuilt_static_library" || desc.type == "prebuilt_shared_library";
  if (!desc.vars.empty() || !desc.scripts.empty()) {
    out << "  <vars>\n";
    for (const auto& v : desc.vars) {
      out << "    <var name=\"" << xml_escape_text(v.first) << "\"";
      if (!v.second.empty())
        out << " value=\"" << xml_escape_text(v.second) << "\"";
      out << "/>\n";
    }
    for (const auto& s : desc.scripts) {
      out << "    <var name=\"" << xml_escape_text(s.name) << "\" type=\"script\"";
      out << " script_type=\"" << xml_escape_text(s.script_type.empty() ? "lua" : s.script_type) << "\"";
      if (!s.trigger.empty())
        out << " trigger=\"" << xml_escape_text(s.trigger) << "\"";
      if (!s.source.empty())
        out << " value=\"" << xml_escape_text(s.source) << "\"";
      out << "/>\n";
    }
    out << "  </vars>\n";
  }
  if (!desc.config_files.empty()) {
    out << "  <config_files>\n";
    for (const auto& cf : desc.config_files)
      out << "    <file in=\"" << xml_escape_text(cf.in) << "\" to=\"" << xml_escape_text(cf.to) << "\"/>\n";
    out << "  </config_files>\n";
  }
  if (!skip_sources || !desc.source_entries.empty() || !desc.sources.empty()) {
    out << "  <sources>\n";
    if (!desc.source_entries.empty()) {
      for (const auto& se : desc.source_entries) {
        const bool has_stage = !se.preprocess_command.empty() || !se.postprocess_command.empty();
        if (se.kind == "glob") {
          if (has_stage) {
            out << "    <glob from=\"" << xml_escape_text(se.from) << "\"";
            if (!se.when.empty())
              out << " when=\"" << xml_escape_text(se.when) << "\"";
            out << ">\n";
            if (!se.preprocess_command.empty())
              out << "      <preprocess command=\"" << xml_escape_text(se.preprocess_command) << "\"/>\n";
            if (!se.postprocess_command.empty())
              out << "      <postprocess command=\"" << xml_escape_text(se.postprocess_command) << "\"/>\n";
            out << "    </glob>\n";
          } else {
            out << "    <glob from=\"" << xml_escape_text(se.from) << "\"";
            if (!se.when.empty())
              out << " when=\"" << xml_escape_text(se.when) << "\"";
            out << "/>\n";
          }
        } else if (has_stage) {
          out << "    <file from=\"" << xml_escape_text(se.from) << "\"";
          if (!se.when.empty())
            out << " when=\"" << xml_escape_text(se.when) << "\"";
          out << ">\n";
          if (!se.preprocess_command.empty())
            out << "      <preprocess command=\"" << xml_escape_text(se.preprocess_command) << "\"/>\n";
          if (!se.postprocess_command.empty())
            out << "      <postprocess command=\"" << xml_escape_text(se.postprocess_command) << "\"/>\n";
          out << "    </file>\n";
        } else if (!se.when.empty()) {
          out << "    <file from=\"" << xml_escape_text(se.from) << "\" when=\"" << xml_escape_text(se.when) << "\"/>\n";
        } else {
          out << "    <file>" << xml_escape_text(se.from.empty() ? "" : se.from) << "</file>\n";
        }
      }
    } else {
      for (const auto& s : desc.sources)
        out << "    <file>" << xml_escape_text(s) << "</file>\n";
    }
    out << "  </sources>\n";
  }
  if (desc.prebuilt.has_value()) {
    const auto& pb = *desc.prebuilt;
    out << "  <prebuilt";
    if (!pb.import_lib.empty())
      out << " import_lib=\"" << xml_escape_text(pb.import_lib) << "\"";
    if (!pb.location.empty())
      out << " location=\"" << xml_escape_text(pb.location) << "\"";
    if (!pb.dll.empty())
      out << " dll=\"" << xml_escape_text(pb.dll) << "\"";
    write_gz_binary_layout_attrs(out, pb.layout);
    out << "/>\n";
  }
  if (!desc.dependencies.empty()) {
    for (const auto& d : desc.dependencies) {
      out << "  <dependency name=\"" << xml_escape_text(d.name) << "\"";
      if (!d.visibility.empty() && d.visibility != "private")
        out << " visibility=\"" << xml_escape_text(d.visibility) << "\"";
      out << "/>\n";
    }
  }
  if (!desc.defines.empty()) {
    out << "  <defines>\n";
    for (const auto& d : desc.defines) {
      out << "    <define name=\"" << xml_escape_text(d.name) << "\"";
      if (!d.value.empty())
        out << " value=\"" << xml_escape_text(d.value) << "\"";
      out << "/>\n";
    }
    out << "  </defines>\n";
  }
  if (!desc.compile_flags.empty()) {
    out << "  <compile_flags>\n";
    for (const auto& a : desc.compile_flags)
      out << "    <arg>" << xml_escape_text(a) << "</arg>\n";
    out << "  </compile_flags>\n";
  }
  if (!desc.link_flags.empty()) {
    out << "  <link_flags>\n";
    for (const auto& a : desc.link_flags)
      out << "    <arg>" << xml_escape_text(a) << "</arg>\n";
    out << "  </link_flags>\n";
  }
  if (!desc.includes.empty()) {
    out << "  <headers>\n";
    for (const auto& inc : desc.includes) {
      out << "    <" << inc.kind << " from=\"" << xml_escape_text(inc.from) << "\"";
      if (!inc.to.empty())
        out << " to=\"" << xml_escape_text(inc.to) << "\"";
      if (!inc.when.empty())
        out << " when=\"" << xml_escape_text(inc.when) << "\"";
      out << "/>\n";
    }
    out << "  </headers>\n";
  } else if (desc.type == "executable" || desc.type == "library" || desc.type == "static_library" ||
             desc.type == "shared_library") {
    out << "  <headers>\n";
    out << "    <dir from=\".\"/>\n";
    out << "  </headers>\n";
  }
  if (!desc.assets.empty()) {
    out << "  <assets>\n";
    for (const auto& ae : desc.assets) {
      out << "    <" << ae.kind << " from=\"" << xml_escape_text(ae.from) << "\"";
      if (!ae.to.empty())
        out << " to=\"" << xml_escape_text(ae.to) << "\"";
      out << "/>\n";
    }
    out << "  </assets>\n";
  }
  out << "</target>\n";
  return static_cast<bool>(out);
}

bool write_package_xml(const std::filesystem::path& path, const PackageDesc& pkg, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  write_package_xml(f, pkg);
  error.clear();
  return true;
}

bool write_target_xml(const std::filesystem::path& path, const TargetDesc& desc, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  write_target_xml(f, desc);
  error.clear();
  return true;
}

}  // namespace gz
