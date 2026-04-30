# Embedded Lua 5.5 (upstream sources). See src/Makefile: CORE_O + LIB_O; exclude lua.c, luac.c.
if (NOT CMAKE_C_COMPILER_LOADED)
  enable_language(C)
endif()
set(LUA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LUA_CORE_SOURCES
  lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c
  lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c
  lvm.c lzio.c
)
set(LUA_LIB_SOURCES
  lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c loadlib.c
  loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
)
set(LUA_ALL)
foreach (s ${LUA_CORE_SOURCES} ${LUA_LIB_SOURCES})
  list(APPEND LUA_ALL ${LUA_SRC}/${s})
endforeach()

add_library(lz_embed STATIC ${LUA_ALL})
target_include_directories(lz_embed PUBLIC ${LUA_SRC})
# Lua sources are C89/Windows-friendly when LUA_USE_* set in luaconf.h; MSVC: silence CRT warnings
if (MSVC)
  target_compile_definitions(lz_embed PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if (UNIX AND NOT APPLE)
  target_link_libraries(lz_embed PUBLIC m)
endif()
