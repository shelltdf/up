// up-gui：本地 Win32 外壳，调用与 up-gui.exe 同目录的 up.exe（见 DESIGN.md / mindmap：菜单栏→工具栏→四行→状态栏）。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shobjidl.h>

#include <atomic>
#include <array>
#include <algorithm>  // std::max
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/gui_core_actions.hpp"

namespace {

constexpr wchar_t kClassName[] = L"UpGuiMainClass";
constexpr wchar_t kTitle[] = L"up-gui";

constexpr int IDC_PATH = 101;
constexpr int IDC_BROWSE = 102;
constexpr int IDC_CONFIGURE = 103;
constexpr int IDC_BUILD = 104;
constexpr int IDC_TEST = 105;
constexpr int IDC_RUN = 106;
constexpr int IDC_RUNTARGET = 107;
constexpr int IDC_LOG = 108;
constexpr int IDC_EXTRA = 109;
constexpr int IDC_PACK = 110;
constexpr int IDC_TOOLBAR = 111;
constexpr int IDC_STATUS = 112;
constexpr int IDC_VARS = 113;
constexpr int IDC_LBL_EXTRA = 114;
constexpr int IDC_LBL_RUN = 115;
constexpr int IDC_TESTTARGET = 116;
constexpr int IDC_LBL_TEST = 117;
constexpr int IDC_LBL_PATH = 118;
constexpr int IDC_LBL_VARS = 119;
constexpr int IDC_OPT_GROUP = 120;
constexpr int IDC_OPT_PICK = 121;
constexpr int IDC_OPT_CUSTOM = 122;
constexpr int IDC_OPT_APPLY = 123;
constexpr int IDC_LBL_SCAN = 124;
constexpr int IDC_SCAN_LIST = 125;
constexpr int IDC_SCAN_ADD = 126;
constexpr int IDC_SCAN_REMOVE = 127;
constexpr int IDC_SCAN_UP = 128;
constexpr int IDC_SCAN_DOWN = 129;
constexpr int IDC_VARS_TREE = 130;
constexpr int IDC_ENV_SETTINGS = 131;
constexpr int IDC_LOG_SPLITTER = 132;
constexpr int IDC_LOG_COPY = 133;
constexpr int IDC_LOG_CLEAR = 134;
constexpr int IDM_OPT_COPY_KEY = 2101;
constexpr int IDM_OPT_COPY_KEY_VALUE = 2102;
constexpr int IDM_OPT_RESET_DEFAULT = 2103;

constexpr int IDM_EXIT = 1000;
constexpr int IDM_ABOUT = 1001;

constexpr int IDC_ENV_TAB = 3001;
constexpr int IDC_ENV_AUTO = 3002;
constexpr int IDC_ENV_OK = 3003;
constexpr int IDC_ENV_CANCEL = 3004;
constexpr int IDC_ENV_LOCAL_BUILD = 3005;
constexpr int IDC_ENV_LOCAL_COMPILER = 3006;
constexpr int IDC_ENV_ANDROID_SDK = 3007;
constexpr int IDC_ENV_ANDROID_NDK = 3008;
constexpr int IDC_ENV_EMSDK = 3009;
constexpr int IDC_ENV_LOCAL_BUILD_LIST = 3010;
constexpr int IDC_ENV_LOCAL_COMPILER_LIST = 3011;
constexpr int IDC_ENV_LOCAL_VCVARS_LIST = 3012;

constexpr UINT WM_APPEND_LOG = WM_APP + 50;
constexpr UINT WM_PROCESS_DONE = WM_APP + 51;

HWND g_hwnd{};
HWND g_toolbar{};
HWND g_status{};
HWND g_path{};
HWND g_browse{};
HWND g_lbl_path{};
HWND g_vars{};
HWND g_lbl_vars{};
HWND g_opt_group{};
HWND g_opt_pick{};
HWND g_opt_custom{};
HWND g_opt_apply{};
HWND g_lbl_scan{};
HWND g_scan_list{};
HWND g_scan_add{};
HWND g_scan_remove{};
HWND g_scan_up{};
HWND g_scan_down{};
HWND g_vars_tree{};
HWND g_lbl_extra{};
HWND g_extra{};
HWND g_lbl_run{};
HWND g_run_target{};
HWND g_lbl_test{};
HWND g_test_target{};
HWND g_log{};
HWND g_log_splitter{};
HWND g_log_copy{};
HWND g_log_clear{};

std::atomic<bool> g_running{};
std::wstring g_last_up_args;
std::wstring g_status_text = L"Ready";
unsigned long g_run_seq = 0;
std::vector<int> g_row_to_option_idx;
int g_selected_option_idx = -1;
bool g_dragging_log_splitter = false;
double g_log_top_ratio = 0.0;
int g_log_top_cached = 0;
int g_log_panel_top_cached = 0;
int g_log_panel_h_cached = 0;
int g_log_left_cached = 0;
int g_log_width_cached = 0;
int g_log_splitter_y_cached = 0;
RECT g_log_splitter_rect{};

struct DonePack {
  std::wstring output;
  DWORD exit_code{};
};

struct OptionRow {
  std::wstring name;
  std::wstring value;
  std::wstring choices;
};

struct ToolHit {
  std::wstring name;      // cmake / ninja / msvc / gcc / clang
  std::wstring path;      // executable path
  bool from_path = false; // discovered from PATH
};

struct GuiEnvSettings {
  std::wstring selected_build_system = L"cmake";
  std::wstring selected_compiler = L"msvc";
  std::wstring android_sdk_path;
  std::wstring android_ndk_path;
  std::wstring emsdk_path;
  std::wstring selected_vcvars;
  int vcvars_cl_hits = 0;
  std::wstring vcvars_cl_note;
  std::wstring vcvars_cl_output;
  std::vector<ToolHit> build_hits;
  std::vector<ToolHit> compiler_hits;
  std::vector<ToolHit> vcvars_hits;
  std::vector<std::wstring> android_sdk_hits;
  std::vector<std::wstring> android_ndk_hits;
  std::vector<std::wstring> emsdk_hits;
};

GuiEnvSettings g_env_settings;

std::vector<OptionRow> g_options = {
    {L"UP_TARGET_SYSTEM", L"windows", L"windows | linux | macos | android | ios | emsdk | uwp"},
    {L"UP_TARGET_CPU_ARCH", L"x86_64", L"x86 | x86_64 | arm | arm64"},
    {L"UP_TARGET_DYNAMIC_LIBRARY", L"OFF", L"ON | OFF"},
    {L"UP_TARGET_CRT", L"dynamic_md", L"static_mt | dynamic_md"},
    {L"UP_TARGET_DEBUG", L"OFF", L"ON | OFF"},
    {L"UP_TARGET_BUILD_SYSTEM", L"cmake", L"cmake | ninja"},
    {L"UP_CMAKE_GENERATOR", L"Visual Studio 17 2022", L"(when UP_TARGET_BUILD_SYSTEM=cmake) VS/Ninja/Makefiles"},
};
const std::vector<OptionRow> g_default_options = g_options;

std::string WideToUtf8(std::wstring_view ws) {
  if (ws.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
  if (n <= 0) {
    std::string fallback;
    fallback.reserve(ws.size());
    for (wchar_t c : ws)
      fallback.push_back((c >= 0 && c <= 127) ? static_cast<char>(c) : '?');
    return fallback;
  }
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), n, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(std::string_view utf8) {
  if (utf8.empty())
    return {};
  auto convert = [&](unsigned cp) -> std::wstring {
    const int n = MultiByteToWideChar(cp, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0)
      return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(cp, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
  };
  std::wstring w = convert(CP_UTF8);
  if (!w.empty())
    return w;
  return convert(CP_ACP);
}

void TrimInPlace(std::wstring& s) {
  while (!s.empty() && (s.front() == L' ' || s.front() == L'\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == L' ' || s.back() == L'\t'))
    s.pop_back();
}

void SetStatus(const wchar_t* text) {
  g_status_text = text ? text : L"";
  if (g_status)
    SendMessageW(g_status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(g_status_text.c_str()));
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
  if (!OpenClipboard(owner))
    return false;
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!h) {
    CloseClipboard();
    return false;
  }
  void* p = GlobalLock(h);
  if (!p) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }
  memcpy(p, text.c_str(), bytes);
  GlobalUnlock(h);
  if (!SetClipboardData(CF_UNICODETEXT, h)) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }
  CloseClipboard();
  return true;
}

void AppendLogRaw(const std::wstring& text) {
  if (!g_log)
    return;
  SendMessageW(g_log, EM_SETSEL, -1, -1);
  SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
  // Keep log viewport pinned to latest appended output.
  SendMessageW(g_log, EM_SETSEL, -1, -1);
  SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
}

void AppendLog(const std::wstring& text) {
  if (!IsWindow(g_hwnd))
    return;
  PostMessageW(g_hwnd, WM_APPEND_LOG, 0, reinterpret_cast<LPARAM>(new std::wstring(text)));
}

std::wstring NowTimeStamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t buf[64]{};
  swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u", static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
             static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour), static_cast<unsigned>(st.wMinute),
             static_cast<unsigned>(st.wSecond));
  return buf;
}

std::wstring DirOfModule() {
  wchar_t buf[MAX_PATH]{};
  if (!GetModuleFileNameW(nullptr, buf, MAX_PATH))
    return {};
  std::wstring p(buf);
  const auto pos = p.find_last_of(L"\\/");
  if (pos == std::wstring::npos)
    return {};
  return p.substr(0, pos + 1);
}

std::filesystem::path GuiSettingsPath() {
  return std::filesystem::path(DirOfModule()) / "up_gui_settings.txt";
}

std::wstring GetEnvVarW(const wchar_t* name) {
  wchar_t buf[32767]{};
  const DWORD n = GetEnvironmentVariableW(name, buf, static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf))
    return {};
  return std::wstring(buf, buf + n);
}

std::wstring CmdExePath() {
  std::wstring comspec = GetEnvVarW(L"COMSPEC");
  if (!comspec.empty()) {
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(comspec), ec))
      return comspec;
  }
  std::wstring sysroot = GetEnvVarW(L"SystemRoot");
  if (!sysroot.empty()) {
    std::filesystem::path p = std::filesystem::path(sysroot) / "System32" / "cmd.exe";
    std::error_code ec;
    if (std::filesystem::exists(p, ec))
      return p.wstring();
  }
  return L"cmd.exe";
}

std::vector<std::wstring> SplitPathList(const std::wstring& s) {
  std::vector<std::wstring> out;
  size_t off = 0;
  while (off <= s.size()) {
    const size_t p = s.find(L';', off);
    std::wstring part = s.substr(off, p == std::wstring::npos ? std::wstring::npos : (p - off));
    TrimInPlace(part);
    if (!part.empty())
      out.push_back(part);
    if (p == std::wstring::npos)
      break;
    off = p + 1;
  }
  return out;
}

void AddUnique(std::vector<std::wstring>& v, const std::wstring& s) {
  if (s.empty())
    return;
  for (const auto& x : v) {
    if (_wcsicmp(x.c_str(), s.c_str()) == 0)
      return;
  }
  v.push_back(s);
}

std::wstring NormalizePath(const std::filesystem::path& p) {
  std::error_code ec;
  const auto abs = std::filesystem::absolute(p, ec);
  std::wstring s = (ec ? p : abs).lexically_normal().wstring();
  for (auto& c : s)
    if (c == L'/')
      c = L'\\';
  return s;
}

void AddUniqueHit(std::vector<ToolHit>& hits, const std::wstring& name, const std::filesystem::path& p, bool from_path) {
  const std::wstring norm = NormalizePath(p);
  if (norm.empty())
    return;
  for (const auto& h : hits) {
    if (_wcsicmp(h.path.c_str(), norm.c_str()) == 0)
      return;
  }
  ToolHit h;
  h.name = name;
  h.path = norm;
  h.from_path = from_path;
  hits.push_back(std::move(h));
}

std::vector<std::filesystem::path> CommonToolRoots() {
  std::vector<std::filesystem::path> roots;
  auto add = [&](const std::wstring& p) {
    if (p.empty())
      return;
    std::error_code ec;
    std::filesystem::path x(p);
    if (std::filesystem::exists(x, ec))
      roots.push_back(x);
  };
  add(GetEnvVarW(L"ProgramFiles"));
  add(GetEnvVarW(L"ProgramFiles(x86)"));
  add(GetEnvVarW(L"LOCALAPPDATA"));
  add(std::filesystem::path(GetEnvVarW(L"USERPROFILE")) / "scoop" / "apps");
  add(std::filesystem::path(GetEnvVarW(L"USERPROFILE")) / "AppData" / "Local" / "Programs");
  return roots;
}

void DetectToolHits(const std::wstring& label,
                    const std::vector<std::wstring>& exe_names,
                    const std::vector<std::filesystem::path>& roots,
                    std::vector<ToolHit>& out_hits) {
  out_hits.clear();
  const auto path_dirs = SplitPathList(GetEnvVarW(L"PATH"));
  for (const auto& d : path_dirs) {
    for (const auto& exe : exe_names) {
      std::filesystem::path p = std::filesystem::path(d) / exe;
      std::error_code ec;
      if (std::filesystem::exists(p, ec))
        AddUniqueHit(out_hits, label, p, true);
    }
  }
  for (const auto& r : roots) {
    for (const auto& exe : exe_names) {
      std::error_code ec;
      std::filesystem::path direct = r / exe;
      if (std::filesystem::exists(direct, ec))
        AddUniqueHit(out_hits, label, direct, false);
      std::filesystem::path bin = r / "bin" / exe;
      if (std::filesystem::exists(bin, ec))
        AddUniqueHit(out_hits, label, bin, false);
      for (std::filesystem::directory_iterator it(r, std::filesystem::directory_options::skip_permission_denied, ec), end;
           !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec))
          continue;
        std::filesystem::path q = it->path() / exe;
        if (std::filesystem::exists(q, ec))
          AddUniqueHit(out_hits, label, q, false);
        std::filesystem::path qb = it->path() / "bin" / exe;
        if (std::filesystem::exists(qb, ec))
          AddUniqueHit(out_hits, label, qb, false);
      }
    }
  }
  std::stable_sort(out_hits.begin(), out_hits.end(), [](const ToolHit& a, const ToolHit& b) {
    if (a.from_path != b.from_path)
      return a.from_path > b.from_path;
    return _wcsicmp(a.path.c_str(), b.path.c_str()) < 0;
  });
}

