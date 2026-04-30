#include "cmake_interpret.hpp"

#include "cmake_genex.hpp"
#include "file_api_ingest.hpp"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

static std::string read_file_bin(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return {};
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xefu && static_cast<unsigned char>(s[1]) == 0xbbu &&
      static_cast<unsigned char>(s[2]) == 0xbfu)
    s.erase(0, 3);
  return s;
}

static void to_lower(std::string *s) {
  for (char &c : *s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

static void expand_vars(std::string *s, const std::unordered_map<std::string, std::string> &vars) {
  for (int r = 0; r < 8; ++r) {
    bool chg = false;
    for (const auto &kv : vars) {
      const std::string k = "${" + kv.first + "}";
      std::size_t p = 0;
      while ((p = s->find(k, p)) != std::string::npos) {
        s->replace(p, k.size(), kv.second);
        chg = true;
        p += kv.second.size();
      }
    }
    if (!chg) break;
  }
}

static std::string expc(std::string t, const std::unordered_map<std::string, std::string> &vars) {
  expand_vars(&t, vars);
  gz_cm::apply_genex_best_effort(&t);
  return t;
}

static bool string_has_cmake_deref(const std::string &s) { return s.find("${") != std::string::npos; }

static std::vector<std::string> split_list(const std::string &s) {
  if (s.empty()) return {};
  std::vector<std::string> o;
  std::string c;
  for (char ch : s) {
    if (ch == ';') {
      o.push_back(c);
      c.clear();
    } else
      c += ch;
  }
  o.push_back(c);
  return o;
}

/// target_compile_options / target_link_options: skip visibility keywords, split CMake lists, ignore generator expr.
static void collect_target_options_list(const CmakeCommand &c, const std::unordered_map<std::string, std::string> &vars,
                                        std::vector<std::string> *out) {
  for (std::size_t j = 1; j < c.args.size(); ++j) {
    std::string tok = c.args[j];
    std::string tlow = tok;
    to_lower(&tlow);
    if (tlow == "private" || tlow == "public" || tlow == "interface" || tlow == "before" || tlow == "after" || tlow == "system" ||
        tlow == "key" || tlow == "sublink_options" || tlow == "file_set")
      continue;
    if (tok.find("$<") != std::string::npos) continue;
    for (const std::string &seg : split_list(expc(tok, vars))) {
      if (seg.find("$<") != std::string::npos) continue;
      if (seg.empty()) continue;
      out->push_back(seg);
    }
  }
}

static void handle_set(const CmakeCommand &c, std::unordered_map<std::string, std::string> *vars) {
  if (c.args.empty()) return;
  const std::string &n = c.args[0];
  std::vector<std::string> vals;
  for (std::size_t j = 1; j < c.args.size(); ++j) {
    if (c.args[j] == "PARENT_SCOPE" || c.args[j] == "CACHE" || c.args[j] == "FORCE" || c.args[j] == "INTERNAL" ||
        c.args[j] == "BOOL" || c.args[j] == "STRING" || c.args[j] == "FILEPATH" || c.args[j] == "PATH" ||
        c.args[j] == "DOCSTRING")
      break;
    vals.push_back(c.args[j]);
  }
  std::string jn;
  for (std::size_t v = 0; v < vals.size(); ++v) {
    if (v) jn += ';';
    jn += expc(vals[v], *vars);
  }
  (*vars)[n] = jn;
}

static int block_step(const std::string &n) {
  if (n == "function" || n == "macro" || n == "foreach" || n == "while") return 1;
  if (n == "endfunction" || n == "endmacro" || n == "endforeach" || n == "endwhile") return -1;
  return 0;
}

/// 目标/可执行**源列表**中跳过对象文件与明显链接产物：真实 CMake 常将 **add_custom_command** 生成的 `.obj`（如
/// Windows `rc` → `*.obj`）与库列在一起，而 **GroundZero 扁平 add_library 无法**表达该先后关系，且
/// 首次 **cmake** 时该路径尚不存在 → 不可写入 `target.xml` 的 `<sources>`.
static bool skip_as_flat_source_path(const fs::path &p) {
  std::string e = p.extension().string();
  for (char &c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (e == ".obj" || e == ".o" || e == ".iobj") return true;
  return false;
}

// --- L1–L4/L5: include + macro(与 function 同作宏展开) + L3: if 压平 ---

struct MacroOrFnBody {
  std::vector<std::string> param_names;
  std::vector<CmakeCommand> body;
};

static void inject_gz_cmake_path_vars(const fs::path &this_dir, const fs::path &top_source, const fs::path &top_binary,
                                     std::unordered_map<std::string, std::string> *m) {
  std::error_code ec;
  const fs::path ts = fs::weakly_canonical(top_source, ec);
  const fs::path tb = fs::weakly_canonical(top_binary, ec);
  const fs::path td = fs::weakly_canonical(this_dir, ec);
  (*m)["CMAKE_SOURCE_DIR"] = ts.generic_string();
  (*m)["CMAKE_BINARY_DIR"] = tb.generic_string();
  (*m)["CMAKE_CURRENT_SOURCE_DIR"] = td.generic_string();
  (*m)["CMAKE_CURRENT_LIST_DIR"] = td.generic_string();
  fs::path rel = fs::relative(td, ts, ec);
  if (ec) rel = fs::path(".");
  if (rel.empty()) rel = fs::path(".");
  (*m)["CMAKE_CURRENT_BINARY_DIR"] = fs::weakly_canonical(tb / rel, ec).generic_string();
}

static void include_inline_for_listfile(std::vector<CmakeCommand> *cmds, const fs::path &default_listfile, InterpretResult *res,
                                        std::unordered_set<std::string> *include_seen, ListfileProgress *lp) {
  for (int round = 0; round < 32; ++round) {
    if (lp) lp->emit_intra(default_listfile, "include内联", static_cast<std::size_t>(round) + 1, 32);
    std::vector<CmakeCommand> out;
    bool changed = false;
    int blk = 0;
    for (CmakeCommand &c : *cmds) {
      std::string n = c.name;
      to_lower(&n);
      const fs::path list_ref = c.file_path.empty() ? default_listfile : fs::path(c.file_path);
      if (n == "include" && blk == 0 && !c.args.empty()) {
        const fs::path inc = list_ref.parent_path() / c.args[0];
        std::error_code ec;
        if (fs::is_regular_file(inc, ec)) {
          std::error_code ec3;
          const fs::path inc_abs = fs::absolute(inc, ec3);
          const std::string ckey = fs::weakly_canonical(inc_abs, ec3).generic_string();
          if (include_seen->count(ckey)) {
            if (res) res->parse_diagnostics.push_back("include: 跳过环或重复: " + ckey);
            out.push_back(std::move(c));
            blk += block_step(n);
            if (blk < 0) blk = 0;
            continue;
          }
          include_seen->insert(ckey);
          const std::string t = read_file_bin(inc);
          CmakeParseProgress inc_cb;
          const CmakeParseProgress *inc_p = nullptr;
          if (lp) {
            inc_cb = [lp, inc](std::size_t pos, std::size_t tot, std::size_t, std::size_t line, const char *sub) {
              std::string ph = "内联/解析(字节)";
              if (line > 0) {
                ph += " 行";
                ph += std::to_string(line);
              }
              if (sub && sub[0]) {
                ph += " [";
                ph += sub;
                ph += ']';
              }
              lp->emit_intra(inc, ph.c_str(), pos, tot);
            };
            inc_p = &inc_cb;
          }
          CmakeParseResult pr2 = parse_cmake_listfile_text(t, inc.string(), inc_p);
          if (res) {
            res->parse_diagnostics.insert(res->parse_diagnostics.end(), pr2.parse_notes.begin(), pr2.parse_notes.end());
          }
          for (CmakeCommand &c2 : pr2.commands) out.push_back(std::move(c2));
          changed = true;
          continue;
        }
      }
      out.push_back(std::move(c));
      blk += block_step(n);
      if (blk < 0) blk = 0;
    }
    *cmds = std::move(out);
    if (!changed) break;
  }
}

static CmakeCommand substitute_params(const CmakeCommand &c, const std::unordered_map<std::string, std::string> &pmap) {
  CmakeCommand o = c;
  for (auto &a : o.args) {
    for (int r = 0; r < 8; ++r) {
      bool chg = false;
      for (const auto &kv : pmap) {
        const std::string rkey = "${" + kv.first + "}";
        std::size_t pos = 0;
        while ((pos = a.find(rkey, pos)) != std::string::npos) {
          a.replace(pos, rkey.size(), kv.second);
          chg = true;
        }
      }
      if (!chg) break;
    }
  }
  return o;
}

static void remove_macro_function_defs(std::vector<CmakeCommand> *cmds, std::unordered_map<std::string, MacroOrFnBody> *macros) {
  for (std::size_t i = 0; i < cmds->size();) {
    std::string n = (*cmds)[i].name;
    to_lower(&n);
    int b = 0;
    for (std::size_t k = 0; k < i; k++) {
      std::string o = (*cmds)[k].name;
      to_lower(&o);
      b += block_step(o);
    }
    if (b > 0) {
      i++;
      continue;
    }
    if ((n == "macro" || n == "function") && (*cmds)[i].args.size() >= 1) {
      const std::string &mname = (*cmds)[i].args[0];
      std::vector<std::string> pnames;
      for (std::size_t j = 1; j < (*cmds)[i].args.size(); j++) pnames.push_back((*cmds)[i].args[j]);
      int d = 1;
      std::size_t j = i + 1;
      for (; j < cmds->size() && d > 0; j++) {
        std::string n2 = (*cmds)[j].name;
        to_lower(&n2);
        if (n2 == "macro" || n2 == "function") d++;
        else if ((n2 == "endmacro" && n == "macro") || (n2 == "endfunction" && n == "function")) d--;
      }
      if (d != 0) {
        i++;
        continue;
      }
      MacroOrFnBody mb;
      mb.param_names = std::move(pnames);
      for (std::size_t k = i + 1; k + 1 < j; k++) mb.body.push_back((*cmds)[k]);
      (*macros)[mname] = std::move(mb);
      cmds->erase(cmds->begin() + (long)i, cmds->begin() + (long)j);
      continue;
    }
    i++;
  }
}

static void expand_macro_repeatedly(std::vector<CmakeCommand> *cmds, const std::unordered_map<std::string, MacroOrFnBody> &defs,
                                    ListfileProgress *lp, const fs::path &for_listfile) {
  for (int round = 0; round < 32; ++round) {
    if (lp) lp->emit_intra(for_listfile, "宏展开", static_cast<std::size_t>(round) + 1, 32);
    bool chg = false;
    for (std::size_t i = 0; i < cmds->size(); i++) {
      std::string n = (*cmds)[i].name;
      to_lower(&n);
      const auto it = defs.find(n);
      if (it == defs.end()) continue;
      const MacroOrFnBody &def = it->second;
      const std::vector<std::string> cargs = (*cmds)[i].args;
      std::unordered_map<std::string, std::string> pmap;
      for (std::size_t a = 0; a < def.param_names.size(); a++) {
        if (a < cargs.size()) pmap[def.param_names[a]] = cargs[a];
        else
          pmap[def.param_names[a]] = "";
      }
      std::string argn;
      for (std::size_t a = 0; a < cargs.size(); a++) {
        if (a) argn += ';';
        argn += cargs[a];
      }
      pmap["ARGC"] = std::to_string(cargs.size());
      pmap["ARGN"] = argn;
      cmds->erase(cmds->begin() + (long)i);
      for (int bi = (int)def.body.size() - 1; bi >= 0; --bi) {
        CmakeCommand nc = substitute_params(def.body[(std::size_t)bi], pmap);
        cmds->insert(cmds->begin() + (long)i, std::move(nc));
      }
      chg = true;
      break;
    }
    if (!chg) break;
  }
}

struct IfFState {
  bool any = false;
  bool act = false;
};

static void if_stack_if(std::vector<IfFState> *st, bool v) { st->push_back({v != false, v != false}); }
static void if_stack_elseif(std::vector<IfFState> *st, bool v) {
  IfFState &t = st->back();
  if (t.any) t.act = false;
  else {
    t.act = v;
    t.any = v;
  }
}
static void if_stack_else(std::vector<IfFState> *st) {
  IfFState &t = st->back();
  t.act = !t.any;
  t.any = true;
}
static void if_stack_endif(std::vector<IfFState> *st) {
  if (!st->empty()) st->pop_back();
}

static bool if_stack_emitting(const std::vector<IfFState> &st) {
  for (const IfFState &f : st)
    if (!f.act) return false;
  return true;
}

static std::string ev_if_token(const std::string &e0, const std::unordered_map<std::string, std::string> &m) { return expc(e0, m); }

static bool if_eval_one_l(std::string l) {
  to_lower(&l);
  if (l == "0" || l == "false" || l == "off" || l == "no" || l == "n" || l == "ignore" || l.empty()) return false;
  if (l == "1" || l == "on" || l == "true" || l == "y" || l == "yes") return true;
  return !l.empty();
}

static bool eval_if_tokens(const std::vector<std::string> &e, const std::unordered_map<std::string, std::string> &m) {
  if (e.empty()) return false;
  if (e[0] == "NOT" && e.size() >= 2) {
    return !eval_if_tokens(std::vector<std::string>(e.begin() + 1, e.end()), m);
  }
  if (e[0] == "STREQUAL" && e.size() >= 3) {
    return ev_if_token(e[1], m) == ev_if_token(e[2], m);
  }
  if (e[0] == "DEFINED" && e.size() >= 2) {
    const std::string k = ev_if_token(e[1], m);
    return m.find(k) != m.end();
  }
  if (e[0] == "EXISTS" && e.size() >= 2) {
    const fs::path p(expc(e[1], m));
    std::error_code ec;
    return fs::exists(p, ec);
  }
  if (e[0] == "AND" && e.size() >= 2) {
    bool a = true;
    for (std::size_t i = 1; i < e.size() && a; i++) a = a && if_eval_one_l(ev_if_token(e[i], m));
    return a;
  }
  if (e[0] == "OR" && e.size() >= 2) {
    bool a = false;
    for (std::size_t i = 1; i < e.size(); i++) a = a || if_eval_one_l(ev_if_token(e[i], m));
    return a;
  }
  if (e.size() == 1) return if_eval_one_l(ev_if_token(e[0], m));
  for (const auto &a : e)
    if (if_eval_one_l(ev_if_token(a, m))) return true;
  return false;
}

static std::vector<CmakeCommand> filter_if_flat(std::vector<CmakeCommand> cmds, std::unordered_map<std::string, std::string> *m0,
                                                const fs::path &this_dir, const fs::path &top_source, const fs::path &top_binary,
                                                ListfileProgress *lp, const fs::path &for_listfile) {
  inject_gz_cmake_path_vars(this_dir, top_source, top_binary, m0);
  std::vector<IfFState> st;
  std::vector<CmakeCommand> out;
  int fnblk = 0;
  const std::size_t nin = cmds.size();
  const std::size_t stf = (nin > 20000) ? 2000u : (nin > 5000) ? 1000u : 500u;
  for (std::size_t k = 0; k < nin; ++k) {
    if (lp && (k % stf == 0 || k + 1 == nin)) lp->emit_intra(for_listfile, "if压平", k + 1, nin);
    CmakeCommand &c = cmds[k];
    std::string n = c.name;
    to_lower(&n);
    if (n != "if" && n != "else" && n != "elseif" && n != "endif") fnblk += block_step(n);
    if (fnblk < 0) fnblk = 0;
    if (n == "if") {
      if_stack_if(&st, eval_if_tokens(c.args, *m0));
      continue;
    }
    if (n == "elseif") {
      if (!st.empty()) if_stack_elseif(&st, eval_if_tokens(c.args, *m0));
      continue;
    }
    if (n == "else") {
      if (!st.empty()) if_stack_else(&st);
      continue;
    }
    if (n == "endif") {
      if (!st.empty()) if_stack_endif(&st);
      continue;
    }
    if (!if_stack_emitting(st)) continue;
    if (n == "set") {
      if (fnblk == 0) handle_set(c, m0);
      continue;
    }
    if (n == "unset" && c.args.size() >= 1 && fnblk == 0) m0->erase(expc(c.args[0], *m0));
    out.push_back(std::move(c));
  }
  return out;
}

static std::string path_to_cmake_string(const fs::path &p) {
  std::error_code ec;
  const fs::path c = fs::weakly_canonical(p, ec);
  return c.generic_string();
}

// 在单次 process_listfile 中注入 CMake 内置目录变量, 并在离开 Listfile 时从共享 vars 中恢复, 以配合 add_subdirectory 共享同一 map
struct ScopedCmakeDirVars {
  static constexpr const char *kNames[] = {"CMAKE_SOURCE_DIR",   "CMAKE_BINARY_DIR",  "CMAKE_CURRENT_SOURCE_DIR",
                                           "CMAKE_CURRENT_LIST_DIR", "CMAKE_CURRENT_BINARY_DIR"};
  std::unordered_map<std::string, std::string> *const vars;
  std::unordered_map<std::string, std::optional<std::string>> saved;

  ScopedCmakeDirVars(std::unordered_map<std::string, std::string> *v, const fs::path &this_dir, const fs::path &top_source,
                     const fs::path &top_binary) : vars(v) {
    for (const char *name : kNames) {
      const auto it = v->find(name);
      if (it != v->end())
        saved[name] = it->second;
      else
        saved[name] = std::nullopt;
    }
    std::error_code ec;
    const fs::path ts = fs::weakly_canonical(top_source, ec);
    const fs::path tb = fs::weakly_canonical(top_binary, ec);
    const fs::path td = fs::weakly_canonical(this_dir, ec);
    (*v)["CMAKE_SOURCE_DIR"] = path_to_cmake_string(ts);
    (*v)["CMAKE_BINARY_DIR"] = path_to_cmake_string(tb);
    (*v)["CMAKE_CURRENT_SOURCE_DIR"] = path_to_cmake_string(td);
    (*v)["CMAKE_CURRENT_LIST_DIR"] = (*v)["CMAKE_CURRENT_SOURCE_DIR"];
    fs::path rel;
    {
      const fs::path tsn = ts;
      const fs::path tdn = td;
      rel = fs::relative(tdn, tsn, ec);
    }
    if (ec) rel = fs::path(".");
    if (rel.empty()) rel = fs::path(".");
    fs::path cur_b = (tb / rel).lexically_normal();
    cur_b = fs::weakly_canonical(cur_b, ec);
    (*v)["CMAKE_CURRENT_BINARY_DIR"] = path_to_cmake_string(cur_b);
  }

  ~ScopedCmakeDirVars() {
    for (const char *name : kNames) {
      const auto s = saved.find(name);
      if (s == saved.end()) continue;
      if (s->second.has_value())
        (*vars)[name] = *s->second;
      else
        vars->erase(name);
    }
  }
};

static void push_glob_path(const std::string &arg, const fs::path &this_dir, const std::unordered_map<std::string, std::string> &vars,
                           std::vector<fs::path> *out) {
  if (arg.find("$<") != std::string::npos) return;
  std::string e = expc(arg, vars);
  for (const std::string &piece : split_list(e)) {
    if (piece.empty() || piece.find("$<") != std::string::npos) continue;
    fs::path p(piece);
    if (p.is_relative()) p = this_dir / p;
    std::error_code ec;
    out->push_back(fs::weakly_canonical(p, ec));
  }
}

/// Parse `configure_file(…)` into template + output abs paths. False if skipped (genexpr, empty, etc.).
static bool parse_configure_file_command(const CmakeCommand &c, const std::unordered_map<std::string, std::string> &vars,
                                         const fs::path &this_dir, ConfigFilePathPair *out) {
  if (c.args.size() < 2) return false;
  std::string in_str, out_str;
  bool have_in_kw = false, have_out_kw = false;
  for (std::size_t i = 0; i < c.args.size(); ++i) {
    std::string low = c.args[i];
    to_lower(&low);
    if (low == "input" && i + 1 < c.args.size()) {
      in_str = c.args[i + 1];
      have_in_kw = true;
      ++i;
      continue;
    }
    if (low == "output" && i + 1 < c.args.size()) {
      out_str = c.args[i + 1];
      have_out_kw = true;
      ++i;
      continue;
    }
  }
  if (!have_in_kw || !have_out_kw) {
    in_str.clear();
    out_str.clear();
    std::vector<std::string> pos;
    for (std::size_t i = 0; i < c.args.size(); ++i) {
      std::string low = c.args[i];
      to_lower(&low);
      if (low == "input" && i + 1 < c.args.size()) {
        i++;
        continue;
      }
      if (low == "output" && i + 1 < c.args.size()) {
        i++;
        continue;
      }
      if (low == "copyonly" || low == "escape_quotes" || low == "@only") continue;
      if (low == "newline_style" && i + 1 < c.args.size()) {
        i++;
        continue;
      }
      pos.push_back(c.args[i]);
    }
    if (pos.size() < 2) return false;
    in_str = pos[0];
    out_str = pos[1];
  }
  in_str = expc(in_str, vars);
  out_str = expc(out_str, vars);
  if (in_str.find("$<") != std::string::npos || out_str.find("$<") != std::string::npos) return false;
  if (in_str.empty() || out_str.empty()) return false;
  fs::path pin(in_str);
  if (pin.is_relative()) pin = this_dir / pin;
  {
    // 子目录写 `configure_file(模板, …)` 时相对 this_dir, 而模板在**更上层** (如 根 的 `cmake-config.h.in`
    // 被误写成 `examples/…`, `cmake/…`)。`pin` 非文件时沿父级链: 多段相对路径时按**最后一档文件名**向上找 (与
    // 单 `xxx.in` 的整串查找 共用思路)。
    std::error_code ec_reg;
    if (!fs::is_regular_file(pin, ec_reg)) {
      const fs::path fname = pin.has_filename() ? pin.filename() : fs::path(in_str);
      for (fs::path p = this_dir; true; p = p.parent_path()) {
        fs::path alt = in_str.find_first_of("/\\") == std::string::npos ? p / in_str : p / fname;
        if (fs::is_regular_file(alt, ec_reg)) {
          pin = alt;
          break;
        }
        if (p == p.parent_path() || p == p.root_path() || p.empty()) break;
      }
    }
  }
  fs::path pout(out_str);
  if (pout.is_relative()) pout = this_dir / pout;
  std::error_code ec;
  out->in_abs = fs::weakly_canonical(pin, ec);
  out->out_abs = fs::weakly_canonical(pout, ec);
  return true;
}

static void push_filegen_note(const fs::path &listfile, const CmakeCommand &c, const std::string &last_tname,
                              std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res) {
  CmakeFilegenNote note;
  note.ref = listfile.filename().string() + ":" + std::to_string(c.line);
  std::string line;
  for (char ch : c.name) {
    if (ch == '\r' || ch == '\n' || ch == '\t')
      line += ' ';
    else
      line += ch;
  }
  line += ' ';
  for (std::size_t i = 0; i < c.args.size(); ++i) {
    if (i) line += ' ';
    for (char ch : c.args[i]) {
      if (ch == '\r' || ch == '\n' || ch == '\t')
        line += ' ';
      else
        line += ch;
    }
    if (line.size() > 500) {
      line.resize(500);
      line += "…";
      break;
    }
  }
  if (line.size() > 450) {
    line.resize(450);
    line += "…";
  }
  note.one_line = std::move(line);
  if (!last_tname.empty()) {
    auto it = targets->find(last_tname);
    if (it != targets->end()) {
      it->second.filegen_cmake_notes.push_back(std::move(note));
      return;
    }
  }
  res->package_filegen_cmake_notes.push_back(std::move(note));
}

/// 与 `file(WRITE|APPEND)` 及 `add_custom_command(OUTPUT…)` 共用: 将解算后的绝对输出路径登入 `file_write_outputs_*`
static void register_listfile_file_write_output(const std::string &pex, const fs::path &this_dir, const std::string &last_tname,
                                                std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res,
                                                const fs::path &listfile, unsigned line, const char *err_tag) {
  if (pex.find("$<") != std::string::npos) {
    res->errors.push_back(std::string(err_tag) + " skipped generator expression, " + listfile.filename().string() + " line " +
                          std::to_string(line));
    return;
  }
  if (string_has_cmake_deref(pex)) {
    res->errors.push_back(std::string(err_tag) + " unexpanded ${...} in path, " + listfile.filename().string() + " line " +
                          std::to_string(line));
    return;
  }
  fs::path out(pex);
  if (out.is_relative()) out = this_dir / out;
  std::error_code ec;
  out = fs::weakly_canonical(out, ec);
  if (!last_tname.empty()) {
    auto it = targets->find(last_tname);
    if (it != targets->end())
      it->second.file_write_outputs_abs.push_back(std::move(out));
    else
      res->package_file_write_outputs_abs.push_back(std::move(out));
  } else
    res->package_file_write_outputs_abs.push_back(std::move(out));
}

/// 遇下列词则结束**当前** OUTPUT 段、BYPRODUCTS 段 或 整个解析 (不将关键字后的词误登记为路径)
static bool is_add_custom_command_section_start(const std::string &tlow) {
  return tlow == "command" || tlow == "depends" || tlow == "byproducts" || tlow == "implicit_depends" || tlow == "main_dependency" ||
         tlow == "working_directory" || tlow == "comment" || tlow == "verbatim" || tlow == "append" || tlow == "uses_terminal" ||
         tlow == "command_expand_lists" || tlow == "job_pool" || tlow == "depfile";
}

/// `add_custom_command(OUTPUT a [b…] [BYPRODUCTS x [y…]] [COMMAND…])` 中主 OUTPUT 与 **BYPRODUCTS** 下路径, 不演算 COMMAND/DEPENDS. `TARGET` 表形略过.
static void try_register_add_custom_command_outputs(const CmakeCommand &c, const fs::path &this_dir, const std::string &last_tname,
                                                    std::unordered_map<std::string, std::string> *vars,
                                                    std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res,
                                                    const fs::path &listfile) {
  if (c.args.empty()) return;
  {
    std::string t0 = c.args[0];
    to_lower(&t0);
    if (t0 == "target") return;
  }
  std::size_t oi = static_cast<std::size_t>(-1);
  for (std::size_t k = 0; k < c.args.size(); ++k) {
    std::string tk = c.args[k];
    to_lower(&tk);
    if (tk == "output") {
      oi = k;
      break;
    }
  }
  if (oi == static_cast<std::size_t>(-1)) return;
  const char *const tag_out = "add_custom_command(OUTPUT):";
  const char *const tag_byp = "add_custom_command(BYPRODUCTS):";
  for (std::size_t j = oi + 1; j < c.args.size();) {
    const std::string &piece = c.args[j];
    std::string tjlow = piece;
    to_lower(&tjlow);
    if (tjlow == "byproducts") {
      for (++j; j < c.args.size();) {
        const std::string &p2 = c.args[j];
        std::string w = p2;
        to_lower(&w);
        if (is_add_custom_command_section_start(w)) break;
        if (!p2.empty()) {
          const std::string pex = expc(p2, *vars);
          register_listfile_file_write_output(pex, this_dir, last_tname, targets, res, listfile, c.line, tag_byp);
        }
        j++;
      }
      continue;
    }
    if (is_add_custom_command_section_start(tjlow)) break;  // command/depends/… (byproducts 已先处理)
    if (piece.empty()) {
      j++;
      continue;
    }
    {
      const std::string pex = expc(piece, *vars);
      register_listfile_file_write_output(pex, this_dir, last_tname, targets, res, listfile, c.line, tag_out);
    }
    j++;
  }
}

/// `add_custom_command(OUTPUT… … DEPENDS d1 [d2…] …)` 中输入路径, 不读盘; `out_var` 固定为 `"depends"` 以区别于 `file(READ)`.
static void register_listfile_ac_depends_path(const std::string &pex, const fs::path &this_dir, const std::string &last_tname,
                                              std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res,
                                              const fs::path &listfile, unsigned line) {
  const char *err = "add_custom_command(DEPENDS):";
  if (pex.find("$<") != std::string::npos) {
    res->errors.push_back(std::string(err) + " skipped generator expression, " + listfile.filename().string() + " line " +
                          std::to_string(line));
    return;
  }
  if (string_has_cmake_deref(pex)) {
    res->errors.push_back(std::string(err) + " unexpanded ${...} in path, " + listfile.filename().string() + " line " +
                          std::to_string(line));
    return;
  }
  fs::path pth(pex);
  if (pth.is_relative()) pth = this_dir / pth;
  std::error_code ec;
  pth = fs::weakly_canonical(pth, ec);
  FileReadEntry fre{std::move(pth), "depends"};
  if (!last_tname.empty()) {
    auto it = targets->find(last_tname);
    if (it != targets->end())
      it->second.file_read_entries.push_back(std::move(fre));
    else
      res->package_file_read_entries.push_back(std::move(fre));
  } else
    res->package_file_read_entries.push_back(std::move(fre));
}

/// 在**含** `output` 关键字之 `add_custom_command` 中扫描 `depends` 段 (非 `TARGET` 表形).
static void try_register_add_custom_command_depends(const CmakeCommand &c, const fs::path &this_dir, const std::string &last_tname,
                                                    std::unordered_map<std::string, std::string> *vars,
                                                    std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res,
                                                    const fs::path &listfile) {
  if (c.args.size() < 2) return;
  {
    std::string t0 = c.args[0];
    to_lower(&t0);
    if (t0 == "target") return;
  }
  bool has_output_keyword = false;
  for (std::size_t k = 0; k < c.args.size(); ++k) {
    std::string tk = c.args[k];
    to_lower(&tk);
    if (tk == "output") {
      has_output_keyword = true;
      break;
    }
  }
  if (!has_output_keyword) return;
  for (std::size_t i = 0; i < c.args.size(); ++i) {
    std::string ti = c.args[i];
    to_lower(&ti);
    if (ti != "depends") continue;
    for (std::size_t j = i + 1; j < c.args.size(); ++j) {
      const std::string &p2 = c.args[j];
      std::string w = p2;
      to_lower(&w);
      if (is_add_custom_command_section_start(w)) break;
      if (p2.empty()) continue;
      const std::string pex = expc(p2, *vars);
      register_listfile_ac_depends_path(pex, this_dir, last_tname, targets, res, listfile, c.line);
    }
  }
}

static void append_file_read_to_target_model(TargetModel *tm, const std::string &pex, const std::string &out_var, const fs::path &this_dir, const fs::path &listfile, unsigned line, InterpretResult *res) {
  const char *err = "add_custom_target path:";
  if (pex.find("$<") != std::string::npos) {
    res->errors.push_back(std::string(err) + " skipped generator expression, " + listfile.filename().string() + " line " + std::to_string(line));
    return;
  }
  if (string_has_cmake_deref(pex)) {
    res->errors.push_back(std::string(err) + " unexpanded ${...} in path, " + listfile.filename().string() + " line " + std::to_string(line));
    return;
  }
  fs::path pth(pex);
  if (pth.is_relative()) pth = this_dir / pth;
  std::error_code ec;
  pth = fs::weakly_canonical(pth, ec);
  std::string v = out_var;
  if (v.size() > 200u) v = "depends";
  tm->file_read_entries.push_back(FileReadEntry{std::move(pth), std::move(v)});
}

static bool is_add_custom_target_sources_break(const std::string &tlow) {
  return tlow == "command" || tlow == "working_directory" || tlow == "comment" || tlow == "verbatim" || tlow == "uses_terminal" || tlow == "depends" || tlow == "job_pool";
}
static bool is_add_custom_target_depends_break(const std::string &tlow) {
  return tlow == "command" || tlow == "working_directory" || tlow == "comment" || tlow == "verbatim" || tlow == "uses_terminal" || tlow == "sources" || tlow == "job_pool" || tlow == "depends";
}

static void process_listfile(const fs::path &listfile, const std::vector<fs::path> &inherited_includes,
                             std::unordered_map<std::string, std::string> *vars,
                             std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res,
                             const fs::path &top_source, const fs::path &top_binary,
                             std::unordered_set<std::string> *subdir_visited, ListfileProgress *listfile_progress) {
  {
    std::error_code ec0;
    const fs::path abs = fs::absolute(listfile, ec0);
    const std::string listkey = fs::weakly_canonical(abs, ec0).generic_string();
    if (subdir_visited) {
      if (subdir_visited->count(listkey)) {
        if (res) res->parse_diagnostics.push_back("add_subdirectory: 跳过已处理 Listfile(环或重复): " + listkey);
        return;
      }
      subdir_visited->insert(listkey);
    }
  }
  if (listfile_progress) listfile_progress->emit(listfile);

  if (listfile_progress) listfile_progress->emit_intra(listfile, "读入", 0, 0);
  const std::string text = read_file_bin(listfile);
  CmakeParseProgress parse_cb;
  const CmakeParseProgress *parse_p = nullptr;
  if (listfile_progress) {
    parse_cb = [listfile_progress, listfile](std::size_t pos, std::size_t tot, std::size_t, std::size_t line, const char *sub) {
      std::string ph = "词法/解析(字节)";
      if (line > 0) {
        ph += " 行";
        ph += std::to_string(line);
      }
      if (sub && sub[0]) {
        ph += " [";
        ph += sub;
        ph += ']';
      }
      listfile_progress->emit_intra(listfile, ph.c_str(), pos, tot);
    };
    parse_p = &parse_cb;
  }
  CmakeParseResult pr = parse_cmake_listfile_text(text, listfile.string(), parse_p);
  if (res) {
    res->parse_diagnostics.insert(res->parse_diagnostics.end(), pr.parse_notes.begin(), pr.parse_notes.end());
  }
  std::vector<CmakeCommand>   work = std::move(pr.commands);
  std::unordered_set<std::string> include_seen;
  include_inline_for_listfile(&work, listfile, res, &include_seen, listfile_progress);
  std::unordered_map<std::string, MacroOrFnBody> defmap;
  remove_macro_function_defs(&work, &defmap);
  expand_macro_repeatedly(&work, defmap, listfile_progress, listfile);
  const fs::path this_dir = listfile.parent_path();
  work = filter_if_flat(std::move(work), vars, this_dir, top_source, top_binary, listfile_progress, listfile);
  const ScopedCmakeDirVars _cmake_builtins(vars, this_dir, top_source, top_binary);

  std::vector<fs::path> u;
  {
    const std::size_t nwk = work.size();
    const std::size_t stp = (nwk > 5000) ? 512u : (nwk > 500) ? 128u : (nwk > 100) ? 32u : 8u;
    int b0 = 0;
    for (std::size_t k = 0; k < nwk; ++k) {
      if (listfile_progress && (k % stp == 0 || k + 1 == nwk)) listfile_progress->emit_intra(listfile, "预扫", k + 1, nwk);
      const CmakeCommand &c0 = work[k];
      std::string nn = c0.name;
      to_lower(&nn);
      if (nn != "if" && nn != "else" && nn != "elseif" && nn != "endif")
        b0 += block_step(nn);
      if (b0 < 0) b0 = 0;
      if (nn == "include_directories" && b0 == 0) {
        for (const std::string &a0 : c0.args) {
          if (a0 == "AFTER" || a0 == "BEFORE" || a0 == "SYSTEM" || a0 == "INTERFACE" || a0 == "SORT" || a0 == "NO_RECURSE" ||
              a0 == "DIRECTORY")
            continue;
          push_glob_path(a0, this_dir, *vars, &u);
        }
      }
    }
  }

  std::vector<fs::path> base = inherited_includes;
  base.insert(base.end(), u.begin(), u.end());

  int block = 0;
  std::string last_tname;  // 同 Listfile 内最近一次的 add_executable / add_library / add_custom_target 目标名, 供 configure_file 等归属
  const std::size_t nwm = work.size();
  const std::size_t stpm = (nwm > 5000) ? 512u : (nwm > 500) ? 128u : (nwm > 100) ? 32u : 8u;
  for (std::size_t cmd_i = 0; cmd_i < nwm; ++cmd_i) {
    if (listfile_progress && (cmd_i % stpm == 0 || cmd_i + 1 == nwm)) listfile_progress->emit_intra(listfile, "主扫", cmd_i + 1, nwm);
    const CmakeCommand &c = work[cmd_i];
    std::string n = c.name;
    to_lower(&n);
    if (n != "if" && n != "else" && n != "elseif" && n != "endif")
      block += block_step(n);
    if (block < 0) block = 0;

    if (n == "project" && !c.args.empty() && block == 0) {
      if (res->project_name.empty()) {
        std::string p = expc(c.args[0], *vars);
        for (char &ch : p) {
          if (ch == ' ' || ch == '\t' || ch == ')' || ch == '(') ch = '_';
        }
        res->project_name = p;
      }
    }
    if (n == "add_subdirectory" && block == 0 && c.args.size() >= 1) {
      const std::string sub = expc(c.args[0], *vars);
      if (sub.find("$<") == std::string::npos) {
        fs::path sublist = this_dir / sub / "CMakeLists.txt";
        if (fs::is_regular_file(sublist))
          process_listfile(sublist, base, vars, targets, res, top_source, top_binary, subdir_visited, listfile_progress);
      }
    }
    if (n == "add_executable" && block == 0 && c.args.size() >= 1) {
      const std::string tname = expc(c.args[0], *vars);
      if (tname.find("::") != std::string::npos) continue;
      std::size_t i = 1;
      while (i < c.args.size()) {
        std::string t = c.args[i];
        to_lower(&t);
        if (t == "win32" || t == "macosx_bundle" || t == "exclude_from_all" || t == "global") {
          i++;
          continue;
        }
        break;
      }
      if (i < c.args.size()) {
        std::string t = c.args[i];
        to_lower(&t);
        if (t == "imported" || t == "alias") continue;
      }
      std::vector<fs::path> srcs;
      for (; i < c.args.size(); ++i) {
        for (const std::string &piece : split_list(expc(c.args[i], *vars))) {
          if (piece.find("$<") != std::string::npos) continue;
          if (piece.empty()) continue;
          fs::path p(piece);
          if (p.is_relative()) p = this_dir / p;
          std::error_code ec;
          srcs.push_back(fs::weakly_canonical(p, ec));
        }
      }
      TargetModel tm;
      tm.name = tname;
      tm.kind = "executable";
      tm.source_paths_abs = std::move(srcs);
      tm.include_dir_abs = base;
      (*targets)[tname] = std::move(tm);
      last_tname = tname;
    }

    if (n == "add_library" && block == 0 && c.args.size() >= 1) {
      const std::string tname = expc(c.args[0], *vars);
      if (tname.find("::") != std::string::npos) continue;
      std::string kind = "static_library";
      std::size_t i = 1;
      if (c.args.size() > 1) {
        std::string t0 = c.args[1];
        to_lower(&t0);
        if (t0 == "static" || t0 == "shared" || t0 == "module" || t0 == "object" || t0 == "interface" || t0 == "imported" ||
            t0 == "alias" || t0 == "unknown") {
          if (t0 == "object" || t0 == "interface" || t0 == "imported" || t0 == "alias" || t0 == "unknown") continue;
          if (t0 == "shared" || t0 == "module") kind = "shared_library";
          if (t0 == "static") kind = "static_library";
          i = 2;
        }
      }
      for (; i < c.args.size();) {
        std::string t = c.args[i];
        to_lower(&t);
        if (t == "exclude_from_all" || t == "global" || t == "resolve_device_symbols") {
          i++;
          continue;
        }
        break;
      }
      std::vector<fs::path> srcs;
      for (; i < c.args.size(); ++i) {
        for (const std::string &piece : split_list(expc(c.args[i], *vars))) {
          if (piece.find("$<") != std::string::npos) continue;
          if (piece.empty()) continue;
          fs::path p(piece);
          if (p.is_relative()) p = this_dir / p;
          std::error_code ec;
          srcs.push_back(fs::weakly_canonical(p, ec));
        }
      }
      TargetModel tm;
      tm.name = tname;
      tm.kind = kind;
      tm.source_paths_abs = std::move(srcs);
      tm.include_dir_abs = base;
      (*targets)[tname] = std::move(tm);
      last_tname = tname;
    }

    if (n == "add_custom_target" && block == 0 && c.args.size() >= 1) {
      const std::string tname = expc(c.args[0], *vars);
      if (tname.find("::") != std::string::npos) continue;
      TargetModel tm;
      tm.name = tname;
      tm.kind = "custom_target";
      tm.include_dir_abs = base;
      for (std::size_t i = 0; i < c.args.size(); ++i) {
        std::string ti = c.args[i];
        to_lower(&ti);
        if (ti != "sources") continue;
        for (std::size_t j = i + 1; j < c.args.size(); ++j) {
          const std::string &p2 = c.args[j];
          std::string w = p2;
          to_lower(&w);
          if (is_add_custom_target_sources_break(w)) break;
          if (p2.find("$<") != std::string::npos) continue;
          for (const std::string &piece : split_list(expc(p2, *vars))) {
            if (piece.find("$<") != std::string::npos) continue;
            if (piece.empty()) continue;
            fs::path p(piece);
            if (p.is_relative()) p = this_dir / p;
            std::error_code ec;
            tm.source_paths_abs.push_back(fs::weakly_canonical(p, ec));
          }
        }
      }
      for (std::size_t i = 0; i < c.args.size(); ++i) {
        std::string ti = c.args[i];
        to_lower(&ti);
        if (ti != "depends") continue;
        for (std::size_t j = i + 1; j < c.args.size(); ++j) {
          const std::string &p2 = c.args[j];
          std::string w = p2;
          to_lower(&w);
          if (is_add_custom_target_depends_break(w)) break;
          if (p2.find("$<") != std::string::npos) continue;
          for (const std::string &seg : split_list(expc(p2, *vars))) {
            if (seg.find("$<") != std::string::npos) continue;
            if (seg.empty() || seg.find("::") != std::string::npos) continue;
            if (seg.find('/') != std::string::npos)
              append_file_read_to_target_model(&tm, seg, "depends", this_dir, listfile, c.line, res);
            else
              tm.link_to.insert(seg);
          }
        }
      }
      (*targets)[tname] = std::move(tm);
      last_tname = tname;
    }

    if (n == "configure_file" && block == 0 && c.args.size() >= 2) {
      ConfigFilePathPair cfp;
      if (parse_configure_file_command(c, *vars, this_dir, &cfp)) {
        if (!last_tname.empty()) {
          auto it = targets->find(last_tname);
          if (it != targets->end()) it->second.config_files.push_back(std::move(cfp));
          else
            res->package_config_files.push_back(std::move(cfp));
        } else
          res->package_config_files.push_back(std::move(cfp));
      } else {
        std::string raw;
        for (const auto &a : c.args) raw += a;
        if (raw.find("$<") != std::string::npos) {
          res->errors.push_back("configure_file: skipped generator expression, " + listfile.filename().string() + " line " +
                                std::to_string(c.line));
        }
      }
    }

    if (n == "file" && block == 0 && c.args.size() >= 2) {
      std::string sub = c.args[0];
      to_lower(&sub);
      if (sub == "write" && c.args.size() >= 3)
        register_listfile_file_write_output(expc(c.args[1], *vars), this_dir, last_tname, targets, res, listfile, c.line, "file(WRITE:");
      if (sub == "append" && c.args.size() >= 2)
        register_listfile_file_write_output(expc(c.args[1], *vars), this_dir, last_tname, targets, res, listfile, c.line, "file(APPEND:");
      if (sub == "read" && c.args.size() >= 3) {
        // file(READ <filename> <out-var> [OFFSET o] [LIMIT n] [HEX])
        const std::string pex = expc(c.args[1], *vars);
        if (pex.find("$<") != std::string::npos) {
          res->errors.push_back("file(READ: skipped generator expression, " + listfile.filename().string() + " line " + std::to_string(c.line));
        } else if (string_has_cmake_deref(pex)) {
          res->errors.push_back("file(READ: unexpanded ${...} in path, " + listfile.filename().string() + " line " + std::to_string(c.line));
        } else {
          fs::path pth(pex);
          if (pth.is_relative()) pth = this_dir / pth;
          std::error_code ec;
          pth = fs::weakly_canonical(pth, ec);
          std::string vname = expc(c.args[2], *vars);
          if (vname.size() > 200u) vname = "out";
          FileReadEntry fre{std::move(pth), std::move(vname)};
          if (!last_tname.empty()) {
            auto it = targets->find(last_tname);
            if (it != targets->end())
              it->second.file_read_entries.push_back(std::move(fre));
            else
              res->package_file_read_entries.push_back(std::move(fre));
          } else
            res->package_file_read_entries.push_back(std::move(fre));
        }
      }
    }

    if (n == "add_custom_command" && block == 0) {
      try_register_add_custom_command_outputs(c, this_dir, last_tname, vars, targets, res, listfile);
      try_register_add_custom_command_depends(c, this_dir, last_tname, vars, targets, res, listfile);
    }

    if (n == "string" && block == 0 && c.args.size() >= 2) {
      std::string ssub = c.args[0];
      to_lower(&ssub);
      if (ssub == "regex" || ssub == "append" || ssub == "replace" || ssub == "concat")
        push_filegen_note(listfile, c, last_tname, targets, res);
    }
    // 当前命令已先使 block 递增: foreach/while 恰使 depth==1(顶层) 时登记; 嵌套时 depth>=2, 不登记
    if (n == "foreach" && block == 1 && !c.args.empty()) push_filegen_note(listfile, c, last_tname, targets, res);
    if (n == "while" && block == 1 && !c.args.empty()) push_filegen_note(listfile, c, last_tname, targets, res);

    if (n == "target_sources" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      for (std::size_t j = 1; j < c.args.size(); ++j) {
        std::string t = c.args[j];
        to_lower(&t);
        if (t == "private" || t == "public" || t == "interface" || t == "before" || t == "after" || t == "system" || t == "key" ||
            t == "file_set" || t == "target_dependent_sources")
          continue;
        if (c.args[j].find("$<") != std::string::npos) continue;
        for (const std::string &piece : split_list(expc(c.args[j], *vars))) {
          if (piece.find("$<") != std::string::npos) continue;
          if (piece.empty()) continue;
          fs::path p(piece);
          if (p.is_relative()) p = this_dir / p;
          std::error_code ec;
          it2->second.source_paths_abs.push_back(fs::weakly_canonical(p, ec));
        }
      }
    }

    if (n == "target_link_libraries" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      for (std::size_t j = 1; j < c.args.size(); ++j) {
        std::string t = c.args[j];
        to_lower(&t);
        if (t == "private" || t == "public" || t == "interface" || t == "before" || t == "after" || t == "system" || t == "key" ||
            t == "link_interface_libraries" || t == "debug" || t == "optimized" || t == "general")
          continue;
        if (c.args[j].find("$<") != std::string::npos) continue;
        for (const std::string &seg : split_list(expc(c.args[j], *vars))) {
          if (seg.empty() || seg.find("::") != std::string::npos) continue;
          if (seg.find("/") != std::string::npos) continue;  // 可能文件路径
          it2->second.link_to.insert(seg);
        }
      }
    }
    if (n == "add_dependencies" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      if (tname.find("::") != std::string::npos) continue;
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      for (std::size_t j = 1; j < c.args.size(); ++j) {
        if (c.args[j].find("$<") != std::string::npos) continue;
        for (const std::string &seg : split_list(expc(c.args[j], *vars))) {
          if (seg.empty() || seg.find("::") != std::string::npos) continue;
          if (seg.find("/") != std::string::npos) continue;
          it2->second.link_to.insert(seg);
        }
      }
    }
    if (n == "target_include_directories" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      for (std::size_t j = 1; j < c.args.size(); ++j) {
        std::string t = c.args[j];
        to_lower(&t);
        if (t == "private" || t == "public" || t == "interface" || t == "before" || t == "after" || t == "system" || t == "key")
          continue;
        if (c.args[j].find("$<") != std::string::npos) continue;
        for (const std::string &piece : split_list(expc(c.args[j], *vars))) {
          if (piece.find("$<") != std::string::npos) continue;
          if (piece.empty()) continue;
          fs::path p(piece);
          if (p.is_relative()) p = this_dir / p;
          std::error_code ec;
          it2->second.include_dir_abs.push_back(fs::weakly_canonical(p, ec));
        }
      }
    }
    if (n == "target_compile_options" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      collect_target_options_list(c, *vars, &it2->second.compile_flags);
    }
    if (n == "target_link_options" && block == 0 && c.args.size() >= 2) {
      const std::string tname = expc(c.args[0], *vars);
      auto it2 = targets->find(tname);
      if (it2 == targets->end()) continue;
      collect_target_options_list(c, *vars, &it2->second.link_flags);
    }
    if (n == "set_target_properties" && block == 0 && c.args.size() >= 3) {
      // CMake: set_target_properties(t1 [t2 ...] PROPERTIES k v k v ...) — 须在实参中定位 PROPERTIES。
      std::size_t prop_i = c.args.size();
      for (std::size_t p = 0; p < c.args.size(); ++p) {
        std::string a = c.args[p];
        to_lower(&a);
        if (a == "properties") {
          prop_i = p;
          break;
        }
      }
      if (prop_i > 0 && prop_i < c.args.size() && prop_i + 1 < c.args.size()) {
        for (std::size_t i = 0; i < prop_i; ++i) {
          const std::string tname = expc(c.args[i], *vars);
          if (tname.find("::") != std::string::npos) continue;  // 与 add_* 一致, 略过 ALIAS/IMPORTED:: 等
          auto it2 = targets->find(tname);
          if (it2 == targets->end()) continue;
          for (std::size_t j = prop_i + 1; j + 1 < c.args.size(); j += 2) {
            std::string k = c.args[j];
            to_lower(&k);
            const std::string v = expc(c.args[j + 1], *vars);
            if (v.find("$<") != std::string::npos) continue;
            if (k == "compile_flags") {
              for (const std::string &seg : split_list(v)) {
                if (seg.find("$<") == std::string::npos && !seg.empty()) it2->second.compile_flags.push_back(seg);
              }
            } else if (k == "link_flags") {
              for (const std::string &seg : split_list(v)) {
                if (seg.find("$<") == std::string::npos && !seg.empty()) it2->second.link_flags.push_back(seg);
              }
            }
          }
        }
      }
    }
  }
}

fs::path infer_gz_default_cmake_binary_root(const fs::path &top_cmake_parent_path) {
  std::error_code ec;
  const fs::path top_source = fs::weakly_canonical(top_cmake_parent_path, ec);
  const fs::path broot = top_source / ".intermediate" / "build";
  const fs::path as_default = broot / "default";
  if (fs::is_directory(as_default, ec)) {
    const fs::path c = fs::weakly_canonical(as_default, ec);
    if (!ec && !c.empty()) return c;
    return fs::absolute(as_default);
  }
  std::vector<fs::path> subs;
  ec.clear();
  if (fs::is_directory(broot, ec)) {
    for (const auto &de : fs::directory_iterator(broot)) {
      if (de.is_directory()) subs.push_back(de.path());
    }
  }
  const fs::path chosen = (subs.size() == 1) ? subs[0] : as_default;
  ec.clear();
  const fs::path c2 = fs::weakly_canonical(chosen, ec);
  if (!ec && !c2.empty()) return c2;
  return fs::absolute(chosen);
}

static std::string read_arch_value_from_gz_cache(const fs::path &cache_path) {
  std::ifstream f(cache_path, std::ios::binary);
  if (!f) return {};
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() >= 5u && line.compare(0, 5, "arch=") == 0) return line.substr(5u);
  }
  return {};
}

std::string infer_gz_generated_arch_segment(const fs::path &top_cmake_parent_path) {
  const fs::path bro = infer_gz_default_cmake_binary_root(top_cmake_parent_path);
  if (bro.empty()) return "default";
  const std::string from_cache = read_arch_value_from_gz_cache(bro / "gz_cache.txt");
  if (!from_cache.empty()) return from_cache;
  std::string leaf = bro.filename().string();
  for (char &c : leaf) {
    if (c == '/' || c == '\\') c = '_';
  }
  if (leaf.empty()) return "default";
  return leaf;
}

InterpretResult interpret_cmake_tree(const fs::path &source_root, const fs::path &top_cmake, const fs::path *file_api_json_path,
                                    ListfileProgress *listfile_progress) {
  (void)source_root;
  std::error_code ec;
  const fs::path top_source = fs::weakly_canonical(top_cmake.parent_path(), ec);
  const fs::path top_binary = infer_gz_default_cmake_binary_root(top_cmake.parent_path());
  InterpretResult r;
  std::unordered_map<std::string, std::string> vars;
  std::unordered_map<std::string, TargetModel> tmap;
  std::unordered_set<std::string> subdir_visited;
  process_listfile(top_cmake, {}, &vars, &tmap, &r, top_source, top_binary, &subdir_visited, listfile_progress);
  r.targets = std::move(tmap);
  if (r.project_name.empty()) r.project_name = "reversed_project";
  if (file_api_json_path && !file_api_json_path->empty()) {
    std::error_code e2;
    if (fs::is_regular_file(*file_api_json_path, e2)) {
      const std::string jt = read_file_bin(*file_api_json_path);
      r.file_api_target_names = ingest_target_names_from_codemodel_json(jt);
      for (const std::string &n : r.file_api_target_names) {
        if (r.targets.find(n) == r.targets.end())
          r.file_api_merge_notes.push_back("L7 file_api: target \"" + n + "\" 未在静态反解中");
      }
    } else
      r.errors.push_back("L7: 无法读取 file_api JSON: " + file_api_json_path->string());
  }
  return r;
}
