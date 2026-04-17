// up-gui：本地 Win32 外壳，仅通过命令行调用与 up-gui.exe 同目录的 up.exe；不链接、不包含 src 下业务代码（见 DESIGN.md / mindmap）。

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
constexpr int IDC_PROJECT = 139;
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
constexpr int IDC_LBL_BUILD_DIR = 135;
constexpr int IDC_BUILD_DIR = 136;
constexpr int IDC_LBL_INSTALL_DIR = 137;
constexpr int IDC_INSTALL_DIR = 138;
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
constexpr int IDC_ENV_SETTINGS = 131;
constexpr int IDC_LOG_SPLITTER = 132;
constexpr int IDC_LOG_COPY = 133;
constexpr int IDC_LOG_CLEAR = 134;
constexpr int IDM_OPT_COPY_KEY = 2101;
constexpr int IDM_OPT_COPY_KEY_VALUE = 2102;
constexpr int IDM_OPT_RESET_DEFAULT = 2103;

constexpr int IDM_EXIT = 1000;
constexpr int IDM_ABOUT = 1001;
constexpr int IDM_UP_HELP = 1002;
constexpr int IDM_LANG_ZH = 1005;
constexpr int IDM_LANG_EN = 1006;

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

bool g_ui_lang_zh = true;

bool SystemDefaultUiLangIsChinese() {
  const LANGID lid = static_cast<LANGID>(GetUserDefaultUILanguage());
  return PRIMARYLANGID(lid) == LANG_CHINESE;
}

const wchar_t* T(const wchar_t* zh, const wchar_t* en) {
  return g_ui_lang_zh ? zh : en;
}

const wchar_t* EmptyFieldLabel() {
  return T(L"(空)", L"(empty)");
}

const wchar_t* HitScanSourceLabel() {
  return T(L"扫描", L"scan");
}

HWND g_hwnd{};
HWND g_toolbar{};
HWND g_status{};
HWND g_path{};
HWND g_browse{};
HWND g_lbl_path{};
HWND g_vars{};
HWND g_lbl_vars{};
HWND g_lbl_build_dir{};
HWND g_build_dir{};
HWND g_lbl_install_dir{};
HWND g_install_dir{};
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
HWND g_env_settings_dialog_hwnd{};
HWND g_configure_option_dialog_hwnd{};

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
  std::wstring vcvars64_path;
  std::wstring vcvars32_path;
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
struct GuiBrowseHistory {
  std::wstring cwd_folder;
  std::wstring scan_folder;
  std::wstring android_sdk_folder;
  std::wstring android_ndk_folder;
  std::wstring emsdk_folder;
};
GuiBrowseHistory g_browse_history;

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

// 路径统一为 POSIX 风格「/」分隔（generic 形式），便于与 Linux 习惯一致、配置文件易读；Windows API 多数仍接受此种路径。
std::wstring PathToPortableSlashes(std::wstring w) {
  TrimInPlace(w);
  if (w.empty())
    return w;
  std::filesystem::path p(w);
  p = p.lexically_normal();
  return p.generic_wstring();
}

// 配置文件里所有「路径形态」的项统一为正斜杠 /（含从环境/自动探测得到的值）。
void NormalizeGuiSettingsStoredPaths() {
  g_env_settings.android_sdk_path = PathToPortableSlashes(g_env_settings.android_sdk_path);
  g_env_settings.android_ndk_path = PathToPortableSlashes(g_env_settings.android_ndk_path);
  g_env_settings.emsdk_path = PathToPortableSlashes(g_env_settings.emsdk_path);
  g_env_settings.selected_vcvars = PathToPortableSlashes(g_env_settings.selected_vcvars);
  g_env_settings.vcvars64_path = PathToPortableSlashes(g_env_settings.vcvars64_path);
  g_env_settings.vcvars32_path = PathToPortableSlashes(g_env_settings.vcvars32_path);
  g_browse_history.cwd_folder = PathToPortableSlashes(g_browse_history.cwd_folder);
  g_browse_history.scan_folder = PathToPortableSlashes(g_browse_history.scan_folder);
  g_browse_history.android_sdk_folder = PathToPortableSlashes(g_browse_history.android_sdk_folder);
  g_browse_history.android_ndk_folder = PathToPortableSlashes(g_browse_history.android_ndk_folder);
  g_browse_history.emsdk_folder = PathToPortableSlashes(g_browse_history.emsdk_folder);
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
  return PathToPortableSlashes(p.substr(0, pos));
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
      return PathToPortableSlashes(std::move(comspec));
  }
  std::wstring sysroot = GetEnvVarW(L"SystemRoot");
  if (!sysroot.empty()) {
    std::filesystem::path p = std::filesystem::path(sysroot) / "System32" / "cmd.exe";
    std::error_code ec;
    if (std::filesystem::exists(p, ec))
      return p.lexically_normal().generic_wstring();
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
  return (ec ? p : abs).lexically_normal().generic_wstring();
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
  // Preserve user-selected SDK path as a first-class candidate.
  if (!s.android_sdk_path.empty())
    AddUnique(s.android_sdk_hits, NormalizePath(s.android_sdk_path));
  if (!sdk_env1.empty())
    AddUnique(s.android_sdk_hits, NormalizePath(sdk_env1));
  if (!sdk_env2.empty())
    AddUnique(s.android_sdk_hits, NormalizePath(sdk_env2));
  AddUniqueDirPath(s.android_sdk_hits, std::filesystem::path(GetEnvVarW(L"LOCALAPPDATA")) / "Android" / "Sdk");
  AddUniqueDirPath(s.android_sdk_hits, std::filesystem::path(GetEnvVarW(L"USERPROFILE")) / "Android" / "Sdk");

  const auto ndk_env1 = GetEnvVarW(L"ANDROID_NDK_ROOT");
  const auto ndk_env2 = GetEnvVarW(L"ANDROID_NDK_HOME");
  if (!s.android_ndk_path.empty())
    AddUnique(s.android_ndk_hits, NormalizePath(s.android_ndk_path));
  if (!ndk_env1.empty())
    AddUnique(s.android_ndk_hits, NormalizePath(ndk_env1));
  if (!ndk_env2.empty())
    AddUnique(s.android_ndk_hits, NormalizePath(ndk_env2));
  auto collect_ndk_from_sdk = [&](const std::wstring& sdk) {
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
  };
  if (!s.android_sdk_path.empty())
    collect_ndk_from_sdk(s.android_sdk_path);
  for (const auto& sdk : s.android_sdk_hits) {
    collect_ndk_from_sdk(sdk);
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

  if (s.android_sdk_path.empty())
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

void RefreshStoredVcvars32And64Paths(GuiEnvSettings& s);

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
  RefreshStoredVcvars32And64Paths(s);
}

// 在配置中同时记住 vcvars64 / vcvars32 的完整路径（同目录成对 + 命中列表补全）。
void RefreshStoredVcvars32And64Paths(GuiEnvSettings& s) {
  std::error_code ec;
  const std::filesystem::path sel(s.selected_vcvars);
  const std::filesystem::path sel_dir = sel.has_parent_path() ? sel.parent_path() : std::filesystem::path{};

  auto fill_pair_from_dir = [&](const std::filesystem::path& dir) {
    if (dir.empty())
      return;
    const auto p64 = dir / L"vcvars64.bat";
    const auto p32 = dir / L"vcvars32.bat";
    if (s.vcvars64_path.empty() && std::filesystem::exists(p64, ec)) {
      ec.clear();
      s.vcvars64_path = PathToPortableSlashes(p64.wstring());
    }
    if (s.vcvars32_path.empty() && std::filesystem::exists(p32, ec)) {
      ec.clear();
      s.vcvars32_path = PathToPortableSlashes(p32.wstring());
    }
  };

  if (!s.selected_vcvars.empty())
    fill_pair_from_dir(sel_dir);

  for (const auto& h : s.vcvars_hits) {
    if (h.name != L"vcvars64" && h.name != L"vcvars32")
      continue;
    const std::filesystem::path hp(h.path);
    if (!sel_dir.empty() && hp.has_parent_path() && hp.parent_path() == sel_dir) {
      if (h.name == L"vcvars64" && s.vcvars64_path.empty())
        s.vcvars64_path = PathToPortableSlashes(h.path);
      if (h.name == L"vcvars32" && s.vcvars32_path.empty())
        s.vcvars32_path = PathToPortableSlashes(h.path);
    }
  }
  for (const auto& h : s.vcvars_hits) {
    if (h.name == L"vcvars64" && s.vcvars64_path.empty())
      s.vcvars64_path = PathToPortableSlashes(h.path);
    else if (h.name == L"vcvars32" && s.vcvars32_path.empty())
      s.vcvars32_path = PathToPortableSlashes(h.path);
  }
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
      *note = g_ui_lang_zh ? L"vcvars 未选择" : L"vcvars not selected";
    return 0;
  }
  std::error_code ec;
  if (!std::filesystem::exists(std::filesystem::path(vcvars), ec)) {
    if (note)
      *note = g_ui_lang_zh ? L"vcvars 路径不存在" : L"vcvars path does not exist";
    return 0;
  }
  std::wstring output;
  DWORD exit_code = 0;
  const std::wstring cmd_exe = CmdExePath();
  std::wstring cmd = L"/d /c \"\"" + vcvars + L"\" >nul && where cl\"";
  if (!RunProcessCapture(cmd_exe, cmd, L"", output, exit_code)) {
    if (note)
      *note = g_ui_lang_zh ? L"where cl 执行失败（cmd 启动失败）" : L"where cl failed (could not start cmd)";
    return 0;
  }
  if (raw_out)
    *raw_out = output;
  if (exit_code != 0) {
    if (note)
      *note = g_ui_lang_zh ? L"where cl 未命中" : L"where cl: no match";
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
  if (note) {
    if (hits > 0)
      *note = (g_ui_lang_zh ? L"where cl 命中 " : L"where cl hits ") + std::to_wstring(hits);
    else
      *note = g_ui_lang_zh ? L"where cl 未命中" : L"where cl: no match";
  }
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
  g_ui_lang_zh = SystemDefaultUiLangIsChinese();
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
    else if (k == "local.vcvars64" && !v.empty())
      g_env_settings.vcvars64_path = v;
    else if (k == "local.vcvars32" && !v.empty())
      g_env_settings.vcvars32_path = v;
    else if (k == "browse.cwd")
      g_browse_history.cwd_folder = v;
    else if (k == "browse.scan")
      g_browse_history.scan_folder = v;
    else if (k == "browse.android_sdk")
      g_browse_history.android_sdk_folder = v;
    else if (k == "browse.android_ndk")
      g_browse_history.android_ndk_folder = v;
    else if (k == "browse.emsdk")
      g_browse_history.emsdk_folder = v;
    else if (k == "gui.ui_lang") {
      if (_wcsicmp(v.c_str(), L"en") == 0)
        g_ui_lang_zh = false;
      else
        g_ui_lang_zh = true;
    }
  }
  DetectVcvarsHits(g_env_settings);
  NormalizeGuiSettingsStoredPaths();
}