void AddUniqueDirPath(std::vector<std::wstring>& v, const std::filesystem::path& p) {
  std::error_code ec;
  if (!std::filesystem::exists(p, ec))
    return;
  AddUnique(v, NormalizePath(p));
}

bool PathInHits(const std::wstring& p, const std::vector<std::wstring>& hits) {
  if (p.empty())
    return false;
  for (const auto& x : hits) {
    if (_wcsicmp(p.c_str(), x.c_str()) == 0)
      return true;
  }
  return false;
}

std::wstring QuoteWinArg(const std::wstring& s);
bool RunProcessCapture(const std::wstring& app, const std::wstring& cmdline, const std::wstring& cwd,
                       std::wstring& out_w, DWORD& exit_code);

std::wstring PickPreferredPath(const std::wstring& current, const std::vector<std::wstring>& hits) {
  if (!current.empty() && PathInHits(current, hits))
    return current;
  if (!hits.empty())
    return hits.front();
  return current;
}

void DetectAndroidAndEmsdkHits(GuiEnvSettings& s) {
  s.android_sdk_hits.clear();
  s.android_ndk_hits.clear();
  s.emsdk_hits.clear();

  const auto sdk_env1 = GetEnvVarW(L"ANDROID_SDK_ROOT");
  const auto sdk_env2 = GetEnvVarW(L"ANDROID_HOME");
  if (!sdk_env1.empty())
    AddUnique(s.android_sdk_hits, NormalizePath(sdk_env1));
  if (!sdk_env2.empty())
    AddUnique(s.android_sdk_hits, NormalizePath(sdk_env2));
  AddUniqueDirPath(s.android_sdk_hits, std::filesystem::path(GetEnvVarW(L"LOCALAPPDATA")) / "Android" / "Sdk");
  AddUniqueDirPath(s.android_sdk_hits, std::filesystem::path(GetEnvVarW(L"USERPROFILE")) / "Android" / "Sdk");

  const auto ndk_env1 = GetEnvVarW(L"ANDROID_NDK_ROOT");
  const auto ndk_env2 = GetEnvVarW(L"ANDROID_NDK_HOME");
  if (!ndk_env1.empty())
    AddUnique(s.android_ndk_hits, NormalizePath(ndk_env1));
  if (!ndk_env2.empty())
    AddUnique(s.android_ndk_hits, NormalizePath(ndk_env2));
  for (const auto& sdk : s.android_sdk_hits) {
    std::error_code ec;
    const std::filesystem::path sdkp(sdk);
    AddUniqueDirPath(s.android_ndk_hits, sdkp / "ndk-bundle");
    const std::filesystem::path ndk_root = sdkp / "ndk";
    if (std::filesystem::exists(ndk_root, ec)) {
      for (std::filesystem::directory_iterator it(ndk_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
           !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec))
          AddUniqueDirPath(s.android_ndk_hits, it->path());
      }
    }
  }

  const auto emsdk_env = GetEnvVarW(L"EMSDK");
  if (!emsdk_env.empty())
    AddUnique(s.emsdk_hits, NormalizePath(emsdk_env));
  AddUniqueDirPath(s.emsdk_hits, std::filesystem::path(GetEnvVarW(L"USERPROFILE")) / "emsdk");
  AddUniqueDirPath(s.emsdk_hits, std::filesystem::path(GetEnvVarW(L"LOCALAPPDATA")) / "emsdk");
  AddUniqueDirPath(s.emsdk_hits, std::filesystem::path(GetEnvVarW(L"ProgramFiles")) / "emsdk");
  AddUniqueDirPath(s.emsdk_hits, std::filesystem::path(GetEnvVarW(L"ProgramFiles(x86)")) / "emsdk");

#if defined(_WIN32)
  if (s.emsdk_hits.empty()) {
    for (wchar_t d = L'C'; d <= L'Z'; ++d) {
      const std::filesystem::path root = std::wstring(1, d) + L":\\";
      AddUniqueDirPath(s.emsdk_hits, root / "emsdk");
      std::error_code ec;
      for (std::filesystem::directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
           !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec))
          continue;
        std::wstring name = it->path().filename().wstring();
        if (_wcsicmp(name.c_str(), L"emsdk") == 0) {
          AddUniqueDirPath(s.emsdk_hits, it->path());
        }
      }
    }
  }
#endif

  s.android_sdk_path = PickPreferredPath(s.android_sdk_path, s.android_sdk_hits);
  if (!s.android_ndk_hits.empty() && s.android_ndk_path.empty()) {
    auto version_score = [](const std::wstring& p) {
      std::wstring name = std::filesystem::path(p).filename().wstring();
      std::vector<int> nums;
      int cur = -1;
      for (wchar_t c : name) {
        if (c >= L'0' && c <= L'9') {
          if (cur < 0)
            cur = 0;
          cur = cur * 10 + (c - L'0');
        } else if (cur >= 0) {
          nums.push_back(cur);
          cur = -1;
        }
      }
      if (cur >= 0)
        nums.push_back(cur);
      while (nums.size() < 4)
        nums.push_back(0);
      return nums;
    };
    std::wstring best = s.android_ndk_hits.front();
    std::vector<int> best_ver = version_score(best);
    std::error_code bec;
    auto best_time = std::filesystem::last_write_time(std::filesystem::path(best), bec);
    for (const auto& p : s.android_ndk_hits) {
      const auto ver = version_score(p);
      if (ver > best_ver) {
        best = p;
        best_ver = ver;
        std::error_code ec;
        best_time = std::filesystem::last_write_time(std::filesystem::path(best), ec);
        continue;
      }
      if (ver == best_ver) {
        std::error_code ec;
        const auto t = std::filesystem::last_write_time(std::filesystem::path(p), ec);
        if (!ec && (bec || t > best_time)) {
          best = p;
          best_time = t;
          bec = {};
        }
      }
    }
    s.android_ndk_path = best;
  }
  s.emsdk_path = PickPreferredPath(s.emsdk_path, s.emsdk_hits);
}

void DetectVcvarsHits(GuiEnvSettings& s) {
  s.vcvars_hits.clear();
  auto try_add = [&](const std::filesystem::path& p, const std::wstring& name, bool from_env) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec))
      AddUniqueHit(s.vcvars_hits, name, p, from_env);
  };

  const std::wstring vsinst = GetEnvVarW(L"VSINSTALLDIR");
  if (!vsinst.empty()) {
    const auto root = std::filesystem::path(vsinst);
    try_add(root / "VC" / "Auxiliary" / "Build" / "vcvars64.bat", L"vcvars64", true);
    try_add(root / "VC" / "Auxiliary" / "Build" / "vcvars32.bat", L"vcvars32", true);
    try_add(root / "Common7" / "Tools" / "VsDevCmd.bat", L"VsDevCmd", true);
  }
  const std::wstring vctools = GetEnvVarW(L"VCToolsInstallDir");
  if (!vctools.empty()) {
    const auto root = std::filesystem::path(vctools).parent_path().parent_path();
    try_add(root / "Auxiliary" / "Build" / "vcvars64.bat", L"vcvars64", true);
    try_add(root / "Auxiliary" / "Build" / "vcvars32.bat", L"vcvars32", true);
  }

  const std::vector<std::filesystem::path> vs_roots = {
      std::filesystem::path(GetEnvVarW(L"ProgramFiles")) / "Microsoft Visual Studio",
      std::filesystem::path(GetEnvVarW(L"ProgramFiles(x86)")) / "Microsoft Visual Studio"};
  for (const auto& root : vs_roots) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
      continue;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec))
        continue;
      const std::wstring fn = it->path().filename().wstring();
      if (_wcsicmp(fn.c_str(), L"vcvars64.bat") == 0)
        AddUniqueHit(s.vcvars_hits, L"vcvars64", it->path(), false);
      else if (_wcsicmp(fn.c_str(), L"vcvars32.bat") == 0)
        AddUniqueHit(s.vcvars_hits, L"vcvars32", it->path(), false);
      else if (_wcsicmp(fn.c_str(), L"VsDevCmd.bat") == 0)
        AddUniqueHit(s.vcvars_hits, L"VsDevCmd", it->path(), false);
    }
  }

  std::stable_sort(s.vcvars_hits.begin(), s.vcvars_hits.end(), [](const ToolHit& a, const ToolHit& b) {
    auto rank = [](const ToolHit& h) {
      if (h.from_path)
        return 0;
      if (h.name == L"vcvars64")
        return 1;
      if (h.name == L"VsDevCmd")
        return 2;
      return 3;
    };
    const int ra = rank(a);
    const int rb = rank(b);
    if (ra != rb)
      return ra < rb;
    return _wcsicmp(a.path.c_str(), b.path.c_str()) < 0;
  });

  bool sel_ok = false;
  for (const auto& h : s.vcvars_hits) {
    if (_wcsicmp(h.path.c_str(), s.selected_vcvars.c_str()) == 0) {
      sel_ok = true;
      break;
    }
  }
  if (!sel_ok)
    s.selected_vcvars = s.vcvars_hits.empty() ? L"" : s.vcvars_hits.front().path;
}

void DetectCompilerHitsBase(const std::vector<std::filesystem::path>& roots, std::vector<ToolHit>& out) {
  DetectToolHits(L"msvc", {L"cl.exe"}, roots, out);
  std::vector<ToolHit> gcc_hits;
  DetectToolHits(L"gcc", {L"gcc.exe"}, roots, gcc_hits);
  for (const auto& h : gcc_hits)
    AddUniqueHit(out, L"gcc", h.path, h.from_path);
  std::vector<ToolHit> clang_hits;
  DetectToolHits(L"clang", {L"clang.exe"}, roots, clang_hits);
  for (const auto& h : clang_hits)
    AddUniqueHit(out, L"clang", h.path, h.from_path);
}

int AddCompilerHitsFromVcvars(const std::wstring& vcvars, std::vector<ToolHit>& out, std::wstring* note, std::wstring* raw_out) {
  if (note)
    *note = L"";
  if (raw_out)
    raw_out->clear();
  if (vcvars.empty()) {
    if (note)
      *note = L"vcvars 未选择";
    return 0;
  }
  std::error_code ec;
  if (!std::filesystem::exists(std::filesystem::path(vcvars), ec)) {
    if (note)
      *note = L"vcvars 路径不存在";
    return 0;
  }
  std::wstring output;
  DWORD exit_code = 0;
  const std::wstring cmd_exe = CmdExePath();
  std::wstring cmd = L"/d /c \"\"" + vcvars + L"\" >nul && where cl\"";
  if (!RunProcessCapture(cmd_exe, cmd, L"", output, exit_code)) {
    if (note)
      *note = L"where cl 执行失败（cmd 启动失败）";
    return 0;
  }
  if (raw_out)
    *raw_out = output;
  if (exit_code != 0) {
    if (note)
      *note = L"where cl 未命中";
    return 0;
  }
  int hits = 0;
  size_t off = 0;
  while (off <= output.size()) {
    const size_t p = output.find_first_of(L"\r\n", off);
    std::wstring line = output.substr(off, p == std::wstring::npos ? std::wstring::npos : (p - off));
    TrimInPlace(line);
    if (!line.empty()) {
      const size_t before = out.size();
      AddUniqueHit(out, L"msvc", line, false);
      if (out.size() > before)
        ++hits;
    }
    if (p == std::wstring::npos)
      break;
    off = p + 1;
    if (off < output.size() && output[off] == L'\n' && output[p] == L'\r')
      ++off;
  }
  if (note)
    *note = (hits > 0) ? (L"where cl 命中 " + std::to_wstring(hits)) : L"where cl 未命中";
  return hits;
}

void RefreshCompilerHitsForCurrentVcvars(GuiEnvSettings& s) {
  const auto roots = CommonToolRoots();
  s.compiler_hits.clear();
  DetectCompilerHitsBase(roots, s.compiler_hits);
  s.vcvars_cl_hits = AddCompilerHitsFromVcvars(s.selected_vcvars, s.compiler_hits, &s.vcvars_cl_note, &s.vcvars_cl_output);
}

void AutoDetectEnvSettings(GuiEnvSettings& s) {
  const auto roots = CommonToolRoots();
  DetectToolHits(L"cmake", {L"cmake.exe"}, roots, s.build_hits);
  std::vector<ToolHit> ninja_hits;
  DetectToolHits(L"ninja", {L"ninja.exe"}, roots, ninja_hits);
  for (const auto& h : ninja_hits)
    AddUniqueHit(s.build_hits, L"ninja", h.path, h.from_path);

  DetectCompilerHitsBase(roots, s.compiler_hits);

  auto has_name = [](const std::vector<ToolHit>& hits, const std::wstring& name) {
    for (const auto& h : hits) {
      if (_wcsicmp(h.name.c_str(), name.c_str()) == 0)
        return true;
    }
    return false;
  };
  if (!s.build_hits.empty()) {
    if (s.selected_build_system.empty() || !has_name(s.build_hits, s.selected_build_system))
      s.selected_build_system = s.build_hits.front().name;
  } else if (s.selected_build_system.empty()) {
    s.selected_build_system = L"cmake";
  }
  if (!s.compiler_hits.empty()) {
    if (s.selected_compiler.empty() || !has_name(s.compiler_hits, s.selected_compiler))
      s.selected_compiler = s.compiler_hits.front().name;
  } else if (s.selected_compiler.empty()) {
    s.selected_compiler = L"msvc";
  }

  DetectAndroidAndEmsdkHits(s);
  DetectVcvarsHits(s);
  s.vcvars_cl_hits = AddCompilerHitsFromVcvars(s.selected_vcvars, s.compiler_hits, &s.vcvars_cl_note, &s.vcvars_cl_output);
}

