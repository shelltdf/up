#include "ninja/ninja_backend.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace up {

namespace {

std::string sanitize_name(std::string s) {
  for (char& c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok)
      c = '_';
  }
  return s;
}

std::string quote_ninja_path(const std::filesystem::path& p) {
  std::string s = p.generic_string();
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == ' ')
      out += "$ ";
    else if (c == ':')
      out += "$:";
    else
      out.push_back(c);
  }
  return out;
}

// Ninja expands `$` in variables; double for literal `$` in /DNAME=value.
std::string ninja_escape_compile_def_token(std::string s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    if (c == '$')
      out += "$$";
    else
      out.push_back(c);
  }
  return out;
}

std::string ninja_compile_def_flags(const std::vector<std::string>& defs) {
  std::string s;
  for (const auto& d : defs) {
#if defined(_WIN32)
    s += "/D";
#else
    s += "-D";
#endif
    s += ninja_escape_compile_def_token(d);
    s += ' ';
  }
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  return s;
}

}  // namespace

std::string build_ninja_install_command(const BuildBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "ninja -C \"" << to_posix_path_string(ctx.bin_dir) << "\" -j " << parallel_jobs_for_build(ctx.opts) << " -v install";
  return cmd.str();
}

int write_ninja_file(const ConfigureGraphModel& model) {
  if (!model.external_cmake.empty()) {
    std::cerr << "configure: ninja backend does not support package.xml <cmake> (external CMake projects).\n";
    return 9;
  }
  for (const auto& t : model.targets) {
    if (t.imported_prebuilt) {
      std::cerr << "configure: ninja backend does not support imported_* prebuilt targets.\n";
      return 9;
    }
  }
  std::filesystem::create_directories(model.out_dir);
  const auto ninja_path = model.out_dir / "build.ninja";
  std::ofstream nf(ninja_path);
  if (!nf)
    return 5;

  const bool debug = (model.config_mode == "debug");
  nf << "ninja_required_version = 1.7\n";
#if defined(_WIN32)
  nf << "cxx = cl\n";
  nf << "cflags = /nologo /EHsc /std:c++17 " << (debug ? "/Zi /Od" : "/O2") << "\n";
  nf << "ldflags = /nologo\n\n";
  nf << "rule cxx\n";
  nf << "  command = $cxx /c $cflags $defs /Fo$out $in\n";
  nf << "  description = CXX $out\n\n";
  nf << "rule link_exe\n";
  nf << "  command = link $ldflags /OUT:$out $in $libs\n";
  nf << "  description = LINK $out\n\n";
  nf << "rule link_shared\n";
  nf << "  command = link $ldflags /DLL /OUT:$out $in $libs\n";
  nf << "  description = SHARED $out\n\n";
  nf << "rule link_static\n";
  nf << "  command = lib /nologo /OUT:$out $in\n";
  nf << "  description = STATIC $out\n\n";
  nf << "rule copy\n";
  nf << "  command = cmd /c if not exist \"$outdir\" mkdir \"$outdir\" && copy /Y \"$in\" \"$out\" >nul\n";
  nf << "  description = COPY $out\n\n";
  nf << "rule run_cmd\n";
  nf << "  command = cmd /c $cmd && if not exist \"$outdir\" mkdir \"$outdir\" && type nul > \"$out\"\n";
  nf << "  description = RUN $cmd\n\n";
#else
  nf << "cxx = c++\n";
  nf << "cflags = -std=c++17 " << (debug ? "-g -O0" : "-O2") << "\n\n";
  nf << "rule cxx\n";
  nf << "  command = $cxx -MMD -MF $out.d -c $cflags $defs -o $out $in\n";
  nf << "  depfile = $out.d\n";
  nf << "  description = CXX $out\n\n";
  nf << "rule link_exe\n";
  nf << "  command = $cxx -o $out $in $libs\n";
  nf << "  description = LINK $out\n\n";
  nf << "rule link_shared\n";
  nf << "  command = $cxx -shared -o $out $in $libs\n";
  nf << "  description = SHARED $out\n\n";
  nf << "rule link_static\n";
  nf << "  command = ar rcs $out $in\n";
  nf << "  description = STATIC $out\n\n";
  nf << "rule copy\n";
  nf << "  command = mkdir -p \"$outdir\" && cp -f $in $out\n";
  nf << "  description = COPY $out\n\n";
  nf << "rule run_cmd\n";
  nf << "  command = sh -lc \"$cmd\" && mkdir -p \"$outdir\" && touch \"$out\"\n";
  nf << "  description = RUN $cmd\n\n";
#endif

  std::vector<std::pair<std::string, std::string>> lib_outputs;
  std::vector<std::pair<std::string, std::string>> exe_outputs;
  std::vector<std::string> all_outputs;
  int cmd_idx = 0;

  for (const auto& t : model.targets) {
    if (t.type == "asset_bundle")
      continue;
    std::vector<std::string> objs;
    for (size_t i = 0; i < t.source_paths.size(); ++i) {
      const auto src = std::filesystem::path(t.source_paths[i]);
      const std::string obj_name = sanitize_name(t.name) + "_" + std::to_string(i)
#if defined(_WIN32)
                                   + ".obj";
#else
                                   + ".o";
#endif
      const auto obj = model.out_dir / "obj" / obj_name;
      std::filesystem::create_directories(obj.parent_path());
      const auto src_n = quote_ninja_path(src);
      const auto obj_n = quote_ninja_path(obj);
      std::string implicit_dep;
      if (i < t.source_rules.size() && !t.source_rules[i].preprocess_command.empty()) {
        const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("pre_" + std::to_string(cmd_idx++) + ".stamp"));
        nf << "build " << stamp << ": run_cmd " << src_n << "\n";
        nf << "  cmd = " << t.source_rules[i].preprocess_command << "\n";
        nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
        implicit_dep = stamp;
        all_outputs.push_back(stamp);
      }
      nf << "build " << obj_n << ": cxx " << src_n;
      if (!implicit_dep.empty())
        nf << " | " << implicit_dep;
      nf << "\n";
      if (!t.compile_definitions.empty())
        nf << "  defs =" << ninja_compile_def_flags(t.compile_definitions) << "\n";
      objs.push_back(obj_n);
      all_outputs.push_back(obj_n);
    }

    std::ostringstream in_list;
    for (size_t i = 0; i < objs.size(); ++i) {
      if (i)
        in_list << " ";
      in_list << objs[i];
    }

    std::string out_file;
    std::string rule;
    if (t.type == "static_library") {
#if defined(_WIN32)
      out_file = quote_ninja_path(model.out_dir / (t.name + ".lib"));
#else
      out_file = quote_ninja_path(model.out_dir / ("lib" + t.name + ".a"));
#endif
      rule = "link_static";
      lib_outputs.push_back({t.name, out_file});
    } else if (t.type == "shared_library") {
#if defined(_WIN32)
      out_file = quote_ninja_path(model.out_dir / (t.name + ".dll"));
#else
      out_file = quote_ninja_path(model.out_dir / ("lib" + t.name + ".so"));
#endif
      rule = "link_shared";
      lib_outputs.push_back({t.name, out_file});
    } else {
#if defined(_WIN32)
      out_file = quote_ninja_path(model.out_dir / (t.name + ".exe"));
#else
      out_file = quote_ninja_path(model.out_dir / t.name);
#endif
      rule = "link_exe";
      exe_outputs.push_back({t.name, out_file});
    }

    nf << "build " << out_file << ": " << rule;
    if (!objs.empty())
      nf << " " << in_list.str();
    nf << "\n";
    if (t.type == "executable" || t.type == "shared_library") {
      if (!lib_outputs.empty()) {
        nf << "  libs =";
        for (const auto& kv : lib_outputs)
          nf << " " << kv.second;
        nf << "\n";
      }
    }
    all_outputs.push_back(out_file);
    for (const auto& s : t.source_rules) {
      if (!s.postprocess_command.empty()) {
        const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("post_" + std::to_string(cmd_idx++) + ".stamp"));
        nf << "build " << stamp << ": run_cmd " << out_file << "\n";
        nf << "  cmd = " << s.postprocess_command << "\n";
        nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
        all_outputs.push_back(stamp);
      }
    }
  }

  const auto install_bin = model.install_root / "bin";
  std::vector<std::string> install_outputs;
  for (const auto& ex : exe_outputs) {
    std::string out_name = ex.first;
#if defined(_WIN32)
    out_name += ".exe";
#endif
    const auto dst = quote_ninja_path(install_bin / out_name);
    nf << "build " << dst << ": copy " << ex.second << "\n";
    nf << "  outdir = " << quote_ninja_path((model.install_root / "bin")) << "\n";
    install_outputs.push_back(dst);
  }
  for (const auto& rule : model.install_file_rules) {
    const auto src = quote_ninja_path(std::filesystem::path(rule.src));
    const auto dst = quote_ninja_path(model.install_root / rule.dst / std::filesystem::path(rule.src).filename());
    if (!rule.preprocess_command.empty()) {
      const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("inc_pre_" + std::to_string(cmd_idx++) + ".stamp"));
      nf << "build " << stamp << ": run_cmd " << src << "\n";
      nf << "  cmd = " << rule.preprocess_command << "\n";
      nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
      all_outputs.push_back(stamp);
    }
    nf << "build " << dst << ": copy " << src << "\n";
    nf << "  outdir = " << quote_ninja_path((model.install_root / rule.dst)) << "\n";
    install_outputs.push_back(dst);
    if (!rule.postprocess_command.empty()) {
      const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("inc_post_" + std::to_string(cmd_idx++) + ".stamp"));
      nf << "build " << stamp << ": run_cmd " << dst << "\n";
      nf << "  cmd = " << rule.postprocess_command << "\n";
      nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
      all_outputs.push_back(stamp);
    }
  }
  for (const auto& rule : model.asset_file_rules) {
    const auto src = quote_ninja_path(std::filesystem::path(rule.src));
    const auto dst = quote_ninja_path(model.install_root / rule.dst / std::filesystem::path(rule.src).filename());
    if (!rule.preprocess_command.empty()) {
      const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("asset_pre_" + std::to_string(cmd_idx++) + ".stamp"));
      nf << "build " << stamp << ": run_cmd " << src << "\n";
      nf << "  cmd = " << rule.preprocess_command << "\n";
      nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
      all_outputs.push_back(stamp);
    }
    nf << "build " << dst << ": copy " << src << "\n";
    nf << "  outdir = " << quote_ninja_path((model.install_root / rule.dst)) << "\n";
    install_outputs.push_back(dst);
    if (!rule.postprocess_command.empty()) {
      const auto stamp = quote_ninja_path(model.out_dir / "cmd" / ("asset_post_" + std::to_string(cmd_idx++) + ".stamp"));
      nf << "build " << stamp << ": run_cmd " << dst << "\n";
      nf << "  cmd = " << rule.postprocess_command << "\n";
      nf << "  outdir = " << quote_ninja_path(model.out_dir / "cmd") << "\n";
      all_outputs.push_back(stamp);
    }
  }

  nf << "\nbuild all: phony";
  for (const auto& x : all_outputs)
    nf << " " << x;
  nf << "\n";

  nf << "build install: phony";
  for (const auto& x : install_outputs)
    nf << " " << x;
  nf << "\n";
  nf << "default install\n";

  std::cout << "Wrote " << to_posix_path_string(ninja_path) << std::endl;
  return 0;
}

}  // namespace up