void SaveGuiEnvSettings() {
  NormalizeGuiSettingsStoredPaths();
  std::ofstream f(GuiSettingsPath(), std::ios::binary | std::ios::trunc);
  if (!f)
    return;
  f << "local.build_system=" << WideToUtf8(g_env_settings.selected_build_system) << "\n";
  f << "local.compiler=" << WideToUtf8(g_env_settings.selected_compiler) << "\n";
  f << "android.sdk=" << WideToUtf8(g_env_settings.android_sdk_path) << "\n";
  f << "android.ndk=" << WideToUtf8(g_env_settings.android_ndk_path) << "\n";
  f << "emsdk.path=" << WideToUtf8(g_env_settings.emsdk_path) << "\n";
  f << "local.vcvars=" << WideToUtf8(g_env_settings.selected_vcvars) << "\n";
  f << "local.vcvars64=" << WideToUtf8(g_env_settings.vcvars64_path) << "\n";
  f << "local.vcvars32=" << WideToUtf8(g_env_settings.vcvars32_path) << "\n";
  f << "browse.cwd=" << WideToUtf8(g_browse_history.cwd_folder) << "\n";
  f << "browse.scan=" << WideToUtf8(g_browse_history.scan_folder) << "\n";
  f << "browse.android_sdk=" << WideToUtf8(g_browse_history.android_sdk_folder) << "\n";
  f << "browse.android_ndk=" << WideToUtf8(g_browse_history.android_ndk_folder) << "\n";
  f << "browse.emsdk=" << WideToUtf8(g_browse_history.emsdk_folder) << "\n";
  f << "gui.ui_lang=" << (g_ui_lang_zh ? "zh" : "en") << "\n";
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

bool PickFolder(HWND owner, const std::wstring& initial_dir, std::wstring& out);
void GetEditText(HWND ed, std::wstring& out);
bool QueryConfigureBuildDirNameFromUpExe(const std::wstring& up_exe, const std::wstring& cwd, std::wstring& out_arch_leaf,
                                         std::wstring& err_msg);

void EnsureListColumns(HWND lv) {
  if (ListView_GetColumnWidth(lv, 0) > 0)
    return;
  LVCOLUMNW col{};
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  col.cx = 100;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"工具", L"Tool")));
  SendMessageW(lv, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
  col.cx = 310;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"路径", L"Path")));
  SendMessageW(lv, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
  col.cx = 90;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"来源", L"Source")));
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
    const wchar_t* src = hits[i].from_path ? L"PATH" : HitScanSourceLabel();
    sub.iSubItem = 2;
    sub.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(src));
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
    const wchar_t* src = hits[i].from_path ? L"PATH" : HitScanSourceLabel();
    sub.iSubItem = 2;
    sub.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(src));
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
  if (g_ui_lang_zh) {
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
  ss << L"Build environment — auto-detect: build hits " << s.build_hits.size() << L", vcvars " << s.vcvars_hits.size()
     << L", compilers " << s.compiler_hits.size() << L", SDK " << s.android_sdk_hits.size() << L", NDK "
     << s.android_ndk_hits.size() << L", emsdk " << s.emsdk_hits.size();
  if (s.android_ndk_hits.empty())
    ss << L"; no Android NDK hits";
  if (s.emsdk_hits.empty())
    ss << L"; no emsdk hits";
  if (!s.selected_vcvars.empty())
    ss << L"; " << (s.vcvars_cl_note.empty() ? (L"where cl hits " + std::to_wstring(s.vcvars_cl_hits)) : s.vcvars_cl_note);
  return ss.str();
}

std::wstring BuildEnvDetectLogText(const GuiEnvSettings& s) {
  std::wstringstream ss;
  if (g_ui_lang_zh) {
    ss << L"[环境搜索] 自动搜索结果\r\n";
    ss << L"  - 选中建造系统: " << (s.selected_build_system.empty() ? EmptyFieldLabel() : s.selected_build_system) << L"\r\n";
    ss << L"  - 选中 vcvars: " << (s.selected_vcvars.empty() ? EmptyFieldLabel() : s.selected_vcvars) << L"\r\n";
    ss << L"  - 选中编译器: " << (s.selected_compiler.empty() ? EmptyFieldLabel() : s.selected_compiler) << L"\r\n";
    ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? EmptyFieldLabel() : s.android_sdk_path) << L"\r\n";
    ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? EmptyFieldLabel() : s.android_ndk_path) << L"\r\n";
    ss << L"  - emsdk: " << (s.emsdk_path.empty() ? EmptyFieldLabel() : s.emsdk_path) << L"\r\n";

    auto dump_hits = [&](const wchar_t* title, const std::vector<ToolHit>& hits) {
      ss << L"  * " << title << L" 命中 " << hits.size() << L" 条\r\n";
      for (const auto& h : hits) {
        ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
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

  ss << L"[env search] auto-detect results\r\n";
  ss << L"  - build system: " << (s.selected_build_system.empty() ? EmptyFieldLabel() : s.selected_build_system) << L"\r\n";
  ss << L"  - vcvars: " << (s.selected_vcvars.empty() ? EmptyFieldLabel() : s.selected_vcvars) << L"\r\n";
  ss << L"  - compiler: " << (s.selected_compiler.empty() ? EmptyFieldLabel() : s.selected_compiler) << L"\r\n";
  ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? EmptyFieldLabel() : s.android_sdk_path) << L"\r\n";
  ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? EmptyFieldLabel() : s.android_ndk_path) << L"\r\n";
  ss << L"  - emsdk: " << (s.emsdk_path.empty() ? EmptyFieldLabel() : s.emsdk_path) << L"\r\n";

  auto dump_hits = [&](const wchar_t* title, const std::vector<ToolHit>& hits) {
    ss << L"  * " << title << L" hits " << hits.size() << L"\r\n";
    for (const auto& h : hits) {
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
    }
  };
  auto dump_paths = [&](const wchar_t* title, const std::vector<std::wstring>& hits) {
    ss << L"  * " << title << L" hits " << hits.size() << L"\r\n";
    for (const auto& h : hits)
      ss << L"      - " << h << L"\r\n";
  };

  dump_hits(L"Build systems", s.build_hits);
  dump_hits(L"vcvars", s.vcvars_hits);
  dump_hits(L"Compilers", s.compiler_hits);
  dump_paths(L"Android SDK", s.android_sdk_hits);
  dump_paths(L"Android NDK", s.android_ndk_hits);
  dump_paths(L"emsdk", s.emsdk_hits);
  if (!s.vcvars_cl_note.empty())
    ss << L"  * vcvars where cl: " << s.vcvars_cl_note << L"\r\n";
  if (!s.vcvars_cl_output.empty())
    ss << L"  * where cl output:\r\n" << s.vcvars_cl_output << L"\r\n";
  return ss.str();
}