void LoadGuiEnvSettings() {
  g_env_settings = GuiEnvSettings{};
  AutoDetectEnvSettings(g_env_settings);
  std::ifstream f(GuiSettingsPath());
  if (!f)
    return;
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    const std::wstring v = Utf8ToWide(line.substr(pos + 1));
    if (k == "local.build_system")
      g_env_settings.selected_build_system = v;
    else if (k == "local.compiler")
      g_env_settings.selected_compiler = v;
    else if (k == "android.sdk" && !v.empty())
      g_env_settings.android_sdk_path = v;
    else if (k == "android.ndk" && !v.empty())
      g_env_settings.android_ndk_path = v;
    else if (k == "emsdk.path" && !v.empty())
      g_env_settings.emsdk_path = v;
    else if (k == "local.vcvars")
      g_env_settings.selected_vcvars = v;
  }
  DetectVcvarsHits(g_env_settings);
}

void SaveGuiEnvSettings() {
  std::ofstream f(GuiSettingsPath(), std::ios::binary | std::ios::trunc);
  if (!f)
    return;
  f << "local.build_system=" << WideToUtf8(g_env_settings.selected_build_system) << "\n";
  f << "local.compiler=" << WideToUtf8(g_env_settings.selected_compiler) << "\n";
  f << "android.sdk=" << WideToUtf8(g_env_settings.android_sdk_path) << "\n";
  f << "android.ndk=" << WideToUtf8(g_env_settings.android_ndk_path) << "\n";
  f << "emsdk.path=" << WideToUtf8(g_env_settings.emsdk_path) << "\n";
  f << "local.vcvars=" << WideToUtf8(g_env_settings.selected_vcvars) << "\n";
}

void AppendConfigureEnvArgs(std::wstring& args_no_exe) {
  auto append_opt = [&](const std::wstring& k, const std::wstring& v) {
    if (v.empty())
      return;
    std::wstring kv = k + L"=" + v;
    args_no_exe += L" --opt ";
    if (kv.find(L' ') != std::wstring::npos)
      args_no_exe += L"\"" + kv + L"\"";
    else
      args_no_exe += kv;
  };
  append_opt(L"UP_TARGET_BUILD_SYSTEM", g_env_settings.selected_build_system);
  append_opt(L"UP_TARGET_COMPILER", g_env_settings.selected_compiler);
  append_opt(L"UP_ANDROID_SDK", g_env_settings.android_sdk_path);
  append_opt(L"UP_ANDROID_NDK", g_env_settings.android_ndk_path);
  append_opt(L"UP_EMSDK_PATH", g_env_settings.emsdk_path);
}

struct EnvDialogState {
  GuiEnvSettings work;
  bool accepted = false;
  bool suppress_list_notify = false;
  HWND tab{};
  HWND lbl_local_build{};
  HWND lv_local_build{};
  HWND lbl_local_vcvars{};
  HWND lv_local_vcvars{};
  HWND lbl_local_compiler{};
  HWND lv_local_compiler{};
  HWND lbl_android_sdk{};
  HWND edt_android_sdk{};
  HWND btn_android_sdk{};
  HWND lbl_android_ndk{};
  HWND edt_android_ndk{};
  HWND btn_android_ndk{};
  HWND lbl_emsdk{};
  HWND edt_emsdk{};
  HWND btn_emsdk{};
  HWND btn_auto{};
  HWND btn_ok{};
  HWND btn_cancel{};
};

bool PickFolder(HWND owner, std::wstring& out);

void EnsureListColumns(HWND lv) {
  if (ListView_GetColumnWidth(lv, 0) > 0)
    return;
  LVCOLUMNW col{};
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  col.cx = 100;
  col.pszText = const_cast<LPWSTR>(L"工具");
  SendMessageW(lv, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
  col.cx = 310;
  col.pszText = const_cast<LPWSTR>(L"路径");
  SendMessageW(lv, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
  col.cx = 90;
  col.pszText = const_cast<LPWSTR>(L"来源");
  SendMessageW(lv, LVM_INSERTCOLUMNW, 2, reinterpret_cast<LPARAM>(&col));
}

void FillToolHitsList(HWND lv, const std::vector<ToolHit>& hits, const std::wstring& selected_name) {
  ListView_DeleteAllItems(lv);
  EnsureListColumns(lv);
  int first_match = -1;
  for (size_t i = 0; i < hits.size(); ++i) {
    LVITEMW it{};
    it.mask = LVIF_TEXT;
    it.iItem = static_cast<int>(i);
    it.pszText = const_cast<LPWSTR>(hits[i].name.c_str());
    SendMessageW(lv, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&it));
    LVITEMW sub{};
    sub.iSubItem = 1;
    sub.pszText = const_cast<LPWSTR>(hits[i].path.c_str());
    SendMessageW(lv, LVM_SETITEMTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&sub));
    const wchar_t* src = hits[i].from_path ? L"PATH" : L"扫描";
    sub.iSubItem = 2;
    sub.pszText = const_cast<LPWSTR>(src);
    SendMessageW(lv, LVM_SETITEMTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&sub));
    if (first_match < 0 && _wcsicmp(hits[i].name.c_str(), selected_name.c_str()) == 0)
      first_match = static_cast<int>(i);
  }
  if (first_match < 0 && !hits.empty())
    first_match = 0;
  if (first_match >= 0) {
    ListView_SetItemState(lv, first_match, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(lv, first_match, FALSE);
  }
}

void FillToolHitsListByPath(HWND lv, const std::vector<ToolHit>& hits, const std::wstring& selected_path) {
  ListView_DeleteAllItems(lv);
  EnsureListColumns(lv);
  int first_match = -1;
  for (size_t i = 0; i < hits.size(); ++i) {
    LVITEMW it{};
    it.mask = LVIF_TEXT;
    it.iItem = static_cast<int>(i);
    it.pszText = const_cast<LPWSTR>(hits[i].name.c_str());
    SendMessageW(lv, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&it));
    LVITEMW sub{};
    sub.iSubItem = 1;
    sub.pszText = const_cast<LPWSTR>(hits[i].path.c_str());
    SendMessageW(lv, LVM_SETITEMTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&sub));
    const wchar_t* src = hits[i].from_path ? L"PATH" : L"扫描";
    sub.iSubItem = 2;
    sub.pszText = const_cast<LPWSTR>(src);
    SendMessageW(lv, LVM_SETITEMTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&sub));
    if (first_match < 0 && _wcsicmp(hits[i].path.c_str(), selected_path.c_str()) == 0)
      first_match = static_cast<int>(i);
  }
  if (first_match < 0 && !hits.empty())
    first_match = 0;
  if (first_match >= 0) {
    ListView_SetItemState(lv, first_match, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(lv, first_match, FALSE);
  }
}

std::wstring SelectedToolNameFromList(HWND lv, const std::vector<ToolHit>& hits) {
  const int idx = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
  if (idx < 0 || static_cast<size_t>(idx) >= hits.size())
    return {};
  return hits[static_cast<size_t>(idx)].name;
}

std::wstring SelectedToolPathFromList(HWND lv, const std::vector<ToolHit>& hits) {
  const int idx = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
  if (idx < 0 || static_cast<size_t>(idx) >= hits.size())
    return {};
  return hits[static_cast<size_t>(idx)].path;
}

std::wstring BuildDetectStatusText(const GuiEnvSettings& s) {
  std::wstringstream ss;
  ss << L"编译环境设置 - 自动搜索：构建命中 " << s.build_hits.size() << L"，vcvars 命中 " << s.vcvars_hits.size()
     << L"，编译器命中 " << s.compiler_hits.size()
     << L"，SDK " << s.android_sdk_hits.size() << L"，NDK " << s.android_ndk_hits.size() << L"，emsdk "
     << s.emsdk_hits.size();
  if (s.android_ndk_hits.empty())
    ss << L"；未命中 Android NDK";
  if (s.emsdk_hits.empty())
    ss << L"；未命中 emsdk";
  if (!s.selected_vcvars.empty())
    ss << L"；" << (s.vcvars_cl_note.empty() ? (L"where cl 命中 " + std::to_wstring(s.vcvars_cl_hits)) : s.vcvars_cl_note);
  return ss.str();
}

std::wstring BuildEnvDetectLogText(const GuiEnvSettings& s) {
  std::wstringstream ss;
  ss << L"[环境搜索] 自动搜索结果\r\n";
  ss << L"  - 选中建造系统: " << (s.selected_build_system.empty() ? L"(空)" : s.selected_build_system) << L"\r\n";
  ss << L"  - 选中 vcvars: " << (s.selected_vcvars.empty() ? L"(空)" : s.selected_vcvars) << L"\r\n";
  ss << L"  - 选中编译器: " << (s.selected_compiler.empty() ? L"(空)" : s.selected_compiler) << L"\r\n";
  ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? L"(空)" : s.android_sdk_path) << L"\r\n";
  ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? L"(空)" : s.android_ndk_path) << L"\r\n";
  ss << L"  - emsdk: " << (s.emsdk_path.empty() ? L"(空)" : s.emsdk_path) << L"\r\n";

  auto dump_hits = [&](const wchar_t* title, const std::vector<ToolHit>& hits) {
    ss << L"  * " << title << L" 命中 " << hits.size() << L" 条\r\n";
    for (const auto& h : hits) {
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : L"扫描") << L")\r\n";
    }
  };
  auto dump_paths = [&](const wchar_t* title, const std::vector<std::wstring>& hits) {
    ss << L"  * " << title << L" 命中 " << hits.size() << L" 条\r\n";
    for (const auto& h : hits)
      ss << L"      - " << h << L"\r\n";
  };

  dump_hits(L"建造系统", s.build_hits);
  dump_hits(L"vcvars", s.vcvars_hits);
  dump_hits(L"编译器", s.compiler_hits);
  dump_paths(L"Android SDK", s.android_sdk_hits);
  dump_paths(L"Android NDK", s.android_ndk_hits);
  dump_paths(L"emsdk", s.emsdk_hits);
  if (!s.vcvars_cl_note.empty())
    ss << L"  * vcvars where cl: " << s.vcvars_cl_note << L"\r\n";
  if (!s.vcvars_cl_output.empty())
    ss << L"  * where cl 输出:\r\n" << s.vcvars_cl_output << L"\r\n";
  return ss.str();
}

std::wstring BuildEnvDetectLogTextForTab(const GuiEnvSettings& s, int tab) {
  std::wstringstream ss;
  if (tab == 0) {
    ss << L"[环境搜索][本地环境]\r\n";
    ss << L"  - 选中建造系统: " << (s.selected_build_system.empty() ? L"(空)" : s.selected_build_system) << L"\r\n";
    ss << L"  - 选中 vcvars: " << (s.selected_vcvars.empty() ? L"(空)" : s.selected_vcvars) << L"\r\n";
    ss << L"  - 选中编译器: " << (s.selected_compiler.empty() ? L"(空)" : s.selected_compiler) << L"\r\n";
    ss << L"  * 建造系统命中: " << s.build_hits.size() << L"\r\n";
    for (const auto& h : s.build_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : L"扫描") << L")\r\n";
    ss << L"  * vcvars 命中: " << s.vcvars_hits.size() << L"\r\n";
    for (const auto& h : s.vcvars_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : L"扫描") << L")\r\n";
    ss << L"  * 编译器命中: " << s.compiler_hits.size() << L"\r\n";
    for (const auto& h : s.compiler_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : L"扫描") << L")\r\n";
    if (!s.vcvars_cl_note.empty())
      ss << L"  * where cl: " << s.vcvars_cl_note << L"\r\n";
  } else if (tab == 1) {
    ss << L"[环境搜索][Android 环境]\r\n";
    ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? L"(空)" : s.android_sdk_path) << L"\r\n";
    ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? L"(空)" : s.android_ndk_path) << L"\r\n";
    ss << L"  * SDK 命中: " << s.android_sdk_hits.size() << L"\r\n";
    for (const auto& p : s.android_sdk_hits)
      ss << L"      - " << p << L"\r\n";
    ss << L"  * NDK 命中: " << s.android_ndk_hits.size() << L"\r\n";
    for (const auto& p : s.android_ndk_hits)
      ss << L"      - " << p << L"\r\n";
  } else {
    ss << L"[环境搜索][emsdk 环境]\r\n";
    ss << L"  - emsdk: " << (s.emsdk_path.empty() ? L"(空)" : s.emsdk_path) << L"\r\n";
    ss << L"  * emsdk 命中: " << s.emsdk_hits.size() << L"\r\n";
    for (const auto& p : s.emsdk_hits)
      ss << L"      - " << p << L"\r\n";
  }
  return ss.str();
}

