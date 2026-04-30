#include "gz_embedded_lua.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace gz {
namespace {

thread_local GzLuaFileRoots* g_roots_tl = nullptr;

static bool is_subpath_of(const std::filesystem::path& child, const std::filesystem::path& parent) {
  if (parent.empty())
    return false;
  std::error_code oec;
  const auto c = std::filesystem::weakly_canonical(child, oec);
  if (oec)
    return false;
  const auto p = std::filesystem::weakly_canonical(parent, oec);
  if (oec)
    return false;
  const auto r = std::filesystem::relative(c, p, oec);
  if (oec)
    return false;
  for (const auto& comp : r) {
    if (comp == "..")
      return false;
  }
  return true;
}

static bool is_allowed_path(const std::filesystem::path& p, std::string& err) {
  if (!g_roots_tl) {
    err = "internal: no sandbox roots";
    return false;
  }
  std::error_code ec;
  const std::vector<const std::filesystem::path*> cand = {&g_roots_tl->workspace, &g_roots_tl->build_root,
                                                            &g_roots_tl->package_dir, &g_roots_tl->generated_dir};
  for (const auto* pr : cand) {
    if (!pr || pr->empty())
      continue;
    if (is_subpath_of(p, *pr)) {
      return true;
    }
  }
  std::error_code e2;
  const auto show = std::filesystem::weakly_canonical(p, e2);
  err = "path outside GZ file sandbox: " + (e2 ? p.string() : show.string());
  return false;
}

static int l_file_read(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  std::string e;
  std::filesystem::path p(s);
  if (p.is_relative() && g_roots_tl && !g_roots_tl->workspace.empty())
    p = g_roots_tl->workspace / p;
  p = p.lexically_normal();
  if (!is_allowed_path(p, e)) {
    return luaL_error(L, "%s", e.c_str());
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    return luaL_error(L, "gz.file.read: cannot open %s", s);
  }
  std::ostringstream o;
  o << f.rdbuf();
  const std::string data = o.str();
  lua_pushlstring(L, data.c_str(), static_cast<size_t>(data.size()));
  return 1;
}

static int l_file_write(lua_State* L) {
  const char* pathc = luaL_checkstring(L, 1);
  size_t dlen = 0;
  const char* data = luaL_checklstring(L, 2, &dlen);
  std::string e;
  std::filesystem::path p(pathc);
  if (p.is_relative() && g_roots_tl && !g_roots_tl->workspace.empty())
    p = g_roots_tl->workspace / p;
  p = p.lexically_normal();
  if (!is_allowed_path(p, e)) {
    return luaL_error(L, "%s", e.c_str());
  }
  std::error_code mk;
  if (p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path(), mk);
    if (mk) {
      return luaL_error(L, "gz.file.write: mkdir: %s", mk.message().c_str());
    }
  }
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) {
    return luaL_error(L, "gz.file.write: open failed: %s", pathc);
  }
  f.write(data, static_cast<std::streamsize>(dlen));
  if (!f) {
    return luaL_error(L, "gz.file.write: write failed: %s", pathc);
  }
  return 0;
}

static int l_file_append(lua_State* L) {
  const char* pathc = luaL_checkstring(L, 1);
  size_t dlen = 0;
  const char* data = luaL_checklstring(L, 2, &dlen);
  std::string e;
  std::filesystem::path p(pathc);
  if (p.is_relative() && g_roots_tl && !g_roots_tl->workspace.empty())
    p = g_roots_tl->workspace / p;
  p = p.lexically_normal();
  if (!is_allowed_path(p, e)) {
    return luaL_error(L, "%s", e.c_str());
  }
  std::ofstream f(p, std::ios::binary | std::ios::app);
  if (!f) {
    return luaL_error(L, "gz.file.append: open failed: %s", pathc);
  }
  f.write(data, static_cast<std::streamsize>(dlen));
  if (!f) {
    return luaL_error(L, "gz.file.append: write failed: %s", pathc);
  }
  return 0;
}

static const luaL_Reg k_file_funcs[] = {{"read", l_file_read}, {"write", l_file_write}, {"append", l_file_append},
                                        {NULL, NULL}};

static void register_gz_module(lua_State* L) {
  lua_newtable(L);
  luaL_newlib(L, k_file_funcs);
  lua_setfield(L, -2, "file");
  lua_setglobal(L, "gz");
}

}  // namespace

std::string run_gz_embedded_configure_lua(const std::string& lua_source, const GzLuaFileRoots& roots,
                                          const std::map<std::string, std::string>& extra_env_strings) {
  GzLuaFileRoots r = roots;
  g_roots_tl = &r;
  struct Reset {
    ~Reset() { g_roots_tl = nullptr; }
  } _reset{};

  std::string err;
  lua_State* L = luaL_newstate();
  if (!L) {
    return "run_gz_embedded_configure_lua: luaL_newstate failed";
  }
  luaL_openlibs(L);
  register_gz_module(L);

  lua_newtable(L);
  for (const auto& kv : extra_env_strings) {
    if (kv.first.empty())
      continue;
    lua_pushlstring(L, kv.second.c_str(), static_cast<size_t>(kv.second.size()));
    lua_setfield(L, -2, kv.first.c_str());
  }
  lua_setglobal(L, "GZ");

  if (luaL_loadstring(L, lua_source.c_str()) != LUA_OK) {
    const char* s = lua_tostring(L, -1);
    err = s ? s : "luaL_loadstring";
    lua_close(L);
    return err;
  }
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    const char* s = lua_tostring(L, -1);
    err = s ? s : "lua_pcall";
    lua_close(L);
    return err;
  }
  lua_close(L);
  return {};
}

}  // namespace gz
