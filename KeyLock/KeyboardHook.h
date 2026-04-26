#pragma once

#include <windows.h>

#include <memory>
#include <vector>

#include "Constants.h"
#include "IndicatorContext.h"

/**
 * @brief 键盘钩子管理器类
 * 负责安装、卸载键盘钩子以及处理键盘事件
 */
struct KeyboardHook {
  // 钩子管理
  static bool InstallHook();
  static void UninstallHook();

  static void SetWindowHandle(HWND window_handle);

private:
  static HWND window_handle_;
  static HHOOK hook_handle_;

  // 钩子过程
  static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param, KeyboardHook* self);
};
