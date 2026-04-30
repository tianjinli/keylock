#include "MainWindow.h"

#include <Shlwapi.h>
#include <shellapi.h>
#pragma comment(lib, "Shcore.lib")

#include "AutoStartManager.h"
#include "ResourceManager.h"
#include "SoundManager.h"

void MainWindow::LoadAppSettings() {
  // 加载菜单文本
  const TCHAR* menu_section = TEXT("MenuText");
  HMENU menu_file = GetSubMenu(menu_handle_, 0);

  struct MenuItem {
    uint32_t command_id;
    const TCHAR* ini_key;
    const TCHAR* default_text;
  };

  const MenuItem menu_items[] = {{ID_FILE_MODIFY, TEXT("FileModify"), TEXT("Modify Settings")},
                                 {ID_FILE_START, TEXT("FileStart"), TEXT("Auto Start")},
                                 {ID_FILE_EXIT, TEXT("FileExit"), TEXT("Quick Exit")}};

  if (auto ini_handle = ResourceManager::LoadIniWithFallback(CONFIG_NAME, IDR_APPSETTINGS); ini_handle != nullptr) {
    menu_text_list_[0] = ini_handle->GetValue(menu_section, TEXT("FileNum"), TEXT("Num Lock"));
    menu_text_list_[1] = ini_handle->GetValue(menu_section, TEXT("FileCaps"), TEXT("Caps Lock"));
    menu_text_list_[2] = ini_handle->GetValue(menu_section, TEXT("FileScroll"), TEXT("Scroll Lock"));

    for (const auto& item: menu_items) {
      auto text = ini_handle->GetValue(menu_section, item.ini_key, item.default_text);
      ModifyMenu(menu_file, item.command_id, MF_BYCOMMAND, item.command_id, text);
    }
    for (int i = std::size(INDICATOR_KEYS) - 1; i >= 0; --i) {
      contexts_[i] = std::make_unique<IndicatorContext>(static_cast<KeyType>(i), m_hWnd);
      contexts_[i]->LoadIndicators(ini_handle, m_hWnd);
    }
  }
}

LRESULT MainWindow::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  // 加载菜单
  menu_handle_ = LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_KEYLOCK));

  // 加载配置
  LoadAppSettings();

  // 设置键盘钩子
  KeyboardHook::SetWindowHandle(m_hWnd);
  KeyboardHook::InstallHook();
  // 自动恢复按键状态
  HandleKeyLogic();
  return 0;
}

LRESULT MainWindow::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
  KeyboardHook::UninstallHook();

  // 清理菜单
  if (menu_handle_) {
    DestroyMenu(menu_handle_);
    menu_handle_ = nullptr;
  }
  PostQuitMessage(0); // 退出消息循环
  return 0;
}