void LayoutEnvDialog(HWND hwnd, EnvDialogState* st) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int pad = 14;
  const int footer_h = 44;
  const int tab_h = rc.bottom - footer_h - pad * 2;
  MoveWindow(st->tab, pad, pad, rc.right - pad * 2, tab_h, TRUE);
  RECT tr{};
  GetWindowRect(st->tab, &tr);
  POINT p0{tr.left, tr.top};
  POINT p1{tr.right, tr.bottom};
  ScreenToClient(hwnd, &p0);
  ScreenToClient(hwnd, &p1);
  RECT page{p0.x + 18, p0.y + 36, p1.x - 18, p1.y - 14};

  const int lbl_w = 140;
  const int pick_w = 78;
  const int row_h = 28;
  const int row_gap = 12;
  const int local_total_h = (page.bottom - page.top);
  const int list_h = std::max(72, (local_total_h - row_gap * 2) / 3);
  int y = page.top;
  MoveWindow(st->lbl_local_build, page.left, y + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->lv_local_build, page.left + lbl_w + 6, y, page.right - page.left - lbl_w - 6, list_h, TRUE);
  y += list_h + row_gap;
  MoveWindow(st->lbl_local_vcvars, page.left, y + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->lv_local_vcvars, page.left + lbl_w + 6, y, page.right - page.left - lbl_w - 6, list_h, TRUE);
  y += list_h + row_gap;
  MoveWindow(st->lbl_local_compiler, page.left, y + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->lv_local_compiler, page.left + lbl_w + 6, y, page.right - page.left - lbl_w - 6, list_h, TRUE);

  y = page.top;
  MoveWindow(st->lbl_android_sdk, page.left, y + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->edt_android_sdk, page.left + lbl_w + 6, y, page.right - page.left - lbl_w - 6 - pick_w - 6, row_h, TRUE);
  MoveWindow(st->btn_android_sdk, page.right - pick_w, y, pick_w, row_h, TRUE);
  y += row_h + row_gap;
  MoveWindow(st->lbl_android_ndk, page.left, y + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->edt_android_ndk, page.left + lbl_w + 6, y, page.right - page.left - lbl_w - 6 - pick_w - 6, row_h, TRUE);
  MoveWindow(st->btn_android_ndk, page.right - pick_w, y, pick_w, row_h, TRUE);

  MoveWindow(st->lbl_emsdk, page.left, page.top + 3, lbl_w, row_h, TRUE);
  MoveWindow(st->edt_emsdk, page.left + lbl_w + 6, page.top, page.right - page.left - lbl_w - 6 - pick_w - 6, row_h, TRUE);
  MoveWindow(st->btn_emsdk, page.right - pick_w, page.top, pick_w, row_h, TRUE);

  const int btn_y = rc.bottom - 34 - pad;
  MoveWindow(st->btn_auto, pad, btn_y, 108, 30, TRUE);
  MoveWindow(st->btn_cancel, rc.right - pad - 96, btn_y, 96, 30, TRUE);
  MoveWindow(st->btn_ok, rc.right - pad - 96 - 104, btn_y, 96, 30, TRUE);
}

void ShowEnvTab(EnvDialogState* st, int tab) {
  auto set_vis = [](HWND h, bool vis) { ShowWindow(h, vis ? SW_SHOW : SW_HIDE); };
  const bool local = tab == 0;
  const bool android = tab == 1;
  const bool emsdk = tab == 2;
  set_vis(st->lbl_local_build, local);
  set_vis(st->lv_local_build, local);
  set_vis(st->lbl_local_vcvars, local);
  set_vis(st->lv_local_vcvars, local);
  set_vis(st->lbl_local_compiler, local);
  set_vis(st->lv_local_compiler, local);
  set_vis(st->lbl_android_sdk, android);
  set_vis(st->edt_android_sdk, android);
  set_vis(st->btn_android_sdk, android);
  set_vis(st->lbl_android_ndk, android);
  set_vis(st->edt_android_ndk, android);
  set_vis(st->btn_android_ndk, android);
  set_vis(st->lbl_emsdk, emsdk);
  set_vis(st->edt_emsdk, emsdk);
  set_vis(st->btn_emsdk, emsdk);
}

void PullEnvDialogValues(EnvDialogState* st) {
  const std::wstring build = SelectedToolNameFromList(st->lv_local_build, st->work.build_hits);
  const std::wstring comp = SelectedToolNameFromList(st->lv_local_compiler, st->work.compiler_hits);
  if (!build.empty())
    st->work.selected_build_system = build;
  const std::wstring vcvars = SelectedToolPathFromList(st->lv_local_vcvars, st->work.vcvars_hits);
  if (!vcvars.empty())
    st->work.selected_vcvars = vcvars;
  if (!comp.empty())
    st->work.selected_compiler = comp;
  wchar_t buf[2048]{};
  GetWindowTextW(st->edt_android_sdk, buf, static_cast<int>(std::size(buf)));
  st->work.android_sdk_path = buf;
  GetWindowTextW(st->edt_android_ndk, buf, static_cast<int>(std::size(buf)));
  st->work.android_ndk_path = buf;
  GetWindowTextW(st->edt_emsdk, buf, static_cast<int>(std::size(buf)));
  st->work.emsdk_path = buf;
}

void PushEnvDialogValues(EnvDialogState* st) {
  st->suppress_list_notify = true;
  st->work.android_sdk_path = PickPreferredPath(st->work.android_sdk_path, st->work.android_sdk_hits);
  st->work.android_ndk_path = PickPreferredPath(st->work.android_ndk_path, st->work.android_ndk_hits);
  st->work.emsdk_path = PickPreferredPath(st->work.emsdk_path, st->work.emsdk_hits);
  FillToolHitsList(st->lv_local_build, st->work.build_hits, st->work.selected_build_system);
  FillToolHitsListByPath(st->lv_local_vcvars, st->work.vcvars_hits, st->work.selected_vcvars);
  FillToolHitsList(st->lv_local_compiler, st->work.compiler_hits, st->work.selected_compiler);
  SetWindowTextW(st->edt_android_sdk, st->work.android_sdk_path.c_str());
  SetWindowTextW(st->edt_android_ndk, st->work.android_ndk_path.c_str());
  SetWindowTextW(st->edt_emsdk, st->work.emsdk_path.c_str());
  st->suppress_list_notify = false;
}

