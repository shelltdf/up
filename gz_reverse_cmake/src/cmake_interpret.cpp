#include "cmake_interpret.hpp"

#include <cctype>
#include <fstream>

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
  return t;
}

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
    jn += vals[v];
  }
  (*vars)[n] = jn;
}

static int block_step(const std::string &n) {
  if (n == "function" || n == "macro" || n == "foreach" || n == "while") return 1;
  if (n == "endfunction" || n == "endmacro" || n == "endforeach" || n == "endwhile") return -1;
  return 0;
}

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

static void process_listfile(
    const fs::path &listfile, const std::vector<fs::path> &inherited_includes, std::unordered_map<std::string, std::string> *vars,
    std::unordered_map<std::string, TargetModel> *targets, InterpretResult *res) {

  const std::string text = read_file_bin(listfile);
  const std::vector<CmakeCommand> cmds = parse_cmake_script(text);
  const fs::path this_dir = listfile.parent_path();

  std::vector<fs::path> u;
  {
    int b0 = 0;
    for (const auto &c0 : cmds) {
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
  for (const auto &c : cmds) {
    std::string n = c.name;
    to_lower(&n);
    if (n != "if" && n != "else" && n != "elseif" && n != "endif")
      block += block_step(n);
    if (block < 0) block = 0;

    if (n == "set") handle_set(c, vars);
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
        if (fs::is_regular_file(sublist)) process_listfile(sublist, base, vars, targets, res);
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
    }

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
  }
}

InterpretResult interpret_cmake_tree(const fs::path &source_root, const fs::path &top_cmake) {
  (void)source_root;
  InterpretResult r;
  std::unordered_map<std::string, std::string> vars;
  std::unordered_map<std::string, TargetModel> tmap;
  process_listfile(top_cmake, {}, &vars, &tmap, &r);
  r.targets = std::move(tmap);
  if (r.project_name.empty()) r.project_name = "reversed_project";
  return r;
}