LRESULT MainWindow::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
  KillTimer(id);
  if (id == kHudAutoHideTimerID) {
    ShowWindowAsync(SW_HIDE);
  } else if (id == kHookKeyLogicTimerID) {
    HandleKeyLogic();
  } else if (int index = (int) id - kNumLockTimerID; IsValidIndex(index)) {
    auto& context = contexts_[index];
    auto lock_state = context->GetCurrentLockState();
    if (lock_state != context->GetAutoRestoreState()) {
      keybd_event(INDICATOR_KEYS[index], 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
      keybd_event(INDICATOR_KEYS[index], 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
  }
  return 0;
}

LRESULT MainWindow::OnTrayIcon(UINT, WPARAM id, LPARAM l_param, BOOL&) {
  auto message = LOWORD(l_param);
  if (message == WM_RBUTTONUP) {
    // 显示右键菜单
    SetForegroundWindow(m_hWnd);
    POINT popup_point;
    GetCursorPos(&popup_point);
    HMENU menu_file = GetSubMenu(menu_handle_, 0);

    // 更新开机启动菜单状态
    uint32_t checked_state = AutoStartManager::CheckAutoStart() ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem(menu_file, ID_FILE_START, checked_state);

    if (int index = (int) id - kNotifyNumLockID; IsValidIndex(index)) {
      ModifyMenu(menu_file, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, menu_text_list_[index]);
      SHORT key_state = GetKeyState(INDICATOR_KEYS[index]);
      CheckMenuItem(menu_file, ID_FILE_ABOUT, key_state ? MF_CHECKED : MF_UNCHECKED);

      TrackPopupMenu(menu_file, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERTICAL, popup_point.x, popup_point.y, 0, m_hWnd, nullptr);
    }
  } else if (message == WM_LBUTTONUP) {
    if (int index = (int) id - kNotifyNumLockID; IsValidIndex(index)) {
      contexts_[index]->DisplayIndicator();
    }
  }
  return 0;
}

LRESULT MainWindow::OnKeyboardHook(UINT, WPARAM vk_code, LPARAM flags, BOOL&) {
  // 停止自动恢复定时器
  if (!(flags & LLKHF_INJECTED)) { // 非注入按键
    for (auto& context: contexts_) {
      context->StopAutoRestoreTimer();
    }
  }
  bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  if (alt_down) {
    if (vk_code == '0') {
      ShowWindowAsync(SW_HIDE);
    } else {
      auto index = vk_code - '1';
      if (IsValidIndex(index)) {
        contexts_[index]->DisplayIndicator();
      }
    }
  }
  for (size_t i = 0; i < std::size(INDICATOR_KEYS); i++) {
    if (vk_code == INDICATOR_KEYS[i]) {
      indicator_index_queue_.emplace(i);
    }
  }

  SetTimer(kHookKeyLogicTimerID, 10, nullptr);
  return 0;
}

LRESULT MainWindow::OnCommand(WORD, WORD id, HWND, BOOL&) {
  switch (id) {
    case ID_FILE_MODIFY:
      ModifyAppSettings();
      break;
    case ID_FILE_START:
      ToggleAutoStart();
      break;
    case ID_FILE_EXIT:
      DestroyWindow();
      break;
  }
  return 0;
}

void MainWindow::ModifyAppSettings() {
  // 提取资源文件
  const struct ResourceFile {
    uint32_t resource_id;
    const TCHAR* file_name;
  } resource_files[] = {{IDB_NUMLOCKON, TEXT("NumLock_On.png")},       {IDB_CAPSLOCKON, TEXT("CapsLock_On.png")},
                        {IDB_SCROLLLOCKON, TEXT("ScrollLock_On.png")}, {IDB_NUMLOCKOFF, TEXT("NumLock_Off.png")},
                        {IDB_CAPSLOCKOFF, TEXT("CapsLock_Off.png")},   {IDB_SCROLLLOCKOFF, TEXT("ScrollLock_Off.png")},
                        {IDR_APPSETTINGS, TEXT("AppSettings.ini")}};

  for (const auto& res_file: resource_files) {
    const TCHAR* resource_type = (res_file.resource_id == IDR_APPSETTINGS) ? RT_RCDATA : TEXT("PNG");
    ResourceManager::ExtractResourceToFile(res_file.resource_id, resource_type, res_file.file_name);
  }

  // 打开配置文件
  ShellExecute(nullptr, nullptr, CONFIG_NAME, nullptr, nullptr, SW_SHOW);
}

void MainWindow::ToggleAutoStart() {
  HMENU menu_file = GetSubMenu(menu_handle_, 0);
  uint32_t checked_state = CheckMenuItem(menu_file, ID_FILE_START, 0);

  if (checked_state == MF_CHECKED) {
    AutoStartManager::RemoveAutoStart();
    CheckMenuItem(menu_file, ID_FILE_START, MF_UNCHECKED);
  } else if (checked_state == MF_UNCHECKED) {
    AutoStartManager::EnableAutoStart();
    CheckMenuItem(menu_file, ID_FILE_START, MF_CHECKED);
  }
}

void MainWindow::HandleKeyLogic() {
  while (!indicator_index_queue_.empty()) {
    auto index = indicator_index_queue_.front();
    indicator_index_queue_.pop();
    auto& context = contexts_[index];
    context->HandleIndicator();
  }
  for (auto& context: contexts_) {
    auto lock_state = context->GetCurrentLockState();
    // 安排自动恢复定时器
    context->ScheduleAutoRestore(lock_state);
  }
}
