#pragma once

#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>

#include <memory>
#include <vector>

#include "Constants.h"
#include "IndicatorContext.h"
#include "KeyboardHook.h"

#include "resource.h"

/**
 * @brief 主窗口类（单例）
 * 负责窗口创建、消息处理和应用程序生命周期管理
 */
class MainWindow : public CDialogImpl<MainWindow> {
public:
  enum { IDD = IDD_KEYLOCK };

  // 禁用拷贝构造和赋值
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  // 禁用移动构造和赋值
  MainWindow(MainWindow&&) noexcept = delete;
  MainWindow& operator=(MainWindow&&) noexcept = delete;

  MainWindow() = default;
  ~MainWindow() = default;

  // 消息映射
  BEGIN_MSG_MAP(MainWindow)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_TIMER, OnTimer)
  MESSAGE_HANDLER(UM_NOTIFY_NUMLOCK, OnTrayIcon)
  MESSAGE_HANDLER(UM_NOTIFY_CAPSLOCK, OnTrayIcon)
  MESSAGE_HANDLER(UM_NOTIFY_SCROLL, OnTrayIcon)
  MESSAGE_HANDLER(UM_KEYBOARD_HOOK, OnKeyboardHook)
  COMMAND_ID_HANDLER(ID_FILE_MODIFY, OnCommand)
  COMMAND_ID_HANDLER(ID_FILE_START, OnCommand)
  COMMAND_ID_HANDLER(ID_FILE_EXIT, OnCommand)
  END_MSG_MAP()

  // 初始化
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);

  // 销毁
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);

  // 定时器
  LRESULT OnTimer(UINT, WPARAM id, LPARAM, BOOL&);

  // 托盘
  LRESULT OnTrayIcon(UINT, WPARAM id, LPARAM l_param, BOOL&);

  // 键盘钩子
  LRESULT OnKeyboardHook(UINT, WPARAM vk_code, LPARAM flags, BOOL&);

  // 命令
  LRESULT OnCommand(WORD, WORD id, HWND, BOOL&);

private:
  HMENU menu_handle_{LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_KEYLOCK))};

  CString menu_text_list_[std::size(INDICATOR_KEYS)];

  uint32_t vk_code_{0};

  // 指示器上下文和键盘钩子
  std::unique_ptr<IndicatorContext> contexts_[std::size(INDICATOR_KEYS)];

  bool IsValidIndex(uint32_t index) const {
    return index < std::size(INDICATOR_KEYS);
  }

  // 辅助方法
  void LoadAppSettings();

  void ModifyAppSettings();
  void ToggleAutoStart();
  void HandleKeyLogic();
};