std::wstring BuildEnvDetectLogTextForTab(const GuiEnvSettings& s, int tab) {
  std::wstringstream ss;
  if (g_ui_lang_zh) {
    if (tab == 0) {
      ss << L"[环境搜索][本地环境]\r\n";
      ss << L"  - 选中建造系统: " << (s.selected_build_system.empty() ? EmptyFieldLabel() : s.selected_build_system) << L"\r\n";
      ss << L"  - 选中 vcvars: " << (s.selected_vcvars.empty() ? EmptyFieldLabel() : s.selected_vcvars) << L"\r\n";
      ss << L"  - 选中编译器: " << (s.selected_compiler.empty() ? EmptyFieldLabel() : s.selected_compiler) << L"\r\n";
      ss << L"  * 建造系统命中: " << s.build_hits.size() << L"\r\n";
      for (const auto& h : s.build_hits)
        ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
      ss << L"  * vcvars 命中: " << s.vcvars_hits.size() << L"\r\n";
      for (const auto& h : s.vcvars_hits)
        ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
      ss << L"  * 编译器命中: " << s.compiler_hits.size() << L"\r\n";
      for (const auto& h : s.compiler_hits)
        ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
      if (!s.vcvars_cl_note.empty())
        ss << L"  * where cl: " << s.vcvars_cl_note << L"\r\n";
    } else if (tab == 1) {
      ss << L"[环境搜索][Android 环境]\r\n";
      ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? EmptyFieldLabel() : s.android_sdk_path) << L"\r\n";
      ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? EmptyFieldLabel() : s.android_ndk_path) << L"\r\n";
      ss << L"  * SDK 命中: " << s.android_sdk_hits.size() << L"\r\n";
      for (const auto& p : s.android_sdk_hits)
        ss << L"      - " << p << L"\r\n";
      ss << L"  * NDK 命中: " << s.android_ndk_hits.size() << L"\r\n";
      for (const auto& p : s.android_ndk_hits)
        ss << L"      - " << p << L"\r\n";
    } else {
      ss << L"[环境搜索][emsdk 环境]\r\n";
      ss << L"  - emsdk: " << (s.emsdk_path.empty() ? EmptyFieldLabel() : s.emsdk_path) << L"\r\n";
      ss << L"  * emsdk 命中: " << s.emsdk_hits.size() << L"\r\n";
      for (const auto& p : s.emsdk_hits)
        ss << L"      - " << p << L"\r\n";
    }
    return ss.str();
  }

  if (tab == 0) {
    ss << L"[env search][local]\r\n";
    ss << L"  - build system: " << (s.selected_build_system.empty() ? EmptyFieldLabel() : s.selected_build_system) << L"\r\n";
    ss << L"  - vcvars: " << (s.selected_vcvars.empty() ? EmptyFieldLabel() : s.selected_vcvars) << L"\r\n";
    ss << L"  - compiler: " << (s.selected_compiler.empty() ? EmptyFieldLabel() : s.selected_compiler) << L"\r\n";
    ss << L"  * build system hits: " << s.build_hits.size() << L"\r\n";
    for (const auto& h : s.build_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
    ss << L"  * vcvars hits: " << s.vcvars_hits.size() << L"\r\n";
    for (const auto& h : s.vcvars_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
    ss << L"  * compiler hits: " << s.compiler_hits.size() << L"\r\n";
    for (const auto& h : s.compiler_hits)
      ss << L"      - [" << h.name << L"] " << h.path << L" (" << (h.from_path ? L"PATH" : HitScanSourceLabel()) << L")\r\n";
    if (!s.vcvars_cl_note.empty())
      ss << L"  * where cl: " << s.vcvars_cl_note << L"\r\n";
  } else if (tab == 1) {
    ss << L"[env search][Android]\r\n";
    ss << L"  - Android SDK: " << (s.android_sdk_path.empty() ? EmptyFieldLabel() : s.android_sdk_path) << L"\r\n";
    ss << L"  - Android NDK: " << (s.android_ndk_path.empty() ? EmptyFieldLabel() : s.android_ndk_path) << L"\r\n";
    ss << L"  * SDK hits: " << s.android_sdk_hits.size() << L"\r\n";
    for (const auto& p : s.android_sdk_hits)
      ss << L"      - " << p << L"\r\n";
    ss << L"  * NDK hits: " << s.android_ndk_hits.size() << L"\r\n";
    for (const auto& p : s.android_ndk_hits)
      ss << L"      - " << p << L"\r\n";
  } else {
    ss << L"[env search][emsdk]\r\n";
    ss << L"  - emsdk: " << (s.emsdk_path.empty() ? EmptyFieldLabel() : s.emsdk_path) << L"\r\n";
    ss << L"  * emsdk hits: " << s.emsdk_hits.size() << L"\r\n";
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
  RefreshStoredVcvars32And64Paths(st->work);
}

void PushEnvDialogValues(EnvDialogState* st) {
  st->suppress_list_notify = true;
  if (st->work.android_sdk_path.empty())
    st->work.android_sdk_path = PickPreferredPath(st->work.android_sdk_path, st->work.android_sdk_hits);
  if (st->work.android_ndk_path.empty())
    st->work.android_ndk_path = PickPreferredPath(st->work.android_ndk_path, st->work.android_ndk_hits);
  if (st->work.emsdk_path.empty())
    st->work.emsdk_path = PickPreferredPath(st->work.emsdk_path, st->work.emsdk_hits);
  FillToolHitsList(st->lv_local_build, st->work.build_hits, st->work.selected_build_system);
  FillToolHitsListByPath(st->lv_local_vcvars, st->work.vcvars_hits, st->work.selected_vcvars);
  FillToolHitsList(st->lv_local_compiler, st->work.compiler_hits, st->work.selected_compiler);
  SetWindowTextW(st->edt_android_sdk, st->work.android_sdk_path.c_str());
  SetWindowTextW(st->edt_android_ndk, st->work.android_ndk_path.c_str());
  SetWindowTextW(st->edt_emsdk, st->work.emsdk_path.c_str());
  st->suppress_list_notify = false;
}

void PushEnvAndroidSdkNdkEmsdkEditsOnly(EnvDialogState* st) {
  if (!st)
    return;
  SetWindowTextW(st->edt_android_sdk, st->work.android_sdk_path.c_str());
  SetWindowTextW(st->edt_android_ndk, st->work.android_ndk_path.c_str());
  SetWindowTextW(st->edt_emsdk, st->work.emsdk_path.c_str());
}

void EnvFinalizeAndroidEmsdkPathsInWork(GuiEnvSettings& w, bool rescan_hits) {
  w.android_sdk_path = PathToPortableSlashes(w.android_sdk_path);
  w.android_ndk_path = PathToPortableSlashes(w.android_ndk_path);
  w.emsdk_path = PathToPortableSlashes(w.emsdk_path);
  if (rescan_hits)
    DetectAndroidAndEmsdkHits(w);
  w.android_sdk_path = PathToPortableSlashes(w.android_sdk_path);
  w.android_ndk_path = PathToPortableSlashes(w.android_ndk_path);
  w.emsdk_path = PathToPortableSlashes(w.emsdk_path);
}

// 将对话框里的 SDK/NDK/emsdk 路径写回 g_env_settings 并保存；此前「浏览」后只更新了 st->work，SaveGuiEnvSettings 仍写旧的全局值，导致 android.ndk 等不落盘。
void EnvCommitAndroidEmsdkPathsToGlobal(EnvDialogState* st, HWND dlg, bool rescan_hits) {
  if (!st)
    return;
  PullEnvDialogValues(st);
  EnvFinalizeAndroidEmsdkPathsInWork(st->work, rescan_hits);
  PushEnvAndroidSdkNdkEmsdkEditsOnly(st);
  g_env_settings.android_sdk_path = st->work.android_sdk_path;
  g_env_settings.android_ndk_path = st->work.android_ndk_path;
  g_env_settings.emsdk_path = st->work.emsdk_path;
  SaveGuiEnvSettings();
  if (dlg && IsWindow(dlg))
    SetWindowTextW(dlg, BuildDetectStatusText(st->work).c_str());
}

void RefreshEnvSettingsDialogLanguage() {
  HWND dlg = g_env_settings_dialog_hwnd;
  if (!dlg || !IsWindow(dlg))
    return;
  auto* st = reinterpret_cast<EnvDialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
  if (!st)
    return;
  if (st->tab) {
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"本地环境", L"Local")));
    TabCtrl_SetItem(st->tab, 0, &ti);
    ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"Android 环境", L"Android")));
    TabCtrl_SetItem(st->tab, 1, &ti);
    ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"emsdk 环境", L"emsdk")));
    TabCtrl_SetItem(st->tab, 2, &ti);
  }
  if (st->lbl_local_build)
    SetWindowTextW(st->lbl_local_build, T(L"建造系统", L"Build system"));
  if (st->lbl_local_vcvars)
    SetWindowTextW(st->lbl_local_vcvars, T(L"VS vcvars", L"VS vcvars"));
  if (st->lbl_local_compiler)
    SetWindowTextW(st->lbl_local_compiler, T(L"编译器", L"Compiler"));
  if (st->lbl_android_sdk)
    SetWindowTextW(st->lbl_android_sdk, T(L"Android SDK 路径", L"Android SDK"));
  if (st->btn_android_sdk)
    SetWindowTextW(st->btn_android_sdk, T(L"浏览…", L"Browse…"));
  if (st->lbl_android_ndk)
    SetWindowTextW(st->lbl_android_ndk, T(L"Android NDK 路径", L"Android NDK"));
  if (st->btn_android_ndk)
    SetWindowTextW(st->btn_android_ndk, T(L"浏览…", L"Browse…"));
  if (st->lbl_emsdk)
    SetWindowTextW(st->lbl_emsdk, T(L"emsdk 路径", L"emsdk path"));
  if (st->btn_emsdk)
    SetWindowTextW(st->btn_emsdk, T(L"浏览…", L"Browse…"));
  if (st->btn_auto)
    SetWindowTextW(st->btn_auto, T(L"自动搜索", L"Auto-detect"));
  if (st->btn_ok)
    SetWindowTextW(st->btn_ok, T(L"确定", L"OK"));
  if (st->btn_cancel)
    SetWindowTextW(st->btn_cancel, T(L"取消", L"Cancel"));
  SetWindowTextW(dlg, BuildDetectStatusText(st->work).c_str());
  PushEnvDialogValues(st);
  const int tab = st->tab ? TabCtrl_GetCurSel(st->tab) : 0;
  ShowEnvTab(st, tab);
  LayoutEnvDialog(dlg, st);
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
      ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"本地环境", L"Local")));
      SendMessageW(st->tab, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ti));
      ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"Android 环境", L"Android")));
      SendMessageW(st->tab, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&ti));
      ti.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"emsdk 环境", L"emsdk")));
      SendMessageW(st->tab, TCM_INSERTITEMW, 2, reinterpret_cast<LPARAM>(&ti));

      st->lbl_local_build = CreateWindowExW(0, L"STATIC", T(L"建造系统", L"Build system"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                            GetModuleHandleW(nullptr), nullptr);
      st->lv_local_build = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                           WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                           0, 0, 100, 100, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_BUILD_LIST)),
                                           GetModuleHandleW(nullptr), nullptr);
      st->lbl_local_vcvars = CreateWindowExW(0, L"STATIC", T(L"VS vcvars", L"VS vcvars"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                             GetModuleHandleW(nullptr), nullptr);
      st->lv_local_vcvars = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                            0, 0, 100, 100, hwnd,
                                            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_VCVARS_LIST)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->lbl_local_compiler = CreateWindowExW(0, L"STATIC", T(L"编译器", L"Compiler"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                               GetModuleHandleW(nullptr), nullptr);
      st->lv_local_compiler = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                              WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                                              0, 0, 100, 100, hwnd,
                                              reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_LOCAL_COMPILER_LIST)),
                                              GetModuleHandleW(nullptr), nullptr);
      st->lbl_android_sdk = CreateWindowExW(0, L"STATIC", T(L"Android SDK 路径", L"Android SDK"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->edt_android_sdk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                            0, 0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_ANDROID_SDK)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->btn_android_sdk = CreateWindowExW(0, L"BUTTON", T(L"浏览…", L"Browse…"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->lbl_android_ndk = CreateWindowExW(0, L"STATIC", T(L"Android NDK 路径", L"Android NDK"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->edt_android_ndk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                            0, 0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_ANDROID_NDK)),
                                            GetModuleHandleW(nullptr), nullptr);
      st->btn_android_ndk = CreateWindowExW(0, L"BUTTON", T(L"浏览…", L"Browse…"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                            nullptr, GetModuleHandleW(nullptr), nullptr);
      st->lbl_emsdk = CreateWindowExW(0, L"STATIC", T(L"emsdk 路径", L"emsdk path"), WS_CHILD | WS_VISIBLE, 0, 0, 80, 22, hwnd, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
      st->edt_emsdk = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                      0, 100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_EMSDK)),
                                      GetModuleHandleW(nullptr), nullptr);
      st->btn_emsdk = CreateWindowExW(0, L"BUTTON", T(L"浏览…", L"Browse…"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                      nullptr, GetModuleHandleW(nullptr), nullptr);
      st->btn_auto = CreateWindowExW(0, L"BUTTON", T(L"自动搜索", L"Auto-detect"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_AUTO)),
                                     GetModuleHandleW(nullptr), nullptr);
      st->btn_ok = CreateWindowExW(0, L"BUTTON", T(L"确定", L"OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ENV_OK)),
                                   GetModuleHandleW(nullptr), nullptr);
      st->btn_cancel = CreateWindowExW(0, L"BUTTON", T(L"取消", L"Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
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
      if (HIWORD(wParam) == EN_KILLFOCUS &&
          (id == IDC_ENV_ANDROID_SDK || id == IDC_ENV_ANDROID_NDK || id == IDC_ENV_EMSDK)) {
        EnvCommitAndroidEmsdkPathsToGlobal(st, hwnd, id == IDC_ENV_ANDROID_SDK);
        return 0;
      }
      if ((HWND)lParam == st->btn_android_sdk) {
        std::wstring folder;
        std::wstring init = g_browse_history.android_sdk_folder;
        if (init.empty())
          GetEditText(st->edt_android_sdk, init);
        if (PickFolder(hwnd, init, folder)) {
          folder = PathToPortableSlashes(std::move(folder));
          SetWindowTextW(st->edt_android_sdk, folder.c_str());
          g_browse_history.android_sdk_folder = folder;
          EnvCommitAndroidEmsdkPathsToGlobal(st, hwnd, true);
        }
        return 0;
      }
      if ((HWND)lParam == st->btn_android_ndk) {
        std::wstring folder;
        std::wstring init = g_browse_history.android_ndk_folder;
        if (init.empty())
          GetEditText(st->edt_android_ndk, init);
        if (PickFolder(hwnd, init, folder)) {
          folder = PathToPortableSlashes(std::move(folder));
          SetWindowTextW(st->edt_android_ndk, folder.c_str());
          g_browse_history.android_ndk_folder = folder;
          EnvCommitAndroidEmsdkPathsToGlobal(st, hwnd, true);
        }
        return 0;
      }
      if ((HWND)lParam == st->btn_emsdk) {
        std::wstring folder;
        std::wstring init = g_browse_history.emsdk_folder;
        if (init.empty())
          GetEditText(st->edt_emsdk, init);
        if (PickFolder(hwnd, init, folder)) {
          folder = PathToPortableSlashes(std::move(folder));
          SetWindowTextW(st->edt_emsdk, folder.c_str());
          g_browse_history.emsdk_folder = folder;
          EnvCommitAndroidEmsdkPathsToGlobal(st, hwnd, true);
        }
        return 0;
      }
      if (id == IDC_ENV_OK) {
        PullEnvDialogValues(st);
        EnvFinalizeAndroidEmsdkPathsInWork(st->work, true);
        PushEnvAndroidSdkNdkEmsdkEditsOnly(st);
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
          LayoutEnvDialog(hwnd, st);
          RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
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
            RefreshStoredVcvars32And64Paths(st->work);
          }
          PullEnvDialogValues(st);
          SetWindowTextW(hwnd, BuildDetectStatusText(st->work).c_str());
          return 0;
        }
      }
      return 0;
    case WM_DESTROY:
      if (g_env_settings_dialog_hwnd == hwnd)
        g_env_settings_dialog_hwnd = nullptr;
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_CLOSE:
      ShowWindow(hwnd, SW_HIDE);
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
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kEnvClass, T(L"编译环境设置", L"Build environment"),
                             WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE | WS_THICKFRAME | WS_CLIPCHILDREN,
                             rc.left + 32, rc.top + 28, 860, 560, owner, nullptr, GetModuleHandleW(nullptr), &st);
  if (!dlg) {
    EnableWindow(owner, TRUE);
    return false;
  }
  g_env_settings_dialog_hwnd = dlg;
  MSG msg{};
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  g_env_settings_dialog_hwnd = nullptr;
  EnableWindow(owner, TRUE);
  SetActiveWindow(owner);
  RedrawWindow(owner, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
  if (st.accepted) {
    g_env_settings = std::move(st.work);
    SaveGuiEnvSettings();
    return true;
  }
  return false;
}

std::wstring UpExePath() {
  return (std::filesystem::path(DirOfModule()) / L"up.exe").lexically_normal().generic_wstring();
}

bool PickFolder(HWND owner, const std::wstring& initial_dir, std::wstring& out) {
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
    return false;
  DWORD opt = 0;
  dlg->GetOptions(&opt);
  dlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  if (!initial_dir.empty()) {
    std::error_code ec;
    std::filesystem::path p(initial_dir);
    if (std::filesystem::is_directory(p, ec)) {
      IShellItem* init = nullptr;
      if (SUCCEEDED(SHCreateItemFromParsingName(p.c_str(), nullptr, IID_PPV_ARGS(&init)))) {
        dlg->SetFolder(init);
        dlg->SetDefaultFolder(init);
        init->Release();
      }
    }
  }
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
    SetStatus(T(L"已刷新运行/测试目标", L"Run/Test targets refreshed"));
  else
    SetStatus(T(L"未找到运行/测试目标", L"No run/test targets found"));
}

std::filesystem::path ResolveUpCachePath(const std::wstring& cwd) {
  if (cwd.empty())
    return {};
  // configure 写入：<cwd>/.intermediate/build/<leaf>/up_cache.txt，其中 <leaf> 为 --build-dir-name（与 print-build-dir-name 一致），
  // 不一定是 "default"。旧工程可能仍只有 default。
  const auto try_leaf = [&](const std::wstring& leaf) -> std::filesystem::path {
    if (leaf.empty())
      return {};
    std::error_code ec{};
    const auto p = std::filesystem::path(cwd) / L".intermediate" / L"build" / leaf / L"up_cache.txt";
    if (std::filesystem::exists(p, ec))
      return p;
    return {};
  };

  const std::wstring up = UpExePath();
  if (GetFileAttributesW(up.c_str()) != INVALID_FILE_ATTRIBUTES) {
    std::wstring leaf;
    std::wstring err;
    if (QueryConfigureBuildDirNameFromUpExe(up, cwd, leaf, err) && !leaf.empty()) {
      const auto p = try_leaf(leaf);
      if (!p.empty())
        return p;
    }
  }
  return try_leaf(L"default");
}

void RefreshBuildDirDisplay(const std::filesystem::path& cache_path) {
  if (!g_build_dir)
    return;
  if (cache_path.empty()) {
    SetWindowTextW(g_build_dir, L"");
    return;
  }
  const std::wstring build_dir = std::filesystem::absolute(cache_path.parent_path()).lexically_normal().generic_wstring();
  SetWindowTextW(g_build_dir, build_dir.c_str());
}

std::string ReadUpCacheArchUtf8(const std::filesystem::path& cache_path) {
  std::ifstream f(cache_path);
  if (!f)
    return {};
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    if (line.substr(0, pos) == "arch")
      return line.substr(pos + 1);
  }
  return {};
}

