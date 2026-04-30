-- gz_reverse_cmake: filegen draft for target "frw"
-- 下列 string(REGEX|APPEND|REPLACE)/foreach 为同 Listfile 弱注记, 未执行 CMake; 请手填等价 Lua.
-- 正向: <var type="script" script_type="lua" trigger="configure" …/> + gz.file (见 package-target-xml-spec / gz-cli spec)

-- 同 Listfile 弱注记 (未执行):
--   [CMakeLists.txt:6] REGEX MATCHALL x FRW_MATCHES ${FRW_BUF}
--   [CMakeLists.txt:7] APPEND FRW_OUT x

-- file(READ) 登记 (变量名 = CMake 侧 out-var):
--   FRW_BUF <- gz.file.read("in.txt")

-- file(WRITE) 建议相对路径(相对本 target 的 target.xml 目录, 同 gz 生成物映射规则):
--   gz.file.write("../../../gen_frw.c", content)  -- was E:/ai_github/up/test_projects/gz_reverse_file_read_write_smoke/.intermediate/build/default/gen_frw.c

local GZ = rawget(_G, "GZ")  -- 内嵌时由 GZ 注入; 可删
-- TODO: 按弱注记/读入拼 content, 再 gz.file.write
