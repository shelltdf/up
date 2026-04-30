-- gz_reverse_cmake: filegen draft for package scope (与 package.xml 同目录的 pkg_root 语义)
-- 下列 string(REGEX|APPEND|REPLACE|CONCAT)/foreach/while 为同 Listfile 弱注记, 未执行 CMake; 请手填等价 Lua.
-- 正向: <var type="script" script_type="lua" trigger="configure" …/> + gz.file (见 package-target-xml-spec / gz-cli spec)

-- file(WRITE) 建议相对路径(相对本 target 的 target.xml 目录, 同 gz 生成物映射规则):
--   gz.file.write("__GZ_CMAKE_BINARY_DIR__/gen_fw.c", content)  -- was E:/ai_github/up/test_projects/gz_reverse_file_write_smoke/.intermediate/build/default/gen_fw.c

local GZ = rawget(_G, "GZ")  -- 内嵌时由 GZ 注入; 可删
-- TODO: 按弱注记/读入拼 content, 再 gz.file.write
