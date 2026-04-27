#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// 与主窗、工具栏、菜单、环境对话框及 up_gui.rc 图标 ID 对齐；由 win32_gui.cpp 匿名命名空间 `using` 引入。
namespace up::gui::platform::win32::ids {

constexpr wchar_t kClassName[] = L"UpGuiMainClass";
constexpr wchar_t kTitle[] = L"up-gui";

// Must match numeric id in src_gui/resources/up_gui.rc (RT_ICON).
enum : WORD { kUpGuiAppIconResourceId = 1 };

constexpr int IDC_PATH = 101;
constexpr int IDC_BROWSE = 102;
constexpr int IDC_CONFIGURE = 103;
constexpr int IDC_LIST = 141;
constexpr int IDC_LIST_HINT = 142;
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
constexpr int IDC_STOP = 140;
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
constexpr int IDM_STOP_RUN = 1007;

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
constexpr UINT WM_APPEND_RUN_LOG = WM_APP + 52;

}  // namespace up::gui::platform::win32::ids