void RefreshInstallDirDisplay(const std::filesystem::path& cache_path, const std::wstring& cwd_w) {
  if (!g_install_dir)
    return;
  if (cache_path.empty() || cwd_w.empty()) {
    SetWindowTextW(g_install_dir, L"");
    return;
  }
  const std::string arch_utf8 = ReadUpCacheArchUtf8(cache_path);
  if (arch_utf8.empty()) {
    SetWindowTextW(g_install_dir, L"");
    return;
  }
  const std::wstring arch_w = Utf8ToWide(arch_utf8);
  const std::filesystem::path inst =
      std::filesystem::path(cwd_w) / L".intermediate" / L"install" / arch_w;
  const std::wstring install_dir = std::filesystem::absolute(inst).lexically_normal().generic_wstring();
  SetWindowTextW(g_install_dir, install_dir.c_str());
}

void LoadOptionsFromCache(const std::wstring& cwd, bool restore_scan_roots = true) {
  const auto cache = ResolveUpCachePath(cwd);
  RefreshBuildDirDisplay(cache);
  RefreshInstallDirDisplay(cache, cwd);
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
          scan_roots.push_back(PathToPortableSlashes(Utf8ToWide(part)));
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

// Takes full path or a single name; returns the leaf directory name for .intermediate/build|install.
std::wstring IntermediateLeafFromPathishEdit(std::wstring s) {
  TrimInPlace(s);
  if (s.empty())
    return {};
  std::wstring leaf = std::filesystem::path(s).filename().wstring();
  if (leaf.empty() || leaf == L"." || leaf == L"..")
    return {};
  return leaf;
}

// Appends ` --build-dir-name <leaf>` (name under .intermediate/build). When required and empty, logs and returns false.
bool AppendBuildDirFlagOrAbort(std::wstring& args_no_exe, bool required) {
  if (!g_build_dir) {
    if (required) {
      AppendLog(g_ui_lang_zh ? L"[错误] Build Dir 控件未初始化。\r\n" : L"[Error] Build Dir control is not initialized.\r\n");
      return false;
    }
    return true;
  }
  std::wstring bd;
  GetEditText(g_build_dir, bd);
  const std::wstring leaf = IntermediateLeafFromPathishEdit(bd);
  if (leaf.empty()) {
    if (required) {
      AppendLog(g_ui_lang_zh ? L"[错误] 请填写「Build Dir」或构建目录名（对应 CLI：--build-dir-name），再执行 build。\r\n"
                           : L"[Error] Set Build Dir or build leaf name (--build-dir-name) before build.\r\n");
      return false;
    }
    return true;
  }
  args_no_exe += L" --build-dir-name ";
  args_no_exe += QuoteWinArg(leaf);
  return true;
}

// Appends ` --install-dir-name <leaf>` (name under .intermediate/install).
bool AppendInstallDirFlagOrAbort(std::wstring& args_no_exe, bool required) {
  if (!g_install_dir) {
    if (required) {
      AppendLog(g_ui_lang_zh ? L"[错误] 安装目录控件未初始化。\r\n" : L"[Error] Install Dir control is not initialized.\r\n");
      return false;
    }
    return true;
  }
  std::wstring id;
  GetEditText(g_install_dir, id);
  const std::wstring leaf = IntermediateLeafFromPathishEdit(id);
  if (leaf.empty()) {
    if (required) {
      AppendLog(g_ui_lang_zh ? L"[错误] 请填写「安装目录」或 install 子目录名（对应 CLI：--install-dir-name），再执行 run/test。\r\n"
                           : L"[Error] Set Install Dir or install leaf (--install-dir-name) before run/test.\r\n");
      return false;
    }
    return true;
  }
  args_no_exe += L" --install-dir-name ";
  args_no_exe += QuoteWinArg(leaf);
  return true;
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

// Shells out to up.exe (same UP_* as configure) — no compile-time dependency on src/.
bool QueryConfigureBuildDirNameFromUpExe(const std::wstring& up_exe, const std::wstring& cwd, std::wstring& out_arch_leaf,
                                         std::wstring& err_msg) {
  out_arch_leaf.clear();
  err_msg.clear();
  std::wstring bd;
  if (g_build_dir)
    GetEditText(g_build_dir, bd);
  std::wstring cache_leaf = IntermediateLeafFromPathishEdit(bd);
  if (cache_leaf.empty())
    cache_leaf = L"default";

  std::wstring opt_part;
  AppendConfigureOptions(opt_part);
  AppendConfigureEnvArgs(opt_part);

  std::wstring cmdline = L"\"";
  cmdline += up_exe;
  cmdline += L"\" print-build-dir-name --build-dir-name ";
  cmdline += QuoteWinArg(cache_leaf);
  cmdline += opt_part;

  std::wstring output;
  DWORD code = static_cast<DWORD>(-1);
  if (!RunProcessCapture(up_exe, cmdline, cwd, output, code)) {
    err_msg = g_ui_lang_zh ? L"无法启动 print-build-dir-name。" : L"Failed to start print-build-dir-name.";
    return false;
  }
  TrimInPlace(output);
  while (!output.empty() && (output.back() == L'\r' || output.back() == L'\n'))
    output.pop_back();
  TrimInPlace(output);
  const auto line_end = output.find_first_of(L"\r\n");
  if (line_end != std::wstring::npos)
    output.resize(line_end);
  TrimInPlace(output);
  if (code != 0 || output.empty()) {
    err_msg = (g_ui_lang_zh ? L"print-build-dir-name 失败 (退出码 " : L"print-build-dir-name failed (exit code ") +
              std::to_wstring(static_cast<unsigned long>(code)) + L")";
    if (!output.empty())
      err_msg += L": " + output;
    return false;
  }
  out_arch_leaf = std::move(output);
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
  if (g_vars)
    EnableWindow(g_vars, en);
  if (g_opt_group)
    EnableWindow(g_opt_group, en);
  if (g_opt_pick)
    EnableWindow(g_opt_pick, en);
  if (g_opt_custom)
    EnableWindow(g_opt_custom, en);
  if (g_opt_apply)
    EnableWindow(g_opt_apply, en);
  HMENU menu = GetMenu(g_hwnd);
  if (menu) {
    const UINT gray = MF_BYCOMMAND | MF_GRAYED;
    const UINT ena = MF_BYCOMMAND | MF_ENABLED;
    EnableMenuItem(menu, IDM_EXIT, ena);
    EnableMenuItem(menu, IDM_ABOUT, running ? gray : ena);
    EnableMenuItem(menu, IDM_UP_HELP, running ? gray : ena);
    EnableMenuItem(menu, IDC_PROJECT, running ? gray : ena);
    EnableMenuItem(menu, IDC_CONFIGURE, running ? gray : ena);
    EnableMenuItem(menu, IDC_BUILD, running ? gray : ena);
    EnableMenuItem(menu, IDC_TEST, running ? gray : ena);
    EnableMenuItem(menu, IDC_PACK, running ? gray : ena);
    EnableMenuItem(menu, IDC_RUN, running ? gray : ena);
    EnableMenuItem(menu, IDC_ENV_SETTINGS, running ? gray : ena);
    EnableMenuItem(menu, IDM_LANG_ZH, ena);
    EnableMenuItem(menu, IDM_LANG_EN, ena);
  }
  SetStatus(running ? T(L"正在运行 up…", L"Running up…") : T(L"就绪", L"Ready"));
}

bool FinishConfigureArgsForRun(std::wstring& args_no_exe, std::wstring& err_out) {
  if (args_no_exe.rfind(L"configure", 0) != 0)
    return true;
  AppendConfigureScanDirs(args_no_exe);
  AppendConfigureOptions(args_no_exe);
  AppendConfigureEnvArgs(args_no_exe);
  const std::wstring up = UpExePath();
  std::wstring cwd;
  GetEditText(g_path, cwd);
  std::wstring arch_leaf;
  std::wstring qerr;
  if (!QueryConfigureBuildDirNameFromUpExe(up, cwd, arch_leaf, qerr)) {
    err_out = (g_ui_lang_zh ? L"[错误] 无法从 up.exe 计算 --build-dir-name: " : L"[Error] Cannot resolve --build-dir-name from up.exe: ") +
              qerr + L"\r\n";
    return false;
  }
  args_no_exe += L" --build-dir-name ";
  args_no_exe += QuoteWinArg(arch_leaf);
  return true;
}

void RunUpAsync(std::wstring args_no_exe) {
  const std::wstring up = UpExePath();
  if (GetFileAttributesW(up.c_str()) == INVALID_FILE_ATTRIBUTES) {
    AppendLog(g_ui_lang_zh ? L"[错误] 找不到 up.exe，请与 up-gui.exe 放在同一目录。\r\n"
                           : L"[Error] up.exe not found (place next to up-gui.exe).\r\n");
    return;
  }

  std::wstring cwd;
  GetEditText(g_path, cwd);
  if (cwd.empty()) {
    AppendLog(g_ui_lang_zh ? L"[错误] 请先填写或选择「当前工作目录 (CWD)」。\r\n"
                           : L"[Error] Set or pick the working directory (CWD) first.\r\n");
    return;
  }

  std::wstring extra;
  GetEditText(g_extra, extra);
  TrimInPlace(extra);

  if (args_no_exe.rfind(L"configure", 0) == 0) {
    std::wstring err;
    if (!FinishConfigureArgsForRun(args_no_exe, err)) {
      AppendLog(err);
      return;
    }
  }

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
      pack->output =
          g_ui_lang_zh ? L"[错误] CreateProcess 失败。\r\n" : L"[Error] CreateProcess failed.\r\n";
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

  // Build Dir（configure 成功后显示当前 build 目录）
  MoveWindow(g_lbl_build_dir, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_build_dir, pad + lblW + 4, y + 2, w - pad * 2 - lblW - 4, editH, TRUE);
  y += editH + pad;

  // Install Dir（与 up 的 default_install_root(cwd)/<arch> 一致，arch 来自 up_cache.txt）
  MoveWindow(g_lbl_install_dir, pad, y + 4, lblW, 20, TRUE);
  MoveWindow(g_install_dir, pad + lblW + 4, y + 2, w - pad * 2 - lblW - 4, editH, TRUE);
  y += editH + pad;

  // 第三行：运行目标（左）+ 运行参数（右）（runComboW + extraW = avail_total）
  const int midGap = 10;
  int avail_total = w - pad * 2 - lblW * 2 - 8 - midGap;
  if (avail_total < 2)
    avail_total = 2;
  int runComboW = (avail_total * 11) / 20;
  int extraW = avail_total - runComboW;
  if (runComboW < 56 || extraW < 56) {
    runComboW = avail_total / 2;
    extraW = avail_total - runComboW;
  }
  const int xRunLbl = pad;
  const int xRun = pad + lblW + 4;
  const int xExtraLbl = xRun + runComboW + midGap;
  const int xExtra = xExtraLbl + lblW + 4;
  MoveWindow(g_lbl_run, xRunLbl, y + 4, lblW, 20, TRUE);
  MoveWindow(g_run_target, xRun, y, runComboW, 240, TRUE);
  MoveWindow(g_lbl_extra, xExtraLbl, y + 4, lblW, 20, TRUE);
  MoveWindow(g_extra, xExtra, y, extraW, editH, TRUE);
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
    AppendLog(std::wstring(g_ui_lang_zh ? L"[警告] " : L"[Warning] ") + opt.name +
              (g_ui_lang_zh ? L" 的值 \"" : L" value \"") + value +
              (g_ui_lang_zh ? L"\" 不在推荐候选中，已按自定义值保存。\r\n"
                            : L"\" is not in the suggested list; saved as custom.\r\n"));
    SetStatus(T(L"警告：已使用自定义 Option 值", L"Warning: custom option value used"));
    warned = true;
  }

  if (opt.name == L"UP_CMAKE_GENERATOR") {
    const OptionRow* build = FindOptionByName(L"UP_TARGET_BUILD_SYSTEM");
    if (build && _wcsicmp(build->value.c_str(), L"cmake") != 0) {
      AppendLog(g_ui_lang_zh ? L"[警告] 当前 UP_TARGET_BUILD_SYSTEM 不是 cmake，UP_CMAKE_GENERATOR 可能不会生效。\r\n"
                           : L"[Warning] UP_TARGET_BUILD_SYSTEM is not cmake; UP_CMAKE_GENERATOR may be ignored.\r\n");
      SetStatus(T(L"警告：生成器可能被忽略", L"Warning: generator may be ignored"));
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
      SetStatus(T(L"已复制变量名", L"Copied option name"));
    else
      SetStatus(T(L"复制失败", L"Copy failed"));
    return;
  }
  if (action_id == IDM_OPT_COPY_KEY_VALUE) {
    const std::wstring kv = opt.name + L"=" + opt.value;
    if (CopyTextToClipboard(hwnd, kv))
      SetStatus(T(L"已复制 KEY=VALUE", L"Copied option key=value"));
    else
      SetStatus(T(L"复制失败", L"Copy failed"));
    return;
  }
  if (action_id == IDM_OPT_RESET_DEFAULT) {
    for (const auto& d : g_default_options) {
      if (d.name == opt.name) {
        opt.value = d.value;
        RebuildOptionsListView();
        SetStatus(T(L"已恢复为默认 Option", L"Option reset to default"));
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
  SendMessageW(g_vars, LVM_REMOVEALLGROUPS, 0, 0);
  SendMessageW(g_vars, LVM_ENABLEGROUPVIEW, FALSE, 0);
  g_row_to_option_idx.clear();

  std::wstring cwd;
  if (g_path)
    GetEditText(g_path, cwd);
  if (cwd.empty()) {
    g_selected_option_idx = -1;
    SyncOptionEditorFromSelection();
    return;
  }

  const bool grouped = g_opt_group && (SendMessageW(g_opt_group, BM_GETCHECK, 0, 0) == BST_CHECKED);

  std::vector<int> sorted_order(g_options.size());
  for (size_t i = 0; i < g_options.size(); ++i)
    sorted_order[i] = static_cast<int>(i);
  if (grouped) {
    std::sort(sorted_order.begin(), sorted_order.end(), [](int a, int b) {
      const std::wstring ga = OptionGroupName(g_options[static_cast<size_t>(a)].name);
      const std::wstring gb = OptionGroupName(g_options[static_cast<size_t>(b)].name);
      if (ga != gb)
        return ga < gb;
      return g_options[static_cast<size_t>(a)].name < g_options[static_cast<size_t>(b)].name;
    });
    SendMessageW(g_vars, LVM_ENABLEGROUPVIEW, TRUE, 0);
  }

  int row = 0;
  std::map<std::wstring, int> group_id_for;
  int next_group_id = 1;

  auto insert_group_if_needed = [&](const std::wstring& gn) -> int {
    const auto it = group_id_for.find(gn);
    if (it != group_id_for.end())
      return it->second;
    const int gid = next_group_id++;
    LVGROUP lg{};
    lg.cbSize = sizeof(LVGROUP);
    lg.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
    lg.pszHeader = const_cast<LPWSTR>(gn.c_str());
    lg.cchHeader = 0;
    lg.iGroupId = gid;
    lg.stateMask = LVGS_NORMAL;
    lg.state = LVGS_NORMAL;
    SendMessageW(g_vars, LVM_INSERTGROUP, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&lg));
    group_id_for.emplace(gn, gid);
    return gid;
  };

  for (int opt_idx : sorted_order) {
    const auto& o = g_options[static_cast<size_t>(opt_idx)];
    LVITEMW it{};
    it.mask = LVIF_TEXT;
    int gid = 0;
    if (grouped) {
      const std::wstring gn = OptionGroupName(o.name);
      gid = insert_group_if_needed(gn);
      it.mask |= LVIF_GROUPID;
      it.iGroupId = gid;
    }
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
  }

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
  SyncOptionEditorFromSelection();
}

void InitVarsList(HWND lv) {
  ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

  LVCOLUMNW col{};
  col.mask = LVCF_TEXT | LVCF_WIDTH;
  col.cx = 230;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"变量名", L"Name")));
  SendMessageW(lv, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
  col.cx = 120;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"值", L"Value")));
  SendMessageW(lv, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
  col.cx = 360;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"候选", L"Choices")));
  SendMessageW(lv, LVM_INSERTCOLUMNW, 2, reinterpret_cast<LPARAM>(&col));

  RebuildOptionsListView();
}