LRESULT CALLBACK EnvSettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* st = reinterpret_cast<EnvDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
      st = reinterpret_cast<EnvDialogState*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
      const HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      st->tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 100, hwnd,
                                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_TAB)), GetModuleHandleW(nullptr), nullptr);
      TCITEMW ti{};
      ti.mask = TCIF_TEXT;
      ti.pszText = const_cast<LPWSTR>(L"本地环境");
      SendMessageW(st->tab, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ti));
      ti.pszText = const_cast<LPWSTR>(L"Android 环境");
      SendMessageW(st->tab, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&ti));
      ti.pszText = const_cast<LPWSTR>(L"emsdk 环境");
      SendMessageW(st->tab, TCM_INSERTITEMW, 2, reinterpret_cast<LPARAM>(&ti));

      st->lbl_local_build = CreateWindowExW(0, L"STATIC", L"建造系统", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                            GetModuleHandleW(nullptr), nullptr);
      st->lv_local_build = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                           WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                           0, 0, 100, 100, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_BUILD_LIST)),
                                           GetModuleHandleW(nullptr), nullptr);
      st->lbl_local_vcvars = CreateWindowExW(0, L"STATIC", L"VS vcvars", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                             GetModuleHandleW(nullptr), nullptr);
      st->lv_local_vcvars = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                            0, 0, 100, 100, hwnd,
                                            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_VCVARS_LIST)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->lbl_local_compiler = CreateWindowExW(0, L"STATIC", L"编译器", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                               GetModuleHandleW(nullptr), nullptr);
      st->lv_local_compiler = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                              WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                              0, 0, 100, 100, hwnd,
                                              reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_COMPILER_LIST)),
                                              GetModuleHandleW(nullptr), nullptr);
      st->lbl_android_sdk = CreateWindowExW(0, L"STATIC", L"Android SDK 路径", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->edt_android_sdk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                            0, 0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_ANDROID_SDK)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->btn_android_sdk = CreateWindowExW(0, L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->lbl_android_ndk = CreateWindowExW(0, L"STATIC", L"Android NDK 路径", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->edt_android_ndk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                            0, 0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_ANDROID_NDK)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->btn_android_ndk = CreateWindowExW(0, L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->lbl_emsdk = CreateWindowExW(0, L"STATIC", L"emsdk 路径", WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
      st->edt_emsdk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                      0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_EMSDK)),
                                      GetModuleHandleW(nullptr), nullptr);
      st->btn_emsdk = CreateWindowExW(0, L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                      nullptr, GetModuleHandleW(nullptr), nullptr);
      st->btn_auto = CreateWindowExW(0, L"BUTTON", L"自动搜索", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_AUTO)),
                                     GetModuleHandleW(nullptr), nullptr);
      st->btn_ok = CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_OK)),
                                   GetModuleHandleW(nullptr), nullptr);
      st->btn_cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_CANCEL)),
                                       GetModuleHandleW(nullptr), nullptr);
      SendMessageW(st->tab, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_local_build, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lv_local_build, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_local_vcvars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lv_local_vcvars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_local_compiler, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lv_local_compiler, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_android_sdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->edt_android_sdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_android_sdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_android_ndk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->edt_android_ndk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_android_ndk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->lbl_emsdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->edt_emsdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_emsdk, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_auto, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_ok, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_cancel, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      ListView_SetExtendedListViewStyle(st->lv_local_build, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
      ListView_SetExtendedListViewStyle(st->lv_local_vcvars, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
      ListView_SetExtendedListViewStyle(st->lv_local_compiler, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

      PushEnvDialogValues(st);
      SetWindowTextW(hwnd, BuildDetectStatusText(st->work).c_str());
      ShowEnvTab(st, 0);
      return 0;
    }
    case WM_SIZE:
      if (st)
        LayoutEnvDialog(hwnd, st);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 760;
      mmi->ptMinTrackSize.y = 500;
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (!st)
        return 0;
      if (id == IDC_ENV_AUTO) {
        PullEnvDialogValues(st);
        AutoDetectEnvSettings(st->work);
        PushEnvDialogValues(st);
        SetWindowTextW(hwnd, BuildDetectStatusText(st->work).c_str());
        const int tab = st->tab ? TabCtrl_GetCurSel(st->tab) : 0;
        AppendLog(BuildEnvDetectLogTextForTab(st->work, tab));
        return 0;
      }
      if ((HWND)lParam == st->btn_android_sdk) {
        std::wstring folder;
        if (PickFolder(hwnd, folder)) {
          SetWindowTextW(st->edt_android_sdk, folder.c_str());
          PullEnvDialogValues(st);
          PushEnvDialogValues(st);
        }
        return 0;
      }
      if ((HWND)lParam == st->btn_android_ndk) {
        std::wstring folder;
        if (PickFolder(hwnd, folder)) {
          SetWindowTextW(st->edt_android_ndk, folder.c_str());
          PullEnvDialogValues(st);
          PushEnvDialogValues(st);
        }
        return 0;
      }
      if ((HWND)lParam == st->btn_emsdk) {
        std::wstring folder;
        if (PickFolder(hwnd, folder)) {
          SetWindowTextW(st->edt_emsdk, folder.c_str());
          PullEnvDialogValues(st);
          PushEnvDialogValues(st);
        }
        return 0;
      }
      if (id == IDC_ENV_OK) {
        PullEnvDialogValues(st);
        st->accepted = true;
        DestroyWindow(hwnd);
        return 0;
      }
      if (id == IDC_ENV_CANCEL) {
        st->accepted = false;
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
    case WM_NOTIFY:
      if (st) {
        const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
        if (hdr && hdr->idFrom == IDC_ENV_TAB && hdr->code == TCN_SELCHANGE) {
          ShowEnvTab(st, TabCtrl_GetCurSel(st->tab));
          return 0;
        }
        if (hdr && hdr->code == LVN_ITEMCHANGED &&
            (hdr->idFrom == IDC_ENV_LOCAL_BUILD_LIST || hdr->idFrom == IDC_ENV_LOCAL_VCVARS_LIST ||
             hdr->idFrom == IDC_ENV_LOCAL_COMPILER_LIST)) {
          if (st->suppress_list_notify)
            return 0;
          const auto* nmlv = reinterpret_cast<const NMLISTVIEW*>(lParam);
          if (!nmlv || !(nmlv->uChanged & LVIF_STATE) || !(nmlv->uNewState & LVIS_SELECTED))
            return 0;
          if (hdr->idFrom == IDC_ENV_LOCAL_VCVARS_LIST) {
            if (nmlv->iItem >= 0 && static_cast<size_t>(nmlv->iItem) < st->work.vcvars_hits.size())
              st->work.selected_vcvars = st->work.vcvars_hits[static_cast<size_t>(nmlv->iItem)].path;
          }
          PullEnvDialogValues(st);
          if (hdr->idFrom == IDC_ENV_LOCAL_VCVARS_LIST) {
            RefreshCompilerHitsForCurrentVcvars(st->work);
            PushEnvDialogValues(st);
            if (!st->work.vcvars_cl_note.empty())
              SetStatus(st->work.vcvars_cl_note.c_str());
            AppendLog(L"[调试] vcvars 选中: " + st->work.selected_vcvars + L"\r\n");
            AppendLog(L"[调试] where cl: " + st->work.vcvars_cl_note + L"\r\n");
            if (!st->work.vcvars_cl_output.empty())
              AppendLog(L"[调试] where cl 原始输出:\r\n" + st->work.vcvars_cl_output + L"\r\n");
          }
          SetWindowTextW(hwnd, BuildDetectStatusText(st->work).c_str());
          return 0;
        }
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool ShowEnvSettingsDialog(HWND owner) {
  static bool cls_registered = false;
  static constexpr wchar_t kEnvClass[] = L"UpGuiEnvSettingsClass";
  if (!cls_registered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = EnvSettingsWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kEnvClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    cls_registered = true;
  }

  EnvDialogState st{};
  st.work = g_env_settings;
  EnableWindow(owner, FALSE);
  RECT rc{};
  GetWindowRect(owner, &rc);
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kEnvClass, L"编译环境设置",
                             WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE | WS_THICKFRAME,
                             rc.left + 32, rc.top + 28, 860, 560, owner, nullptr, GetModuleHandleW(nullptr), &st);
  if (!dlg) {
    EnableWindow(owner, TRUE);
    return false;
  }
  MSG msg{};
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetActiveWindow(owner);
  if (st.accepted) {
    g_env_settings = std::move(st.work);
    SaveGuiEnvSettings();
    return true;
  }
  return false;
}

std::wstring UpExePath() {
  return DirOfModule() + L"up.exe";
}

bool PickFolder(HWND owner, std::wstring& out) {
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
    return false;
  DWORD opt = 0;
  dlg->GetOptions(&opt);
  dlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  if (FAILED(dlg->Show(owner))) {
    dlg->Release();
    return false;
  }
  IShellItem* item = nullptr;
  if (FAILED(dlg->GetResult(&item))) {
    dlg->Release();
    return false;
  }
  PWSTR path = nullptr;
  if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
    item->Release();
    dlg->Release();
    return false;
  }
  out.assign(path);
  CoTaskMemFree(path);
  item->Release();
  dlg->Release();
  return true;
}

void GetEditText(HWND ed, std::wstring& out);
void RebuildOptionsListView();
void RebuildOptionsTreeView();

std::string ReadTextFileUtf8BestEffort(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    return {};
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string ExtractXmlAttr(const std::string& xml, const char* attr) {
  const std::string key = std::string(attr) + "=\"";
  const auto k = xml.find(key);
  if (k == std::string::npos)
    return {};
  const auto b = k + key.size();
  const auto e = xml.find('"', b);
  if (e == std::string::npos || e <= b)
    return {};
  return xml.substr(b, e - b);
}

std::string ToLowerAscii(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

std::vector<std::wstring> DiscoverExecutableTargets(const std::wstring& cwd_w, bool test_only) {
  std::vector<std::wstring> out;
  if (cwd_w.empty())
    return out;
  std::set<std::wstring> uniq;
  std::error_code ec;
  const std::filesystem::path root(cwd_w);
  if (!std::filesystem::exists(root, ec))
    return out;
  for (std::filesystem::recursive_directory_iterator it(
           root, std::filesystem::directory_options::skip_permission_denied, ec),
       end;
       !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    if (it->path().filename() != "target.xml")
      continue;
    const std::string xml = ReadTextFileUtf8BestEffort(it->path());
    if (xml.empty())
      continue;
    if (ExtractXmlAttr(xml, "type") != "executable")
      continue;
    const std::string name = ExtractXmlAttr(xml, "name");
    if (name.empty())
      continue;
    const std::string l = ToLowerAscii(name);
    const bool is_test_like = l.find("test") != std::string::npos;
    if (test_only && !is_test_like)
      continue;
    if (!test_only && is_test_like)
      continue;
    if (!name.empty())
      uniq.insert(Utf8ToWide(name));
  }
  out.assign(uniq.begin(), uniq.end());
  return out;
}

void RefreshRunTargetListFromPath() {
  if (!g_run_target)
    return;
  std::wstring cwd;
  GetEditText(g_path, cwd);
  SendMessageW(g_run_target, CB_RESETCONTENT, 0, 0);
  const auto targets = DiscoverExecutableTargets(cwd, false);
  for (const auto& t : targets)
    SendMessageW(g_run_target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
  if (!targets.empty()) {
    SendMessageW(g_run_target, CB_SETCURSEL, 0, 0);
  }

  if (g_test_target)
    SendMessageW(g_test_target, CB_RESETCONTENT, 0, 0);
  const auto tests = DiscoverExecutableTargets(cwd, true);
  for (const auto& t : tests) {
    if (g_test_target)
      SendMessageW(g_test_target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
  }
  if (!tests.empty() && g_test_target) {
    SendMessageW(g_test_target, CB_SETCURSEL, 0, 0);
  }

  if (!targets.empty() || !tests.empty())
    SetStatus(L"Run/Test targets refreshed");
  else
    SetStatus(L"No run/test targets found");
}

std::filesystem::path ResolveUpCachePath(const std::wstring& cwd) {
  if (cwd.empty())
    return {};
  std::error_code ec;
  const auto root = std::filesystem::path(cwd) / ".intermediate" / "build";
  if (!std::filesystem::exists(root, ec))
    return {};
  std::filesystem::path best;
  std::filesystem::file_time_type best_time = std::filesystem::file_time_type::min();
  for (const auto& e : std::filesystem::directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec)) {
    if (ec)
      break;
    if (!e.is_directory(ec))
      continue;
    const auto p = e.path() / "up_cache.txt";
    if (!std::filesystem::exists(p, ec))
      continue;
    std::error_code tec;
    const auto t = std::filesystem::last_write_time(p, tec);
    if (tec)
      continue;
    if (best.empty() || t > best_time) {
      best = p;
      best_time = t;
    }
  }
  return best;
}

void LoadOptionsFromCache(const std::wstring& cwd, bool restore_scan_roots = true) {
  const auto cache = ResolveUpCachePath(cwd);
  if (cache.empty()) {
    g_options = g_default_options;
    g_selected_option_idx = g_options.empty() ? -1 : 0;
    RebuildOptionsListView();
    return;
  }
  std::ifstream f(cache);
  if (!f) {
    g_options = g_default_options;
    g_selected_option_idx = g_options.empty() ? -1 : 0;
    RebuildOptionsListView();
    return;
  }
  std::vector<std::wstring> scan_roots;
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    const std::string v = line.substr(pos + 1);
    if (k == "scan_roots") {
      std::string rest = v;
      size_t off = 0;
      while (off <= rest.size()) {
        const size_t p = rest.find(';', off);
        const std::string part = rest.substr(off, p == std::string::npos ? std::string::npos : (p - off));
        if (!part.empty())
          scan_roots.push_back(Utf8ToWide(part));
        if (p == std::string::npos)
          break;
        off = p + 1;
      }
      continue;
    }
    if (k.rfind("UP_", 0) != 0)
      continue;
    const std::wstring wk = Utf8ToWide(k);
    const std::wstring wv = Utf8ToWide(v);
    for (auto& o : g_options) {
      if (o.name == wk) {
        o.value = wv;
        break;
      }
    }
  }
  if (restore_scan_roots && g_scan_list) {
    SendMessageW(g_scan_list, LB_RESETCONTENT, 0, 0);
    for (const auto& d : scan_roots)
      SendMessageW(g_scan_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(d.c_str()));
  }
  RebuildOptionsListView();
}

void AppendConfigureOptions(std::wstring& args_no_exe) {
  for (const auto& o : g_options) {
    std::string kv = WideToUtf8(o.name) + "=" + WideToUtf8(o.value);
    std::wstring wkv = Utf8ToWide(kv);
    args_no_exe += L" --opt ";
    if (wkv.find(L' ') != std::wstring::npos)
      args_no_exe += L"\"" + wkv + L"\"";
    else
      args_no_exe += wkv;
  }
}

std::vector<std::wstring> GetScanDirsFromList() {
  std::vector<std::wstring> out;
  if (!g_scan_list)
    return out;
  const int cnt = static_cast<int>(SendMessageW(g_scan_list, LB_GETCOUNT, 0, 0));
  for (int i = 0; i < cnt; ++i) {
    const int n = static_cast<int>(SendMessageW(g_scan_list, LB_GETTEXTLEN, static_cast<WPARAM>(i), 0));
    if (n <= 0)
      continue;
    std::wstring s(static_cast<size_t>(n), L'\0');
    SendMessageW(g_scan_list, LB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(s.data()));
    if (!s.empty())
      out.push_back(s);
  }
  return out;
}

void ResetScanDirsForCwd(const std::wstring& cwd) {
  (void)cwd;
  if (!g_scan_list)
    return;
  SendMessageW(g_scan_list, LB_RESETCONTENT, 0, 0);
}

void AppendConfigureScanDirs(std::wstring& args_no_exe) {
  const auto dirs = GetScanDirsFromList();
  std::wstring cwd;
  GetEditText(g_path, cwd);
  up::gui::core::append_scan_args(args_no_exe, dirs, cwd);
}

std::wstring QuoteWinArg(const std::wstring& s) {
  if (s.empty())
    return L"\"\"";
  if (s.find_first_of(L" \t\"") == std::wstring::npos)
    return s;
  std::wstring out = L"\"";
  for (wchar_t c : s) {
    if (c == L'"')
      out += L'\\';
    out += c;
  }
  out += L"\"";
  return out;
}

void GetEditText(HWND ed, std::wstring& out) {
  const int n = GetWindowTextLengthW(ed);
  if (n <= 0) {
    out.clear();
    return;
  }
  out.resize(static_cast<size_t>(n) + 1);
  GetWindowTextW(ed, out.data(), n + 1);
  out.resize(static_cast<size_t>(n));
}

bool RunProcessCapture(const std::wstring& app, const std::wstring& cmdline, const std::wstring& cwd,
                       std::wstring& out_w, DWORD& exit_code) {
  exit_code = static_cast<DWORD>(-1);
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = nullptr;
  sa.bInheritHandle = TRUE;

  HANDLE out_rd = nullptr, out_wr = nullptr;
  HANDLE in_rd = nullptr, in_wr = nullptr;
  if (!CreatePipe(&out_rd, &out_wr, &sa, 0))
    return false;
  if (!CreatePipe(&in_rd, &in_wr, &sa, 0)) {
    CloseHandle(out_rd);
    CloseHandle(out_wr);
    return false;
  }
  SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdInput = in_rd;
  si.hStdOutput = out_wr;
  si.hStdError = out_wr;

  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> mutable_cmd(cmdline.begin(), cmdline.end());
  mutable_cmd.push_back(0);

  const BOOL ok = CreateProcessW(app.c_str(), mutable_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                 cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
  CloseHandle(in_rd);
  CloseHandle(out_wr);
  CloseHandle(in_wr);
  if (!ok) {
    CloseHandle(out_rd);
    return false;
  }

  std::string acc;
  char buf[4096]{};
  for (;;) {
    DWORD n = 0;
    if (!ReadFile(out_rd, buf, sizeof(buf) - 1, &n, nullptr) || n == 0)
      break;
    acc.append(buf, static_cast<size_t>(n));
  }
  CloseHandle(out_rd);
  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  out_w = Utf8ToWide(acc);
  return true;
}

void SetUiRunning(bool running) {
  g_running = running;
  const BOOL en = running ? FALSE : TRUE;
  EnableWindow(g_toolbar, en);
  EnableWindow(g_browse, en);
  EnableWindow(g_path, en);
  EnableWindow(g_scan_list, en);
  EnableWindow(g_scan_add, en);
  EnableWindow(g_scan_remove, en);
  EnableWindow(g_scan_up, en);
  EnableWindow(g_scan_down, en);
  EnableWindow(g_extra, en);
  EnableWindow(g_run_target, en);
  EnableWindow(g_test_target, en);
  EnableWindow(g_vars, en);
  HMENU menu = GetMenu(g_hwnd);
  if (menu) {
    const UINT gray = MF_BYCOMMAND | MF_GRAYED;
    const UINT ena = MF_BYCOMMAND | MF_ENABLED;
    EnableMenuItem(menu, IDM_EXIT, ena);
    EnableMenuItem(menu, IDM_ABOUT, running ? gray : ena);
    EnableMenuItem(menu, IDC_CONFIGURE, running ? gray : ena);
    EnableMenuItem(menu, IDC_BUILD, running ? gray : ena);
    EnableMenuItem(menu, IDC_TEST, running ? gray : ena);
    EnableMenuItem(menu, IDC_PACK, running ? gray : ena);
    EnableMenuItem(menu, IDC_RUN, running ? gray : ena);
    EnableMenuItem(menu, IDC_ENV_SETTINGS, running ? gray : ena);
  }
  SetStatus(running ? L"Running up..." : L"Ready");
}

void RunUpAsync(std::wstring args_no_exe) {
  const std::wstring up = UpExePath();
  if (GetFileAttributesW(up.c_str()) == INVALID_FILE_ATTRIBUTES) {
    AppendLog(L"[错误] 找不到 up.exe，请与 up-gui.exe 放在同一目录。\r\n");
    return;
  }

  std::wstring cwd;
  GetEditText(g_path, cwd);
  if (cwd.empty()) {
    AppendLog(L"[错误] 请先填写或选择「当前工作目录 (CWD)」。\r\n");
    return;
  }

  std::wstring extra;
  GetEditText(g_extra, extra);
  TrimInPlace(extra);

  if (args_no_exe.rfind(L"configure", 0) == 0)
    AppendConfigureScanDirs(args_no_exe);
  if (args_no_exe.rfind(L"configure", 0) == 0)
    AppendConfigureOptions(args_no_exe);
  if (args_no_exe.rfind(L"configure", 0) == 0)
    AppendConfigureEnvArgs(args_no_exe);

  std::wstring run_app = up;
  bool use_vcvars = false;
  const std::wstring vcvars = g_env_settings.selected_vcvars;
  if (!vcvars.empty()) {
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(vcvars), ec))
      use_vcvars = true;
  }
  const auto plan = up::gui::core::build_launch_plan(
      up, args_no_exe, extra, use_vcvars ? vcvars : L"", use_vcvars ? CmdExePath() : L"");
  run_app = plan.process_path;
  std::wstring cmd = plan.command_line;

  HWND hwnd = g_hwnd;
  g_last_up_args = args_no_exe;
  const unsigned long run_no = ++g_run_seq;
  AppendLog(L"\r\n==== Run #" + std::to_wstring(run_no) + L" @ " + NowTimeStamp() + L" ====\r\n");
  if (plan.use_vcvars)
    AppendLog(L"\r\n> [vcvars] " + vcvars + L"\r\n");
  AppendLog(L"\r\n> " + plan.display_command + L"\r\n");

  SetUiRunning(true);

  std::thread([run_app, cmd, cwd, hwnd]() {
    std::wstring output;
    DWORD code = 0;
    const bool ok = RunProcessCapture(run_app, cmd, cwd, output, code);
    auto* pack = new DonePack;
    if (ok)
      pack->output = std::move(output);
    else
      pack->output = L"[错误] CreateProcess 失败。\r\n";
    pack->exit_code = code;
    PostMessageW(hwnd, WM_PROCESS_DONE, ok ? 1 : 0, reinterpret_cast<LPARAM>(pack));
  }).detach();
}

int StatusBarHeight(HWND sb) {
  if (!sb || !IsWindow(sb))
    return 22;
  RECT r{};
  GetWindowRect(sb, &r);
  const int h = static_cast<int>(r.bottom - r.top);
  return h < 18 ? 18 : h;
}

int ToolbarHeight(HWND tb) {
  if (!tb || !IsWindow(tb))
    return 0;
  SIZE sz{};
  SendMessageW(tb, TB_GETMAXSIZE, 0, reinterpret_cast<LPARAM>(&sz));
  const int cy = static_cast<int>(sz.cy);
  return cy < 28 ? 28 : cy;
}

void LayoutChildren(HWND hwnd) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;
  if (w < 200 || h < 160)
    return;

  const int pad = 6;
  const int editH = 24;
  const int btnW = 70;
  const int lblW = 68;

  SendMessageW(g_status, WM_SIZE, 0, 0);
  const int cyStatus = StatusBarHeight(g_status);
  MoveWindow(g_status, 0, h - cyStatus, w, cyStatus, TRUE);
  int status_parts[1] = {w - 1};
  SendMessageW(g_status, SB_SETPARTS, 1, reinterpret_cast<LPARAM>(status_parts));
  SendMessageW(g_status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(g_status_text.c_str()));

  const int contentBottom = h - cyStatus;
  int y = 0;

  const int tbh = ToolbarHeight(g_toolbar);
  MoveWindow(g_toolbar, 0, y, w, tbh, TRUE);
  SendMessageW(g_toolbar, TB_AUTOSIZE, 0, 0);
  y = tbh + pad;

  // 第一行：当前工作目录（CWD） + 浏览
  MoveWindow(g_lbl_path, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_path, pad + lblW + 4, y + 2, std::max(120, w - pad * 2 - btnW - lblW - 8), editH, TRUE);
  MoveWindow(g_browse, w - pad - btnW, y, btnW, editH + 4, TRUE);
  y += editH + pad + 6;

  // CWD 下方：scan dir 列表（顺序即优先级）
  MoveWindow(g_lbl_scan, pad, y + 4, lblW, 20, TRUE);
  const int scanLeft = pad + lblW + 4;
  const int scanW = w - pad * 2 - lblW - 4 - 72;
  const int scanH = 104;
  MoveWindow(g_scan_list, scanLeft, y, std::max(180, scanW), scanH, TRUE);
  MoveWindow(g_scan_add, scanLeft + std::max(180, scanW) + 6, y, 66, 24, TRUE);
  MoveWindow(g_scan_remove, scanLeft + std::max(180, scanW) + 6, y + 26, 66, 24, TRUE);
  MoveWindow(g_scan_up, scanLeft + std::max(180, scanW) + 6, y + 52, 66, 24, TRUE);
  MoveWindow(g_scan_down, scanLeft + std::max(180, scanW) + 6, y + 78, 66, 24, TRUE);
  y += scanH + pad;

  // 第二行：Option 列表 + 分组开关
  MoveWindow(g_lbl_vars, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_opt_group, w - pad - 170, y + 2, 170, 24, TRUE);
  y += 24;
  // 先为后续区域预留最小空间，避免默认窗口时底部控件被挤没。
  const int reserveBelowVars = (editH + pad) * 4 + 92;
  int varsH = (contentBottom - y - reserveBelowVars);
  if (varsH < 56)
    varsH = 56;
  if (varsH > 104)
    varsH = 104;
  MoveWindow(g_vars, pad + lblW + 4, y, w - pad * 2 - lblW - 4, varsH, TRUE);
  MoveWindow(g_vars_tree, pad + lblW + 4, y, w - pad * 2 - lblW - 4, varsH, TRUE);
  const bool grouped = g_opt_group && (SendMessageW(g_opt_group, BM_GETCHECK, 0, 0) == BST_CHECKED);
  ShowWindow(g_vars_tree, grouped ? SW_SHOW : SW_HIDE);
  ShowWindow(g_vars, grouped ? SW_HIDE : SW_SHOW);
  y += varsH + pad;

  // Option 编辑：可选 + 自定义
  const int optLeft = pad + lblW + 4;
  const int optW = w - pad * 2 - lblW - 4;
  MoveWindow(g_opt_pick, optLeft, y, std::max(180, optW / 3), editH + 140, TRUE);
  MoveWindow(g_opt_custom, optLeft + std::max(180, optW / 3) + 8, y, std::max(180, optW / 3), editH, TRUE);
  MoveWindow(g_opt_apply, optLeft + std::max(180, optW / 3) * 2 + 16, y - 1, 120, editH + 2, TRUE);
  y += editH + pad;

  // 第三行区块：附加参数
  MoveWindow(g_lbl_extra, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_extra, pad + lblW + 4, y, w - pad * 2 - lblW - 4, editH, TRUE);
  y += editH + pad;

  // 第三行：运行目标列表（configure 成功后刷新）
  MoveWindow(g_lbl_run, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_run_target, pad + lblW + 4, y, w - pad * 2 - lblW - 4, 240, TRUE);
  y += editH + pad;

  // 第三行：单元测试目标列表（configure 成功后刷新）
  MoveWindow(g_lbl_test, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_test_target, pad + lblW + 4, y, w - pad * 2 - lblW - 4, 240, TRUE);
  y += editH + pad;

  // 第四行：日志 + 可拖动分隔条 + 底部操作按钮
  g_log_left_cached = pad;
  g_log_width_cached = w - pad * 2;
  g_log_panel_top_cached = y;
  g_log_panel_h_cached = std::max(0, contentBottom - y - pad);
  auto layout_log_panel = [&](BOOL repaint) {
    const int splitterH = 6;
    const int btnH = 26;
    const int logBtnW = 88;
    const int gapAfterSplitter = 2;
    const int minLogH = 40;
    int maxSplitterY = g_log_panel_top_cached + g_log_panel_h_cached - splitterH - gapAfterSplitter - minLogH - pad - btnH - pad;
    if (maxSplitterY < g_log_panel_top_cached)
      maxSplitterY = g_log_panel_top_cached;
    int splitterY = g_log_panel_top_cached + static_cast<int>(g_log_top_ratio * g_log_panel_h_cached);
    if (splitterY < g_log_panel_top_cached)
      splitterY = g_log_panel_top_cached;
    if (splitterY > maxSplitterY)
      splitterY = maxSplitterY;
    g_log_splitter_y_cached = splitterY;
    g_log_top_ratio = g_log_panel_h_cached > 0
                          ? static_cast<double>(splitterY - g_log_panel_top_cached) / static_cast<double>(g_log_panel_h_cached)
                          : g_log_top_ratio;
    g_log_top_cached = splitterY + splitterH + 2;
    int logH = g_log_panel_top_cached + g_log_panel_h_cached - g_log_top_cached - pad - btnH - pad;
    if (logH < minLogH)
      logH = minLogH;
    MoveWindow(g_log, g_log_left_cached, g_log_top_cached, g_log_width_cached, logH, repaint);
    MoveWindow(g_log_splitter, g_log_left_cached, splitterY, g_log_width_cached, splitterH, repaint);
    g_log_splitter_rect = {g_log_left_cached, splitterY, g_log_left_cached + g_log_width_cached, splitterY + splitterH};
    const int btnY = g_log_top_cached + logH + pad;
    MoveWindow(g_log_copy, g_log_left_cached, btnY, logBtnW, btnH, repaint);
    MoveWindow(g_log_clear, g_log_left_cached + logBtnW + 8, btnY, logBtnW, btnH, repaint);
  };
  layout_log_panel(TRUE);
}

std::wstring OptionGroupName(const std::wstring& name) {
  const auto p = name.find(L'_');
  if (p == std::wstring::npos || p == 0)
    return name.empty() ? L"OTHER" : name;
  return name.substr(0, p);
}

std::vector<std::wstring> SplitChoices(const std::wstring& choices) {
  std::vector<std::wstring> out;
  std::wstring cur;
  for (wchar_t ch : choices) {
    if (ch == L'|') {
      TrimInPlace(cur);
      if (!cur.empty())
        out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  TrimInPlace(cur);
  if (!cur.empty())
    out.push_back(cur);
  return out;
}

const OptionRow* FindOptionByName(const std::wstring& name) {
  for (const auto& o : g_options) {
    if (o.name == name)
      return &o;
  }
  return nullptr;
}

bool ChoiceContainsValue(const OptionRow& opt, const std::wstring& value) {
  const auto choices = SplitChoices(opt.choices);
  for (const auto& c : choices) {
    if (_wcsicmp(c.c_str(), value.c_str()) == 0)
      return true;
  }
  return false;
}

bool SoftValidateOptionValue(const OptionRow& opt, const std::wstring& value) {
  bool warned = false;
  if (!opt.choices.empty() && !ChoiceContainsValue(opt, value)) {
    AppendLog(L"[警告] " + opt.name + L" 的值 \"" + value + L"\" 不在推荐候选中，已按自定义值保存。\r\n");
    SetStatus(L"Warning: custom option value used");
    warned = true;
  }

  if (opt.name == L"UP_CMAKE_GENERATOR") {
    const OptionRow* build = FindOptionByName(L"UP_TARGET_BUILD_SYSTEM");
    if (build && _wcsicmp(build->value.c_str(), L"cmake") != 0) {
      AppendLog(L"[警告] 当前 UP_TARGET_BUILD_SYSTEM 不是 cmake，UP_CMAKE_GENERATOR 可能不会生效。\r\n");
      SetStatus(L"Warning: generator may be ignored");
      warned = true;
    }
  }
  return warned;
}

void HandleOptionContextAction(HWND hwnd, int action_id) {
  if (g_selected_option_idx < 0 || g_selected_option_idx >= static_cast<int>(g_options.size()))
    return;
  auto& opt = g_options[static_cast<size_t>(g_selected_option_idx)];
  if (action_id == IDM_OPT_COPY_KEY) {
    if (CopyTextToClipboard(hwnd, opt.name))
      SetStatus(L"Copied option name");
    else
      SetStatus(L"Copy failed");
    return;
  }
  if (action_id == IDM_OPT_COPY_KEY_VALUE) {
    const std::wstring kv = opt.name + L"=" + opt.value;
    if (CopyTextToClipboard(hwnd, kv))
      SetStatus(L"Copied option key=value");
    else
      SetStatus(L"Copy failed");
    return;
  }
  if (action_id == IDM_OPT_RESET_DEFAULT) {
    for (const auto& d : g_default_options) {
      if (d.name == opt.name) {
        opt.value = d.value;
        RebuildOptionsListView();
        SetStatus(L"Option reset to default");
        break;
      }
    }
  }
}

void SyncOptionEditorFromSelection() {
  if (!g_opt_pick || !g_opt_custom)
    return;
  SendMessageW(g_opt_pick, CB_RESETCONTENT, 0, 0);
  SetWindowTextW(g_opt_custom, L"");
  if (g_selected_option_idx < 0 || g_selected_option_idx >= static_cast<int>(g_options.size()))
    return;
  const auto& o = g_options[static_cast<size_t>(g_selected_option_idx)];
  const auto choices = SplitChoices(o.choices);
  int set_idx = -1;
  for (size_t i = 0; i < choices.size(); ++i) {
    const int idx = static_cast<int>(SendMessageW(g_opt_pick, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choices[i].c_str())));
    if (_wcsicmp(choices[i].c_str(), o.value.c_str()) == 0)
      set_idx = idx;
  }
  if (set_idx >= 0)
    SendMessageW(g_opt_pick, CB_SETCURSEL, static_cast<WPARAM>(set_idx), 0);
  SetWindowTextW(g_opt_custom, o.value.c_str());
}

void RebuildOptionsListView() {
  if (!g_vars)
    return;
  SendMessageW(g_vars, LVM_DELETEALLITEMS, 0, 0);
  g_row_to_option_idx.clear();
  ListView_EnableGroupView(g_vars, FALSE);

  std::wstring cwd;
  if (g_path)
    GetEditText(g_path, cwd);
  if (cwd.empty()) {
    g_selected_option_idx = -1;
    if (g_vars_tree)
      TreeView_DeleteAllItems(g_vars_tree);
    SyncOptionEditorFromSelection();
    return;
  }

  int row = 0;

  auto add_option_row = [&](int opt_idx) {
    const auto& o = g_options[static_cast<size_t>(opt_idx)];
    LVITEMW it{};
    it.mask = LVIF_TEXT;
    it.iItem = row;
    it.iSubItem = 0;
    it.pszText = const_cast<LPWSTR>(o.name.c_str());
    SendMessageW(g_vars, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&it));
    LVITEMW v{};
    v.iSubItem = 1;
    v.pszText = const_cast<LPWSTR>(o.value.c_str());
    SendMessageW(g_vars, LVM_SETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&v));
    LVITEMW c{};
    c.iSubItem = 2;
    c.pszText = const_cast<LPWSTR>(o.choices.c_str());
    SendMessageW(g_vars, LVM_SETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&c));
    g_row_to_option_idx.push_back(opt_idx);
    ++row;
  };

  for (int i = 0; i < static_cast<int>(g_options.size()); ++i)
    add_option_row(i);
  int sel_row = -1;
  for (int r = 0; r < static_cast<int>(g_row_to_option_idx.size()); ++r) {
    if (g_row_to_option_idx[static_cast<size_t>(r)] >= 0) {
      if (g_selected_option_idx < 0)
        g_selected_option_idx = g_row_to_option_idx[static_cast<size_t>(r)];
      if (g_row_to_option_idx[static_cast<size_t>(r)] == g_selected_option_idx) {
        sel_row = r;
        break;
      }
    }
  }
  if (sel_row >= 0)
    ListView_SetItemState(g_vars, sel_row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
  RebuildOptionsTreeView();
  SyncOptionEditorFromSelection();
}

void RebuildOptionsTreeView() {
  if (!g_vars_tree)
    return;
  TreeView_DeleteAllItems(g_vars_tree);
  std::map<std::wstring, HTREEITEM> groups;
  HTREEITEM selected{};
  for (int i = 0; i < static_cast<int>(g_options.size()); ++i) {
    const auto& o = g_options[static_cast<size_t>(i)];
    const std::wstring gname = OptionGroupName(o.name);
    HTREEITEM parent{};
    auto it = groups.find(gname);
    if (it == groups.end()) {
      TVINSERTSTRUCTW ins{};
      ins.hParent = TVI_ROOT;
      ins.hInsertAfter = TVI_LAST;
      ins.item.mask = TVIF_TEXT;
      ins.item.pszText = const_cast<LPWSTR>(gname.c_str());
      parent = reinterpret_cast<HTREEITEM>(SendMessageW(g_vars_tree, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ins)));
      groups.emplace(gname, parent);
    } else {
      parent = it->second;
    }

    std::wstring label = o.name + L" = " + o.value;
    TVINSERTSTRUCTW child{};
    child.hParent = parent;
    child.hInsertAfter = TVI_LAST;
    child.item.mask = TVIF_TEXT | TVIF_PARAM;
    child.item.pszText = const_cast<LPWSTR>(label.c_str());
    child.item.lParam = static_cast<LPARAM>(i);
    HTREEITEM h =
        reinterpret_cast<HTREEITEM>(SendMessageW(g_vars_tree, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&child)));
    if (i == g_selected_option_idx)
      selected = h;
  }

  HTREEITEM root = TreeView_GetRoot(g_vars_tree);
  while (root) {
    TreeView_Expand(g_vars_tree, root, TVE_EXPAND);
    root = TreeView_GetNextSibling(g_vars_tree, root);
  }
  if (selected)
    TreeView_SelectItem(g_vars_tree, selected);
}

