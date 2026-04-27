#pragma once

#include <windows.h>

namespace gz::gui::platform::win32 {

// 在 IFileOpenDialog::Show / GetSaveFileNameW 前后成对调用：用 WH_CBT 将系统文件对话框移到 owner 所在显示器工作区居中。
void ModalFileDialogCenterBegin(HWND owner);
void ModalFileDialogCenterEnd();

}  // namespace gz::gui::platform::win32