void CreateMainMenu(HWND hwnd) {
  HMENU bar = CreateMenu();
  HMENU file = CreateMenu();
  AppendMenuW(file, MF_STRING, IDM_EXIT, T(L"退出(&X)", L"E&xit"));
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), T(L"文件(&F)", L"&File"));

  HMENU tools = CreateMenu();
  AppendMenuW(tools, MF_STRING, IDC_PROJECT, T(L"工程(&J)", L"pro&ject"));
  AppendMenuW(tools, MF_STRING, IDC_CONFIGURE, T(L"配置(&C)", L"&configure"));
  AppendMenuW(tools, MF_STRING, IDC_BUILD, T(L"编译(&B)", L"&build"));
  AppendMenuW(tools, MF_STRING, IDC_TEST, T(L"测试(&T)", L"&test"));
  AppendMenuW(tools, MF_STRING, IDC_PACK, T(L"打包(&P)", L"&pack"));
  AppendMenuW(tools, MF_STRING, IDC_RUN, T(L"运行(&R)…", L"&run…"));
  AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(tools, MF_STRING, IDC_ENV_SETTINGS, T(L"编译环境设置…", L"Build &environment…"));
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), T(L"操作(&O)", L"&Actions"));

  HMENU lang = CreateMenu();
  AppendMenuW(lang, MF_STRING | (g_ui_lang_zh ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_ZH, L"简体中文(&S)");
  AppendMenuW(lang, MF_STRING | (!g_ui_lang_zh ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_EN, L"English(&E)");
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(lang), T(L"语言(&L)", L"&Language"));

  HMENU help = CreateMenu();
  AppendMenuW(help, MF_STRING, IDM_UP_HELP, T(L"up 帮助(&U)…", L"up &Help…"));
  AppendMenuW(help, MF_STRING, IDM_ABOUT, T(L"关于(&A)…", L"&About…"));
  AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), T(L"帮助(&H)", L"&Help"));

  SetMenu(hwnd, bar);
}

struct UpHelpDialogState {
  std::wstring text;
  HWND edit{};
  HWND btn_copy{};
  HWND btn_close{};
};