void InitVarsList(HWND lv) {
  ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

  LVCOLUMNW col{};
  col.mask = LVCF_TEXT | LVCF_WIDTH;
  col.cx = 230;
  wchar_t c0[] = L"Option";
  col.pszText = c0;
  SendMessageW(lv, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
  col.cx = 120;
  wchar_t c1[] = L"Value";
  col.pszText = c1;
  SendMessageW(lv, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
  col.cx = 360;
  wchar_t c2[] = L"Choices";
  col.pszText = c2;
  SendMessageW(lv, LVM_INSERTCOLUMNW, 2, reinterpret_cast<LPARAM>(&col));

  RebuildOptionsListView();
}

void CreateMainMenu(HWND hwnd) {
  HMENU bar = CreateMenu();
  HMENU file = CreateMenu();
  AppendMenuW(file, MF_STRING, IDM_EXIT, L"退出(&X)");
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(&F)");

  HMENU tools = CreateMenu();
  AppendMenuW(tools, MF_STRING, IDC_CONFIGURE, L"&configure");
  AppendMenuW(tools, MF_STRING, IDC_BUILD, L"&build");
  AppendMenuW(tools, MF_STRING, IDC_TEST, L"&test");
  AppendMenuW(tools, MF_STRING, IDC_PACK, L"&pack");
  AppendMenuW(tools, MF_STRING, IDC_RUN, L"&run…");
  AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(tools, MF_STRING, IDC_ENV_SETTINGS, L"编译环境设置...");
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"操作(&O)");

  HMENU help = CreateMenu();
  AppendMenuW(help, MF_STRING, IDM_ABOUT, L"关于(&A)…");
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(&H)");

  SetMenu(hwnd, bar);
}

void CreateToolbarButtons(HWND tb) {
  SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
  SendMessageW(tb, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
  SendMessageW(tb, TB_SETSTYLE, 0, TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN);

  // 无图像列表时 I_IMAGENONE + BTNS_AUTOSIZE 会导致按钮宽高为 0（完全不可见）。
  HIMAGELIST himl = reinterpret_cast<HIMAGELIST>(
      SendMessageW(tb, TB_LOADIMAGES, IDB_STD_SMALL_COLOR, reinterpret_cast<LPARAM>(HINST_COMMCTRL)));
  if (himl)
    SendMessageW(tb, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(himl));

  // 使用图标 + 全名单行文本，确保始终显示完整命令名。
  SendMessageW(tb, TB_SETMAXTEXTROWS, 1, 0);
  SendMessageW(tb, TB_SETBUTTONWIDTH, 0, MAKELONG(150, 280));

  struct Entry {
    int id;
    int std_img;  // STD_* 索引，见 CommCtrl.h（与 TB_LOADIMAGES IDB_STD_SMALL_COLOR 配套）
    const wchar_t* text;
  };
  const Entry entries[] = {
      {IDC_CONFIGURE, STD_PROPERTIES, L"configure"},
      {IDC_BUILD, STD_REPLACE, L"build"},
      {IDC_RUN, STD_FILENEW, L"run"},
      {IDC_TEST, STD_FIND, L"test"},
      {IDC_PACK, STD_PRINT, L"pack"},
      {IDC_ENV_SETTINGS, STD_HELP, L"env"},
  };

  std::vector<TBBUTTON> buttons;
  for (const auto& e : entries) {
    const int strIdx = static_cast<int>(SendMessageW(tb, TB_ADDSTRINGW, 0, reinterpret_cast<LPARAM>(e.text)));
    TBBUTTON b{};
    b.iBitmap = e.std_img;
    b.idCommand = e.id;
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    b.iString = strIdx;
    buttons.push_back(b);
  }
  SendMessageW(tb, TB_ADDBUTTONS, static_cast<WPARAM>(buttons.size()),
               reinterpret_cast<LPARAM>(buttons.data()));

  SendMessageW(tb, TB_AUTOSIZE, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      g_hwnd = hwnd;
      CreateMainMenu(hwnd);

      g_toolbar =
          CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                          WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST | CCS_NODIVIDER |
                              CCS_NOPARENTALIGN,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_TOOLBAR)),
                          GetModuleHandleW(nullptr), nullptr);
      CreateToolbarButtons(g_toolbar);

      g_path = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                               0, 100, 24, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_PATH)),
                               GetModuleHandleW(nullptr), nullptr);
      g_lbl_path = CreateWindowExW(0, L"STATIC", L"CWD", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_PATH)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_browse = CreateWindowExW(0, L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 72, 28, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_BROWSE)), GetModuleHandleW(nullptr),
                                 nullptr);
      g_lbl_scan = CreateWindowExW(0, L"STATIC", L"Scan Dir", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_SCAN)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_scan_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                    0, 0, 220, 104, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_LIST)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_scan_add = CreateWindowExW(0, L"BUTTON", L"+Add", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_ADD)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_scan_remove = CreateWindowExW(0, L"BUTTON", L"-Del", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_REMOVE)),
                                      GetModuleHandleW(nullptr), nullptr);
      g_scan_up = CreateWindowExW(0, L"BUTTON", L"Up", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_UP)),
                                  GetModuleHandleW(nullptr), nullptr);
      g_scan_down = CreateWindowExW(0, L"BUTTON", L"Down", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_DOWN)),
                                    GetModuleHandleW(nullptr), nullptr);

      g_vars = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER, 0, 0, 100, 80,
                               hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_VARS)), GetModuleHandleW(nullptr),
                               nullptr);
      g_vars_tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                                        TVS_SHOWSELALWAYS,
                                    0, 0, 100, 80, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_VARS_TREE)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_lbl_vars = CreateWindowExW(0, L"STATIC", L"Option", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_VARS)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_opt_group = CreateWindowExW(0, L"BUTTON", L"按前缀分组显示", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0,
                                    170, 24, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_GROUP)),
                                    GetModuleHandleW(nullptr), nullptr);
      SendMessageW(g_opt_group, BM_SETCHECK, BST_CHECKED, 0);
      g_opt_pick = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   0, 0, 120, 220, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_PICK)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_opt_custom = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                     0, 160, 26, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_CUSTOM)),
                                     GetModuleHandleW(nullptr), nullptr);
      g_opt_apply = CreateWindowExW(0, L"BUTTON", L"应用到选中项", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 120, 28, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_APPLY)),
                                    GetModuleHandleW(nullptr), nullptr);
      InitVarsList(g_vars);
      LoadGuiEnvSettings();

      g_lbl_extra = CreateWindowExW(0, L"STATIC", L"附加参数", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_EXTRA)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_extra = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                0, 100, 26, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_EXTRA)),
                                GetModuleHandleW(nullptr), nullptr);
      g_lbl_run = CreateWindowExW(0, L"STATIC", L"运行目标", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_RUN)),
                                  GetModuleHandleW(nullptr), nullptr);
      g_run_target =
          CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                          0, 100, 220, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_RUNTARGET)),
                          GetModuleHandleW(nullptr), nullptr);
      g_lbl_test = CreateWindowExW(0, L"STATIC", L"单元测试", WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_TEST)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_test_target =
          CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                          0, 100, 220, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_TESTTARGET)),
                          GetModuleHandleW(nullptr), nullptr);

      g_log = CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", L"",
          WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
          0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG)), GetModuleHandleW(nullptr),
          nullptr);
      g_log_splitter =
          CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_ETCHEDHORZ, 0, 0, 100, 6, hwnd,
                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG_SPLITTER)), GetModuleHandleW(nullptr), nullptr);
      g_log_copy =
          CreateWindowExW(0, L"BUTTON", L"复制内容", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 88, 26, hwnd,
                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG_COPY)), GetModuleHandleW(nullptr), nullptr);
      g_log_clear =
          CreateWindowExW(0, L"BUTTON", L"清理内容", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 88, 26, hwnd,
                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG_CLEAR)), GetModuleHandleW(nullptr), nullptr);

      g_status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                                 hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_STATUS)),
                                 GetModuleHandleW(nullptr), nullptr);
      SetStatus(L"Ready");

      {
        const HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(g_log, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_path, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_path, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_scan, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_scan_list, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_scan_add, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_scan_remove, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_scan_up, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_scan_down, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_extra, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_run_target, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_test_target, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_browse, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_vars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_vars_tree, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_vars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_opt_group, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_opt_pick, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_opt_custom, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_opt_apply, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_extra, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_run, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_test, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_toolbar, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);

        SendMessageW(g_log_copy, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_log_clear, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      }

      AppendLogRaw(
          L"up-gui：在「当前工作目录 (CWD)」下调用同目录的 up.exe。\r\n"
          L"「附加参数」可填例如 --scan test_projects（仓库根 + 多包扫描）。\r\n"
          L"工具栏与「操作」菜单均可触发 configure / build / test / pack / run。\r\n"
          L"运行目标与单元测试列表会在 configure 成功后自动刷新。\r\n"
          L"Option 列表支持按前缀分组显示。\r\n\r\n");
      // CWD 默认留空，等待用户明确选择/输入。
      LayoutChildren(hwnd);
      return 0;

    case WM_SIZE:
      LayoutChildren(hwnd);
      return 0;

    case WM_SETCURSOR: {
      POINT pt{};
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      if (pt.y >= g_log_splitter_rect.top && pt.y <= g_log_splitter_rect.bottom && pt.x >= g_log_splitter_rect.left &&
          pt.x <= g_log_splitter_rect.right) {
        SetCursor(LoadCursor(nullptr, IDC_SIZENS));
        return TRUE;
      }
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_LBUTTONDOWN: {
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);
      if (y >= g_log_splitter_rect.top && y <= g_log_splitter_rect.bottom && x >= g_log_splitter_rect.left &&
          x <= g_log_splitter_rect.right) {
        g_dragging_log_splitter = true;
        SetCapture(hwnd);
        return 0;
      }
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_MOUSEMOVE: {
      if (!g_dragging_log_splitter)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
      const int y = GET_Y_LPARAM(lParam);
      const int panelH = g_log_panel_h_cached;
      if (panelH <= 0)
        return 0;
      const int splitterH = 6;
      const int gapAfterSplitter = 2;
      const int btnH = 26;
      const int pad = 6;
      const int minLogH = 40;
      int splitterY = y;
      if (splitterY < g_log_panel_top_cached)
        splitterY = g_log_panel_top_cached;
      int maxSplitterY = g_log_panel_top_cached + panelH - splitterH - gapAfterSplitter - minLogH - pad - btnH - pad;
      if (maxSplitterY < g_log_panel_top_cached)
        maxSplitterY = g_log_panel_top_cached;
      if (splitterY > maxSplitterY)
        splitterY = maxSplitterY;
      g_log_splitter_y_cached = splitterY;
      g_log_top_ratio = static_cast<double>(splitterY - g_log_panel_top_cached) / static_cast<double>(panelH);
      LayoutChildren(hwnd);
      return 0;
    }

    case WM_LBUTTONUP:
      if (g_dragging_log_splitter) {
        g_dragging_log_splitter = false;
        ReleaseCapture();
        LayoutChildren(hwnd);
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return 0;
      }
      return DefWindowProcW(hwnd, msg, wParam, lParam);

    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 520;
      mmi->ptMinTrackSize.y = 620;
      return 0;
    }

    case WM_APPEND_LOG: {
      auto* s = reinterpret_cast<std::wstring*>(lParam);
      if (s) {
        AppendLogRaw(*s);
        delete s;
      }
      return 0;
    }

    case WM_PROCESS_DONE: {
      auto* pack = reinterpret_cast<DonePack*>(lParam);
      if (pack) {
        if (!pack->output.empty())
          AppendLogRaw(pack->output);
        wchar_t tail[160]{};
        swprintf_s(tail, L"\r\n[退出码 %lu]\r\n", static_cast<unsigned long>(pack->exit_code));
        AppendLogRaw(tail);
        wchar_t st[96]{};
        swprintf_s(st, L"Done (exit code %lu)", static_cast<unsigned long>(pack->exit_code));
        SetStatus(st);
        if (pack->exit_code == 0 && g_last_up_args.rfind(L"configure", 0) == 0) {
          RefreshRunTargetListFromPath();
          std::wstring cwd;
          GetEditText(g_path, cwd);
          // Keep user-edited Scan Dir list untouched after configure.
          LoadOptionsFromCache(cwd, false);
        }
        delete pack;
      }
      SetUiRunning(false);
      return 0;
    }

    case WM_COMMAND: {
      const int id = GET_WM_COMMAND_ID(wParam, lParam);
      if (g_running && id != IDM_EXIT)
        return 0;

      if (id == IDM_EXIT) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }
      if (id == IDM_ABOUT) {
        MessageBoxW(hwnd,
                    L"up-gui：调用同目录 up.exe 的本地外壳。\n布局：菜单栏、工具栏、当前工作目录(CWD)、Option、附加参数、运行目标列表、单元测试列表、日志、状态栏。",
                    L"关于 up-gui", MB_ICONINFORMATION | MB_OK);
        return 0;
      }
      if (id == IDC_ENV_SETTINGS) {
        if (ShowEnvSettingsDialog(hwnd))
          SetStatus(L"Environment settings updated");
        else
          SetStatus(L"Environment settings unchanged");
        return 0;
      }
      if (id == IDC_LOG_COPY) {
        const int len = GetWindowTextLengthW(g_log);
        std::wstring text;
        if (len > 0) {
          text.resize(static_cast<size_t>(len) + 1);
          GetWindowTextW(g_log, text.data(), len + 1);
          text.resize(static_cast<size_t>(len));
        }
        if (CopyTextToClipboard(hwnd, text))
          SetStatus(L"Log copied");
        else
          SetStatus(L"Copy failed");
        return 0;
      }
      if (id == IDC_LOG_CLEAR) {
        SetWindowTextW(g_log, L"");
        SetStatus(L"Log cleared");
        return 0;
      }

      if (id == IDC_BROWSE) {
        std::wstring folder;
        if (PickFolder(hwnd, folder)) {
          SetWindowTextW(g_path, folder.c_str());
          ResetScanDirsForCwd(folder);
          SendMessageW(g_run_target, CB_RESETCONTENT, 0, 0);
          SendMessageW(g_test_target, CB_RESETCONTENT, 0, 0);
          LoadOptionsFromCache(folder, false);
        }
        return 0;
      }
      if (id == IDC_SCAN_ADD) {
        std::wstring folder;
        if (PickFolder(hwnd, folder)) {
          const int exists = static_cast<int>(SendMessageW(g_scan_list, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                                                           reinterpret_cast<LPARAM>(folder.c_str())));
          if (exists == LB_ERR)
            SendMessageW(g_scan_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(folder.c_str()));
        }
        return 0;
      }
      if (id == IDC_SCAN_REMOVE) {
        const int idx = static_cast<int>(SendMessageW(g_scan_list, LB_GETCURSEL, 0, 0));
        if (idx != LB_ERR)
          SendMessageW(g_scan_list, LB_DELETESTRING, static_cast<WPARAM>(idx), 0);
        return 0;
      }
      if (id == IDC_SCAN_UP || id == IDC_SCAN_DOWN) {
        const int idx = static_cast<int>(SendMessageW(g_scan_list, LB_GETCURSEL, 0, 0));
        if (idx == LB_ERR)
          return 0;
        const int cnt = static_cast<int>(SendMessageW(g_scan_list, LB_GETCOUNT, 0, 0));
        const int dst = (id == IDC_SCAN_UP) ? idx - 1 : idx + 1;
        if (dst < 0 || dst >= cnt)
          return 0;
        const int n = static_cast<int>(SendMessageW(g_scan_list, LB_GETTEXTLEN, static_cast<WPARAM>(idx), 0));
        if (n <= 0)
          return 0;
        std::wstring s(static_cast<size_t>(n), L'\0');
        SendMessageW(g_scan_list, LB_GETTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(s.data()));
        SendMessageW(g_scan_list, LB_DELETESTRING, static_cast<WPARAM>(idx), 0);
        SendMessageW(g_scan_list, LB_INSERTSTRING, static_cast<WPARAM>(dst), reinterpret_cast<LPARAM>(s.c_str()));
        SendMessageW(g_scan_list, LB_SETCURSEL, static_cast<WPARAM>(dst), 0);
        return 0;
      }
      if (id == IDC_OPT_GROUP) {
        RebuildOptionsListView();
        LayoutChildren(hwnd);
        SetStatus(L"Option grouping updated");
        return 0;
      }
      if (id == IDC_OPT_PICK && HIWORD(wParam) == CBN_SELCHANGE) {
        const int idx = static_cast<int>(SendMessageW(g_opt_pick, CB_GETCURSEL, 0, 0));
        if (idx != CB_ERR) {
          const int n = static_cast<int>(SendMessageW(g_opt_pick, CB_GETLBTEXTLEN, static_cast<WPARAM>(idx), 0));
          if (n > 0) {
            std::wstring val(static_cast<size_t>(n), L'\0');
            SendMessageW(g_opt_pick, CB_GETLBTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(val.data()));
            SetWindowTextW(g_opt_custom, val.c_str());
          }
        }
        return 0;
      }
      if (id == IDC_OPT_APPLY) {
        if (g_selected_option_idx < 0 || g_selected_option_idx >= static_cast<int>(g_options.size())) {
          AppendLog(L"[错误] 请先在 Option 表中选择一个变量。\r\n");
          return 0;
        }
        std::wstring newv;
        GetEditText(g_opt_custom, newv);
        TrimInPlace(newv);
        if (newv.empty()) {
          const int idx = static_cast<int>(SendMessageW(g_opt_pick, CB_GETCURSEL, 0, 0));
          if (idx != CB_ERR) {
            const int n = static_cast<int>(SendMessageW(g_opt_pick, CB_GETLBTEXTLEN, static_cast<WPARAM>(idx), 0));
            if (n > 0) {
              newv.assign(static_cast<size_t>(n), L'\0');
              SendMessageW(g_opt_pick, CB_GETLBTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(newv.data()));
            }
          }
        }
        if (newv.empty()) {
          AppendLog(L"[错误] 请输入值或从候选中选择。\r\n");
          return 0;
        }
        g_options[static_cast<size_t>(g_selected_option_idx)].value = newv;
        const bool warned =
            SoftValidateOptionValue(g_options[static_cast<size_t>(g_selected_option_idx)], newv);
        RebuildOptionsListView();
        if (!warned)
          SetStatus(L"Option value updated");
        return 0;
      }
      if (id == IDM_OPT_COPY_KEY || id == IDM_OPT_COPY_KEY_VALUE || id == IDM_OPT_RESET_DEFAULT) {
        HandleOptionContextAction(hwnd, id);
        return 0;
      }
      if (id == IDC_CONFIGURE) {
        RunUpAsync(L"configure");
        return 0;
      }
      if (id == IDC_BUILD) {
        RunUpAsync(L"build");
        return 0;
      }
      if (id == IDC_TEST) {
        std::wstring testarg = L"test";
        const int idx = static_cast<int>(SendMessageW(g_test_target, CB_GETCURSEL, 0, 0));
        if (idx != CB_ERR) {
          const int n = static_cast<int>(SendMessageW(g_test_target, CB_GETLBTEXTLEN, static_cast<WPARAM>(idx), 0));
          if (n > 0) {
            std::wstring tgt(static_cast<size_t>(n), L'\0');
            SendMessageW(g_test_target, CB_GETLBTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(tgt.data()));
            testarg += L" ";
            if (tgt.find(L' ') != std::wstring::npos)
              testarg += L"\"" + tgt + L"\"";
            else
              testarg += tgt;
          }
        }
        RunUpAsync(testarg);
        return 0;
      }
      if (id == IDC_PACK) {
        RunUpAsync(L"pack");
        return 0;
      }
      if (id == IDC_RUN) {
        const int idx = static_cast<int>(SendMessageW(g_run_target, CB_GETCURSEL, 0, 0));
        if (idx == CB_ERR) {
          AppendLog(L"[错误] 运行目标列表为空，请先 configure。\r\n");
          return 0;
        }
        const int n = static_cast<int>(SendMessageW(g_run_target, CB_GETLBTEXTLEN, static_cast<WPARAM>(idx), 0));
        if (n <= 0) {
          AppendLog(L"[错误] 无法读取运行目标。\r\n");
          return 0;
        }
        std::wstring tgt(static_cast<size_t>(n), L'\0');
        SendMessageW(g_run_target, CB_GETLBTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(tgt.data()));
        std::wstring runarg = L"run ";
        if (tgt.find(L' ') != std::wstring::npos)
          runarg += L"\"" + tgt + L"\"";
        else
          runarg += tgt;
        RunUpAsync(runarg);
        return 0;
      }
      return 0;
    }

    case WM_CONTEXTMENU: {
      HWND src = reinterpret_cast<HWND>(wParam);
      if (src != g_vars_tree)
        return 0;
      POINT pt{};
      pt.x = GET_X_LPARAM(lParam);
      pt.y = GET_Y_LPARAM(lParam);
      if (pt.x == -1 && pt.y == -1) {
        RECT rc{};
        GetWindowRect(g_vars_tree, &rc);
        pt.x = rc.left + 12;
        pt.y = rc.top + 12;
      }
      POINT local = pt;
      ScreenToClient(g_vars_tree, &local);
      TVHITTESTINFO hti{};
      hti.pt = local;
      TreeView_HitTest(g_vars_tree, &hti);
      if (!hti.hItem)
        return 0;
      TreeView_SelectItem(g_vars_tree, hti.hItem);

      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu, MF_STRING, IDM_OPT_COPY_KEY, L"复制变量名");
      AppendMenuW(menu, MF_STRING, IDM_OPT_COPY_KEY_VALUE, L"复制 KEY=VALUE");
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      AppendMenuW(menu, MF_STRING, IDM_OPT_RESET_DEFAULT, L"重置默认值");
      TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
      DestroyMenu(menu);
      return 0;
    }

    case WM_NOTIFY: {
      const auto* hdr = reinterpret_cast<NMHDR*>(lParam);
      if (!hdr)
        return 0;
      if (hdr->idFrom == IDC_VARS && hdr->code == LVN_ITEMCHANGED) {
        const int row = ListView_GetNextItem(g_vars, -1, LVNI_SELECTED);
        if (row < 0 || row >= static_cast<int>(g_row_to_option_idx.size())) {
          g_selected_option_idx = -1;
          SyncOptionEditorFromSelection();
          return 0;
        }
        g_selected_option_idx = g_row_to_option_idx[static_cast<size_t>(row)];
        SyncOptionEditorFromSelection();
        return 0;
      }
      if (hdr->idFrom == IDC_VARS_TREE && hdr->code == TVN_SELCHANGEDW) {
        const auto* n = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        if (n && n->itemNew.hItem) {
          TVITEMW item{};
          item.hItem = n->itemNew.hItem;
          item.mask = TVIF_PARAM;
          if (TreeView_GetItem(g_vars_tree, &item) && item.lParam >= 0) {
            g_selected_option_idx = static_cast<int>(item.lParam);
            SyncOptionEditorFromSelection();
          }
        }
        return 0;
      }
      return 0;
    }

    case WM_CLOSE:
      if (g_running) {
        if (MessageBoxW(hwnd, L"up 仍在运行，确定要关闭窗口吗？", L"up-gui", MB_YESNO | MB_ICONQUESTION) != IDYES)
          return 0;
      }
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
  HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  (void)hrCo;

  INITCOMMONCONTROLSEX icc{};
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES;
  InitCommonControlsEx(&icc);

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hi;
  wc.lpszClassName = kClassName;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = nullptr;
  RegisterClassW(&wc);

  RECT want{0, 0, 860, 680};
  AdjustWindowRect(&want, WS_OVERLAPPEDWINDOW, TRUE);
  const int win_w = want.right - want.left;
  const int win_h = want.bottom - want.top;
  RECT work{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const int x = work.left + ((work.right - work.left) - win_w) / 2;
  const int y = work.top + ((work.bottom - work.top) - win_h) / 2;
  HWND hwnd = CreateWindowExW(0, kClassName, kTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, x, y, win_w,
                              win_h, nullptr, nullptr, hi, nullptr);
  if (!hwnd)
    return 1;

  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  CoUninitialize();
  return static_cast<int>(msg.wParam);
}