void LayoutUpHelpDialog(HWND hwnd, UpHelpDialogState* st) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int pad = 10;
  const int btn_h = 28;
  const int btn_w = 90;
  MoveWindow(st->edit, pad, pad, rc.right - pad * 2, rc.bottom - pad * 3 - btn_h, TRUE);
  const int y = rc.bottom - pad - btn_h;
  MoveWindow(st->btn_copy, pad, y, btn_w, btn_h, TRUE);
  MoveWindow(st->btn_close, rc.right - pad - btn_w, y, btn_w, btn_h, TRUE);
}

LRESULT CALLBACK UpHelpWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* st = reinterpret_cast<UpHelpDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
      st = reinterpret_cast<UpHelpDialogState*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
      const HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      st->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->text.c_str(),
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL |
                                     ES_READONLY | ES_WANTRETURN,
                                 0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
      st->btn_copy = CreateWindowExW(0, L"BUTTON", T(L"复制", L"Copy"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                     nullptr, GetModuleHandleW(nullptr), nullptr);
      st->btn_close = CreateWindowExW(0, L"BUTTON", T(L"关闭", L"Close"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 90, 28, hwnd,
                                      nullptr, GetModuleHandleW(nullptr), nullptr);
      SendMessageW(st->edit, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_copy, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_close, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      LayoutUpHelpDialog(hwnd, st);
      return 0;
    }
    case WM_SIZE:
      if (st)
        LayoutUpHelpDialog(hwnd, st);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 560;
      mmi->ptMinTrackSize.y = 380;
      return 0;
    }
    case WM_COMMAND:
      if (!st)
        return 0;
      if ((HWND)lParam == st->btn_copy) {
        const int len = GetWindowTextLengthW(st->edit);
        std::wstring text;
        if (len > 0) {
          text.resize(static_cast<size_t>(len) + 1);
          GetWindowTextW(st->edit, text.data(), len + 1);
          text.resize(static_cast<size_t>(len));
        }
        CopyTextToClipboard(hwnd, text);
        return 0;
      }
      if ((HWND)lParam == st->btn_close) {
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

void ShowUpHelpDialog(HWND owner, const std::wstring& text) {
  static bool cls_registered = false;
  static constexpr wchar_t kHelpClass[] = L"UpGuiHelpDialogClass";
  if (!cls_registered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = UpHelpWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kHelpClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    cls_registered = true;
  }
  UpHelpDialogState st{};
  st.text = text;
  EnableWindow(owner, FALSE);
  RECT rc{};
  GetWindowRect(owner, &rc);
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kHelpClass, T(L"up 帮助信息", L"up Help"),
                             WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE | WS_THICKFRAME, rc.left + 40, rc.top + 40,
                             760, 520, owner, nullptr, GetModuleHandleW(nullptr), &st);
  if (!dlg) {
    EnableWindow(owner, TRUE);
    return;
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
}

void ShowUpHelpInfo(HWND hwnd) {
  const std::wstring up = UpExePath();
  if (GetFileAttributesW(up.c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(hwnd, T(L"未找到 up.exe（需与 up-gui.exe 同目录）。", L"up.exe not found (must sit next to up-gui.exe)."),
                T(L"up 帮助信息", L"up Help"), MB_OK | MB_ICONWARNING);
    return;
  }
  SetStatus(T(L"正在运行 up --help…", L"Running up --help…"));
  const std::wstring cmd = L"\"" + up + L"\" --help";
  std::wstring output;
  DWORD exit_code = 0;
  if (!RunProcessCapture(up, cmd, L"", output, exit_code)) {
    SetStatus(T(L"运行 up --help 失败", L"Failed to run up --help"));
    MessageBoxW(hwnd, T(L"执行 up --help 失败。", L"Failed to run up --help."), T(L"up 帮助信息", L"up Help"),
                MB_OK | MB_ICONERROR);
    return;
  }
  std::wstringstream ss;
  ss << L"$ " << cmd << L"\r\n\r\n" << output << L"\r\n";
  if (g_ui_lang_zh)
    ss << L"[退出码 " << static_cast<unsigned long>(exit_code) << L"]\r\n";
  else
    ss << L"[exit code " << static_cast<unsigned long>(exit_code) << L"]\r\n";
  ShowUpHelpDialog(hwnd, ss.str());
  SetStatus(T(L"已加载 up --help", L"up --help loaded"));
}

bool IsConfigureOptionName(const std::wstring& name) {
  if (name.rfind(L"UP_TARGET_", 0) == 0)
    return true;
  return _wcsicmp(name.c_str(), L"UP_CMAKE_GENERATOR") == 0;
}

std::wstring BuildConfigureOptionText() {
  std::wstringstream ss;
  for (const auto& o : g_options) {
    if (!IsConfigureOptionName(o.name))
      continue;
    ss << o.name << L"=" << o.value << L"\r\n";
  }
  return ss.str();
}

struct ConfigureOptionDialogState {
  HWND btn_ok{};
  HWND btn_cancel{};
  HWND owner{};
  bool accepted = false;
};

void LayoutConfigureOptionDialog(HWND hwnd, ConfigureOptionDialogState* st) {
  if (!st)
    return;
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int pad = 10;
  const int editH = 26;
  const int lblW = 68;
  const int btnW = 100;
  const int btnH = 28;
  const int w = rc.right;
  const int h = rc.bottom;
  int y = pad;

  if (g_lbl_vars)
    MoveWindow(g_lbl_vars, pad, y + 4, lblW, 20, TRUE);
  if (g_opt_group)
    MoveWindow(g_opt_group, w - pad - 170, y + 2, 170, 24, TRUE);
  y += 24;

  const int bottomReserved = editH + pad + btnH + pad * 2;
  int varsH = h - y - bottomReserved;
  if (varsH < 100)
    varsH = 100;
  const int maxVars = h - y - 40;
  if (varsH > maxVars)
    varsH = (std::max)(60, maxVars);

  if (g_vars)
    MoveWindow(g_vars, pad + lblW + 4, y, w - pad * 2 - lblW - 4, varsH, TRUE);
  if (g_vars)
    ShowWindow(g_vars, SW_SHOW);
  y += varsH + pad;

  const int optLeft = pad + lblW + 4;
  const int optW = w - pad * 2 - lblW - 4;
  const int third = (std::max)(160, optW / 3);
  if (g_opt_pick)
    MoveWindow(g_opt_pick, optLeft, y, third, editH + 140, TRUE);
  if (g_opt_custom)
    MoveWindow(g_opt_custom, optLeft + third + 8, y, third, editH, TRUE);
  if (g_opt_apply)
    MoveWindow(g_opt_apply, optLeft + third * 2 + 16, y - 1, 120, editH + 2, TRUE);
  y += editH + pad;

  const int btnY = h - pad - btnH;
  if (st->btn_cancel)
    MoveWindow(st->btn_cancel, w - pad - btnW, btnY, btnW, btnH, TRUE);
  if (st->btn_ok)
    MoveWindow(st->btn_ok, w - pad - btnW * 2 - 8, btnY, btnW, btnH, TRUE);
}

LRESULT CALLBACK ConfigureOptionWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* st = reinterpret_cast<ConfigureOptionDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
      st = reinterpret_cast<ConfigureOptionDialogState*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
      if (!st->owner)
        st->owner = GetParent(hwnd);
      const HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      g_lbl_vars = CreateWindowExW(0, L"STATIC", T(L"Option", L"Option"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_VARS)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_opt_group = CreateWindowExW(0, L"BUTTON", T(L"按前缀分组显示", L"Group by prefix"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0,
                                    170, 24, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_GROUP)),
                                    GetModuleHandleW(nullptr), nullptr);
      SendMessageW(g_opt_group, BM_SETCHECK, BST_CHECKED, 0);
      g_vars = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER, 0, 0,
                               100, 80, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_VARS)),
                               GetModuleHandleW(nullptr), nullptr);
      g_opt_pick = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   0, 0, 120, 220, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_PICK)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_opt_custom = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                     0, 160, 26, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_CUSTOM)),
                                     GetModuleHandleW(nullptr), nullptr);
      g_opt_apply = CreateWindowExW(0, L"BUTTON", T(L"应用到选中项", L"Apply to selection"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 120, 28, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OPT_APPLY)),
                                    GetModuleHandleW(nullptr), nullptr);
      st->btn_ok = CreateWindowExW(0, L"BUTTON", T(L"确定并配置", L"Configure"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 28, hwnd,
                                   nullptr, GetModuleHandleW(nullptr), nullptr);
      st->btn_cancel = CreateWindowExW(0, L"BUTTON", T(L"取消", L"Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 28, hwnd,
                                       nullptr, GetModuleHandleW(nullptr), nullptr);

      SendMessageW(g_lbl_vars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(g_opt_group, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(g_vars, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(g_opt_pick, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(g_opt_custom, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(g_opt_apply, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_ok, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      SendMessageW(st->btn_cancel, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      InitVarsList(g_vars);
      LayoutConfigureOptionDialog(hwnd, st);
      return 0;
    }
    case WM_SIZE:
      if (st)
        LayoutConfigureOptionDialog(hwnd, st);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 560;
      mmi->ptMinTrackSize.y = 420;
      return 0;
    }
    case WM_CONTEXTMENU: {
      if (!st || !g_vars)
        return 0;
      HWND src = reinterpret_cast<HWND>(wParam);
      if (src != g_vars)
        return 0;
      POINT pt{};
      pt.x = GET_X_LPARAM(lParam);
      pt.y = GET_Y_LPARAM(lParam);
      if (pt.x == -1 && pt.y == -1) {
        RECT rc{};
        GetWindowRect(g_vars, &rc);
        pt.x = rc.left + 12;
        pt.y = rc.top + 12;
      }
      POINT local = pt;
      ScreenToClient(g_vars, &local);
      LVHITTESTINFO ht{};
      ht.pt = local;
      ht.flags = LVHT_NOWHERE;
      ListView_SubItemHitTest(g_vars, &ht);
      if (ht.iItem < 0 || ht.iItem >= static_cast<int>(g_row_to_option_idx.size()))
        return 0;
      ListView_SetItemState(g_vars, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
      g_selected_option_idx = g_row_to_option_idx[static_cast<size_t>(ht.iItem)];
      SyncOptionEditorFromSelection();

      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu, MF_STRING, IDM_OPT_COPY_KEY, T(L"复制变量名", L"Copy name"));
      AppendMenuW(menu, MF_STRING, IDM_OPT_COPY_KEY_VALUE, T(L"复制 KEY=VALUE", L"Copy KEY=VALUE"));
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      AppendMenuW(menu, MF_STRING, IDM_OPT_RESET_DEFAULT, T(L"重置默认值", L"Reset to default"));
      TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
      DestroyMenu(menu);
      return 0;
    }
    case WM_COMMAND: {
      if (!st)
        return 0;
      const int id = LOWORD(wParam);
      if (id == IDC_OPT_GROUP) {
        RebuildOptionsListView();
        LayoutConfigureOptionDialog(hwnd, st);
        SetStatus(T(L"已更新 Option 分组显示", L"Option grouping updated"));
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
          AppendLog(g_ui_lang_zh ? L"[错误] 请先在 Option 表中选择一个变量。\r\n"
                                : L"[Error] Select an option row first.\r\n");
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
          AppendLog(g_ui_lang_zh ? L"[错误] 请输入值或从候选中选择。\r\n"
                                : L"[Error] Enter a value or pick from the list.\r\n");
          return 0;
        }
        g_options[static_cast<size_t>(g_selected_option_idx)].value = newv;
        const bool warned =
            SoftValidateOptionValue(g_options[static_cast<size_t>(g_selected_option_idx)], newv);
        RebuildOptionsListView();
        if (!warned)
          SetStatus(T(L"已更新 Option 值", L"Option value updated"));
        return 0;
      }
      if (id == IDM_OPT_COPY_KEY || id == IDM_OPT_COPY_KEY_VALUE || id == IDM_OPT_RESET_DEFAULT) {
        HandleOptionContextAction(st->owner ? st->owner : hwnd, id);
        return 0;
      }
      if ((HWND)lParam == st->btn_ok) {
        st->accepted = true;
        DestroyWindow(hwnd);
        return 0;
      }
      if ((HWND)lParam == st->btn_cancel) {
        st->accepted = false;
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
    case WM_NOTIFY: {
      if (!st)
        return 0;
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
      return 0;
    }
    case WM_DESTROY:
      if (g_configure_option_dialog_hwnd == hwnd)
        g_configure_option_dialog_hwnd = nullptr;
      g_vars = nullptr;
      g_lbl_vars = nullptr;
      g_opt_group = nullptr;
      g_opt_pick = nullptr;
      g_opt_custom = nullptr;
      g_opt_apply = nullptr;
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

bool ShowConfigureOptionDialog(HWND owner) {
  static bool cls_registered = false;
  static constexpr wchar_t kClass[] = L"UpGuiConfigureOptionDialogClass";
  if (!cls_registered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = ConfigureOptionWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    cls_registered = true;
  }
  ConfigureOptionDialogState st{};
  st.owner = owner;
  EnableWindow(owner, FALSE);
  RECT rc{};
  GetWindowRect(owner, &rc);
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, T(L"configure 选项设置", L"configure options"),
                             WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE | WS_THICKFRAME, rc.left + 50, rc.top + 40,
                             760, 520, owner, nullptr, GetModuleHandleW(nullptr), &st);
  if (!dlg) {
    EnableWindow(owner, TRUE);
    return false;
  }
  g_configure_option_dialog_hwnd = dlg;
  MSG msg{};
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  g_configure_option_dialog_hwnd = nullptr;
  EnableWindow(owner, TRUE);
  SetActiveWindow(owner);
  return st.accepted;
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
      {IDC_PROJECT, STD_FILEOPEN, T(L"工程", L"project")},
      {IDC_CONFIGURE, STD_PROPERTIES, T(L"配置", L"configure")},
      {IDC_BUILD, STD_REPLACE, T(L"编译", L"build")},
      {IDC_RUN, STD_FILENEW, T(L"运行", L"run")},
      {IDC_TEST, STD_FIND, T(L"测试", L"test")},
      {IDC_PACK, STD_PRINT, T(L"打包", L"pack")},
      {IDC_ENV_SETTINGS, STD_HELP, T(L"环境", L"env")},
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

void DestroyMainMenuBar(HWND hwnd) {
  HMENU bar = GetMenu(hwnd);
  if (!bar)
    return;
  SetMenu(hwnd, nullptr);
  DestroyMenu(bar);
}

void SetMainWindowLocalizedControlTexts() {
  if (!g_hwnd)
    return;
  if (g_lbl_path)
    SetWindowTextW(g_lbl_path, T(L"工作目录", L"CWD"));
  if (g_lbl_vars)
    SetWindowTextW(g_lbl_vars, T(L"Option", L"Option"));
  if (g_opt_group)
    SetWindowTextW(g_opt_group, T(L"按前缀分组显示", L"Group by prefix"));
  if (g_browse)
    SetWindowTextW(g_browse, T(L"浏览…", L"Browse…"));
  if (g_lbl_scan)
    SetWindowTextW(g_lbl_scan, T(L"扫描目录", L"Scan dirs"));
  if (g_scan_add)
    SetWindowTextW(g_scan_add, T(L"添加", L"+Add"));
  if (g_scan_remove)
    SetWindowTextW(g_scan_remove, T(L"删除", L"-Del"));
  if (g_scan_up)
    SetWindowTextW(g_scan_up, T(L"上移", L"Up"));
  if (g_scan_down)
    SetWindowTextW(g_scan_down, T(L"下移", L"Down"));
  if (g_lbl_build_dir)
    SetWindowTextW(g_lbl_build_dir, T(L"构建目录", L"Build dir"));
  if (g_lbl_install_dir)
    SetWindowTextW(g_lbl_install_dir, T(L"安装目录", L"Install dir"));
  if (g_lbl_run)
    SetWindowTextW(g_lbl_run, T(L"运行目标", L"Run target"));
  if (g_lbl_extra)
    SetWindowTextW(g_lbl_extra, T(L"运行参数", L"Extra args"));
  if (g_lbl_test)
    SetWindowTextW(g_lbl_test, T(L"单元测试", L"Tests"));
  if (g_log_copy)
    SetWindowTextW(g_log_copy, T(L"复制日志", L"Copy log"));
  if (g_log_clear)
    SetWindowTextW(g_log_clear, T(L"清空日志", L"Clear log"));
  if (g_opt_apply)
    SetWindowTextW(g_opt_apply, T(L"应用到选中项", L"Apply to selection"));
}

void RefreshVarsListColumnHeaders() {
  if (!g_vars)
    return;
  LVCOLUMNW col{};
  col.mask = LVCF_TEXT;
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"变量名", L"Name")));
  SendMessageW(g_vars, LVM_SETCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"值", L"Value")));
  SendMessageW(g_vars, LVM_SETCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
  col.pszText = const_cast<LPWSTR>(const_cast<wchar_t*>(T(L"候选", L"Choices")));
  SendMessageW(g_vars, LVM_SETCOLUMNW, 2, reinterpret_cast<LPARAM>(&col));
}

void RebuildMainToolbar(HWND parent) {
  const HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  if (g_toolbar) {
    DestroyWindow(g_toolbar);
    g_toolbar = nullptr;
  }
  g_toolbar =
      CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                      WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST | CCS_NODIVIDER |
                          CCS_NOPARENTALIGN,
                      0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_TOOLBAR)),
                      GetModuleHandleW(nullptr), nullptr);
  CreateToolbarButtons(g_toolbar);
  SendMessageW(g_toolbar, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
  LayoutChildren(parent);
}

std::wstring InitialWelcomeLogText() {
  if (g_ui_lang_zh) {
    return L"up-gui：在「当前工作目录 (CWD)」下调用同目录的 up.exe。\r\n"
           L"「运行参数」可加 --scan 指定额外扫描根（与 CWD 相同的路径不会重复传入）；up 默认已扫描当前工作目录。\r\n"
           L"工具栏与「操作」菜单均可触发 configure / build / test / pack / run。\r\n"
           L"运行目标与单元测试列表会在 configure 成功后自动刷新。\r\n"
           L"纯库包（无 executable）也可 configure/build；CMake 导入目标常位于 .targets/<name>/target.xml。\r\n"
           L"点击工具栏「configure」可在弹窗中编辑 Option（含按前缀分组）。\r\n"
           L"使用菜单「语言」可切换界面语言。\r\n\r\n";
  }
  return L"up-gui runs up.exe from the same folder as up-gui.exe, using the working directory (CWD).\r\n"
         L"Use \"Extra args\" to pass --scan for extra scan roots (duplicates of CWD are skipped); up already scans CWD.\r\n"
         L"The toolbar and Actions menu trigger configure / build / test / pack / run.\r\n"
         L"Run targets and test lists refresh after a successful configure.\r\n"
         L"Library-only packages (no executable) are supported for configure/build; imported wrappers are commonly under .targets/<name>/target.xml.\r\n"
         L"Click toolbar \"configure\" to edit Options (including prefix grouping).\r\n"
         L"Use the Language menu to switch UI language.\r\n\r\n";
}

void RefreshConfigureOptionDialogLanguage() {
  HWND d = g_configure_option_dialog_hwnd;
  if (!d || !IsWindow(d))
    return;
  auto* st = reinterpret_cast<ConfigureOptionDialogState*>(GetWindowLongPtrW(d, GWLP_USERDATA));
  SetWindowTextW(d, T(L"configure 选项设置", L"configure options"));
  if (g_lbl_vars)
    SetWindowTextW(g_lbl_vars, T(L"Option", L"Option"));
  if (g_opt_group)
    SetWindowTextW(g_opt_group, T(L"按前缀分组显示", L"Group by prefix"));
  if (g_opt_apply)
    SetWindowTextW(g_opt_apply, T(L"应用到选中项", L"Apply to selection"));
  if (st) {
    if (st->btn_ok)
      SetWindowTextW(st->btn_ok, T(L"确定并配置", L"Configure"));
    if (st->btn_cancel)
      SetWindowTextW(st->btn_cancel, T(L"取消", L"Cancel"));
  }
  RefreshVarsListColumnHeaders();
}

void ApplyMainWindowLanguage(HWND hwnd) {
  if (!hwnd)
    return;
  const bool running = g_running.load();
  DestroyMainMenuBar(hwnd);
  CreateMainMenu(hwnd);
  DrawMenuBar(hwnd);
  SetMainWindowLocalizedControlTexts();
  RefreshVarsListColumnHeaders();
  RebuildMainToolbar(hwnd);
  RefreshEnvSettingsDialogLanguage();
  RefreshConfigureOptionDialogLanguage();
  SetUiRunning(running);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      g_hwnd = hwnd;
      LoadGuiEnvSettings();
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
      g_lbl_path = CreateWindowExW(0, L"STATIC", T(L"工作目录", L"CWD"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_PATH)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_browse = CreateWindowExW(0, L"BUTTON", T(L"浏览…", L"Browse…"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 72, 28, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_BROWSE)), GetModuleHandleW(nullptr),
                                 nullptr);
      g_lbl_scan = CreateWindowExW(0, L"STATIC", T(L"扫描目录", L"Scan dirs"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_SCAN)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_scan_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                    0, 0, 220, 104, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_LIST)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_scan_add = CreateWindowExW(0, L"BUTTON", T(L"添加", L"+Add"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_ADD)),
                                   GetModuleHandleW(nullptr), nullptr);
      g_scan_remove = CreateWindowExW(0, L"BUTTON", T(L"删除", L"-Del"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_REMOVE)),
                                      GetModuleHandleW(nullptr), nullptr);
      g_scan_up = CreateWindowExW(0, L"BUTTON", T(L"上移", L"Up"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_UP)),
                                  GetModuleHandleW(nullptr), nullptr);
      g_scan_down = CreateWindowExW(0, L"BUTTON", T(L"下移", L"Down"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 66, 24, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SCAN_DOWN)),
                                    GetModuleHandleW(nullptr), nullptr);

      g_lbl_build_dir = CreateWindowExW(0, L"STATIC", T(L"构建目录", L"Build dir"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_BUILD_DIR)),
                                        GetModuleHandleW(nullptr), nullptr);
      g_build_dir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                                    0, 0, 100, 24, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_BUILD_DIR)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_lbl_install_dir = CreateWindowExW(0, L"STATIC", T(L"安装目录", L"Install dir"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_INSTALL_DIR)),
                                          GetModuleHandleW(nullptr), nullptr);
      g_install_dir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                                      0, 0, 100, 24, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_INSTALL_DIR)),
                                      GetModuleHandleW(nullptr), nullptr);

      g_lbl_run = CreateWindowExW(0, L"STATIC", T(L"运行目标", L"Run target"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_RUN)),
                                  GetModuleHandleW(nullptr), nullptr);
      g_run_target =
          CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0,
                          0, 100, 220, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_RUNTARGET)),
                          GetModuleHandleW(nullptr), nullptr);
      g_lbl_extra = CreateWindowExW(0, L"STATIC", T(L"运行参数", L"Extra args"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LBL_EXTRA)),
                                    GetModuleHandleW(nullptr), nullptr);
      g_extra = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                                0, 100, 26, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_EXTRA)),
                                GetModuleHandleW(nullptr), nullptr);
      g_lbl_test = CreateWindowExW(0, L"STATIC", T(L"单元测试", L"Tests"), WS_CHILD | WS_VISIBLE, 0, 0, 72, 20, hwnd,
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
          CreateWindowExW(0, L"BUTTON", T(L"复制日志", L"Copy log"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 88, 26, hwnd,
                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG_COPY)), GetModuleHandleW(nullptr), nullptr);
      g_log_clear =
          CreateWindowExW(0, L"BUTTON", T(L"清空日志", L"Clear log"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 88, 26, hwnd,
                          reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LOG_CLEAR)), GetModuleHandleW(nullptr), nullptr);

      g_status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                                 hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_STATUS)),
                                 GetModuleHandleW(nullptr), nullptr);
      SetStatus(T(L"就绪", L"Ready"));

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
        SendMessageW(g_run_target, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_extra, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_test_target, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_browse, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_build_dir, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_build_dir, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_install_dir, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_install_dir, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_extra, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_run, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_lbl_test, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_toolbar, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);

        SendMessageW(g_log_copy, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        SendMessageW(g_log_clear, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
      }

      AppendLogRaw(InitialWelcomeLogText());
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
        if (g_ui_lang_zh)
          swprintf_s(tail, L"\r\n[退出码 %lu]\r\n", static_cast<unsigned long>(pack->exit_code));
        else
          swprintf_s(tail, L"\r\n[exit code %lu]\r\n", static_cast<unsigned long>(pack->exit_code));
        AppendLogRaw(tail);
        wchar_t st[96]{};
        if (g_ui_lang_zh)
          swprintf_s(st, L"完成（退出码 %lu）", static_cast<unsigned long>(pack->exit_code));
        else
          swprintf_s(st, L"Done (exit code %lu)", static_cast<unsigned long>(pack->exit_code));
        SetStatus(st);
        if (pack->exit_code == 0 && g_last_up_args.rfind(L"configure", 0) == 0) {
          RefreshRunTargetListFromPath();
          std::wstring cwd;
          GetEditText(g_path, cwd);
          // 与 up_cache.txt 一致：恢复 UP_* 与 scan_roots（configure 已写入本次实际扫描根）。
          LoadOptionsFromCache(cwd, true);
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
      if (id == IDM_LANG_ZH) {
        g_ui_lang_zh = true;
        SaveGuiEnvSettings();
        ApplyMainWindowLanguage(hwnd);
        return 0;
      }
      if (id == IDM_LANG_EN) {
        g_ui_lang_zh = false;
        SaveGuiEnvSettings();
        ApplyMainWindowLanguage(hwnd);
        return 0;
      }
      if (id == IDM_UP_HELP) {
        ShowUpHelpInfo(hwnd);
        return 0;
      }
      if (id == IDM_ABOUT) {
        MessageBoxW(hwnd,
                    T(L"up-gui：调用同目录 up.exe 的本地外壳。\n布局：菜单栏、工具栏、当前工作目录(CWD)、Build Dir、安装目录、运行目标与运行参数（同行）、单元测试列表、日志、状态栏；Option 在 configure 弹窗中编辑。\n提示：支持纯库包 configure/build；CMake 导入包装目标常在 .targets/<name>/target.xml。",
                        L"up-gui is a local shell that runs up.exe from the same folder.\n"
                        L"Layout: menu bar, toolbar, working directory (CWD), build/install dirs, run targets and extra "
                        L"args, tests, log, status bar; Options are edited in the configure dialog.\n"
                        L"Note: library-only packages are supported for configure/build; CMake imported wrappers are often under .targets/<name>/target.xml."),
                    T(L"关于 up-gui", L"About up-gui"), MB_ICONINFORMATION | MB_OK);
        return 0;
      }
      if (id == IDC_ENV_SETTINGS) {
        if (ShowEnvSettingsDialog(hwnd))
          SetStatus(T(L"已更新编译环境设置", L"Environment settings updated"));
        else
          SetStatus(T(L"编译环境设置未更改", L"Environment settings unchanged"));
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
          SetStatus(T(L"已复制日志", L"Log copied"));
        else
          SetStatus(T(L"复制失败", L"Copy failed"));
        return 0;
      }
      if (id == IDC_LOG_CLEAR) {
        SetWindowTextW(g_log, L"");
        SetStatus(T(L"已清空日志", L"Log cleared"));
        return 0;
      }

      if (id == IDC_BROWSE) {
        std::wstring folder;
        std::wstring init = g_browse_history.cwd_folder;
        if (init.empty())
          GetEditText(g_path, init);
        if (PickFolder(hwnd, init, folder)) {
          folder = PathToPortableSlashes(std::move(folder));
          SetWindowTextW(g_path, folder.c_str());
          g_browse_history.cwd_folder = folder;
          SaveGuiEnvSettings();
          ResetScanDirsForCwd(folder);
          SendMessageW(g_run_target, CB_RESETCONTENT, 0, 0);
          SendMessageW(g_test_target, CB_RESETCONTENT, 0, 0);
          // 若该 CWD 下已有 up_cache.txt，则一并恢复 scan_roots 到「扫描目录」列表。
          LoadOptionsFromCache(folder, true);
          RefreshRunTargetListFromPath();
        }
        return 0;
      }
      if (id == IDC_PATH && HIWORD(wParam) == EN_KILLFOCUS) {
        std::wstring raw;
        GetEditText(g_path, raw);
        const std::wstring cwd = PathToPortableSlashes(raw);
        if (cwd != raw)
          SetWindowTextW(g_path, cwd.c_str());
        if (!cwd.empty()) {
          g_browse_history.cwd_folder = cwd;
          SaveGuiEnvSettings();
          LoadOptionsFromCache(cwd, true);
          RefreshRunTargetListFromPath();
        }
        return 0;
      }
      if (id == IDC_SCAN_ADD) {
        std::wstring folder;
        std::wstring init = g_browse_history.scan_folder;
        if (init.empty())
          GetEditText(g_path, init);
        if (PickFolder(hwnd, init, folder)) {
          folder = PathToPortableSlashes(std::move(folder));
          g_browse_history.scan_folder = folder;
          SaveGuiEnvSettings();
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
      if (id == IDC_PROJECT) {
        RunUpAsync(L"project");
        return 0;
      }
      if (id == IDC_CONFIGURE) {
        if (ShowConfigureOptionDialog(hwnd))
          RunUpAsync(L"configure");
        return 0;
      }
      if (id == IDC_BUILD) {
        std::wstring line = L"build";
        if (!AppendBuildDirFlagOrAbort(line, true))
          return 0;
        RunUpAsync(line);
        return 0;
      }
      if (id == IDC_TEST) {
        const int test_idx = static_cast<int>(SendMessageW(g_test_target, CB_GETCURSEL, 0, 0));
        const int run_idx = static_cast<int>(SendMessageW(g_run_target, CB_GETCURSEL, 0, 0));
        if (test_idx == CB_ERR && run_idx == CB_ERR) {
          AppendLog(g_ui_lang_zh ? L"[提示] 当前包没有可运行/可测试目标（纯库包场景正常）。\r\n"
                               : L"[Info] No run/test targets in current package (normal for library-only packages).\r\n");
          return 0;
        }
        std::wstring testarg = L"test";
        if (!AppendInstallDirFlagOrAbort(testarg, true))
          return 0;
        if (test_idx != CB_ERR) {
          const int n = static_cast<int>(SendMessageW(g_test_target, CB_GETLBTEXTLEN, static_cast<WPARAM>(test_idx), 0));
          if (n > 0) {
            std::wstring tgt(static_cast<size_t>(n), L'\0');
            SendMessageW(g_test_target, CB_GETLBTEXT, static_cast<WPARAM>(test_idx), reinterpret_cast<LPARAM>(tgt.data()));
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
        std::wstring line = L"pack";
        std::wstring inst;
        if (g_install_dir)
          GetEditText(g_install_dir, inst);
        TrimInPlace(inst);
        if (inst.empty()) {
          AppendLog(g_ui_lang_zh ? L"[错误] pack 需要「安装目录」或 install 子目录名（对应 CLI：--install-dir-name）。\r\n"
                               : L"[Error] pack needs Install Dir or install leaf (--install-dir-name).\r\n");
          return 0;
        }
        const std::wstring leaf = IntermediateLeafFromPathishEdit(inst);
        if (leaf.empty()) {
          AppendLog(g_ui_lang_zh ? L"[错误] 无法从安装目录得到有效的 --install-dir-name。\r\n"
                               : L"[Error] Could not derive a valid --install-dir-name from Install Dir.\r\n");
          return 0;
        }
        line += L" --install-dir-name ";
        line += QuoteWinArg(leaf);
        RunUpAsync(line);
        return 0;
      }
      if (id == IDC_RUN) {
        const int idx = static_cast<int>(SendMessageW(g_run_target, CB_GETCURSEL, 0, 0));
        if (idx == CB_ERR) {
          AppendLog(g_ui_lang_zh ? L"[提示] 当前包没有可运行目标（纯库包场景正常）。\r\n"
                               : L"[Info] No run target in current package (normal for library-only packages).\r\n");
          return 0;
        }
        const int n = static_cast<int>(SendMessageW(g_run_target, CB_GETLBTEXTLEN, static_cast<WPARAM>(idx), 0));
        if (n <= 0) {
          AppendLog(g_ui_lang_zh ? L"[错误] 无法读取运行目标。\r\n" : L"[Error] Could not read run target.\r\n");
          return 0;
        }
        std::wstring tgt(static_cast<size_t>(n), L'\0');
        SendMessageW(g_run_target, CB_GETLBTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(tgt.data()));
        std::wstring runarg = L"run";
        if (!AppendInstallDirFlagOrAbort(runarg, true))
          return 0;
        runarg += L' ';
        if (tgt.find(L' ') != std::wstring::npos)
          runarg += L"\"" + tgt + L"\"";
        else
          runarg += tgt;
        RunUpAsync(runarg);
        return 0;
      }
      return 0;
    }

    case WM_CLOSE:
      if (g_running) {
        if (MessageBoxW(hwnd, T(L"up 仍在运行，确定要关闭窗口吗？", L"up is still running. Close the window?"),
                        L"up-gui", MB_YESNO | MB_ICONQUESTION) != IDYES)
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
