#pragma once

#include <iterator>

#include "resource.h"

// 锁定状态枚举
enum class LockState : SHORT {
  None = 0,
  Lock = 1,
  Unlock = 2,
};

// 键盘按键枚举
enum class KeyType {
  NumLock,
  CapsLock,
  ScrollLock,
};

// 虚拟键码定义
constexpr int INDICATOR_KEYS[] = {VK_NUMLOCK, VK_CAPITAL, VK_SCROLL};

// 指示器按键名称
constexpr const TCHAR* INDICATOR_NAMES[] = {TEXT("NumLock"), TEXT("CapsLock"), TEXT("ScrollLock")};

// 资源ID定义
constexpr uint32_t RESOURCE_IDS[][2] = {{IDB_NUMLOCKON, IDB_NUMLOCKOFF}, {IDB_CAPSLOCKON, IDB_CAPSLOCKOFF}, {IDB_SCROLLLOCKON, IDB_SCROLLLOCKOFF}};

// 窗口消息定义
constexpr uint32_t WM_NOTIFY_NUMLOCK = WM_APP + 1;
constexpr uint32_t WM_NOTIFY_CAPSLOCK = WM_APP + 2;
constexpr uint32_t WM_NOTIFY_SCROLLLOCK = WM_APP + 3;

//// 定时器ID定义
constexpr uint32_t kNumLockTimerID = WM_USER + 1;
constexpr uint32_t kCapsLockTimerID = WM_USER + 2;
constexpr uint32_t kScrollLockTimerID = WM_USER + 3;

constexpr uint32_t kHudAutoHideTimerID = WM_APP + 1;
constexpr uint32_t kHookKeyLogicTimerID = WM_APP + 2;

// 托盘图标ID定义
constexpr uint32_t kNotifyNumLockID = WM_USER + 1;
constexpr uint32_t kNotifyCapsLockID = WM_USER + 2;
constexpr uint32_t kNotifyScrollLockID = WM_USER + 3;

// 托盘事件ID定义
#define UM_NOTIFY_NUMLOCK (WM_APP + 1)
#define UM_NOTIFY_CAPSLOCK (WM_APP + 2)
#define UM_NOTIFY_SCROLL (WM_APP + 3)

#define UM_KEYBOARD_HOOK (WM_APP + 4)

// 秒数转毫秒数
constexpr uint32_t SECONDS_TO_MILLISECONDS(uint32_t seconds) {
  return seconds * 1000;
}

// 字符串常量
constexpr const TCHAR* PACKAGE_NAME = TEXT("HNSFNETKL");
constexpr const TCHAR* PROP_VKCODE = TEXT("VKCODE");
constexpr const TCHAR* CONFIG_NAME = TEXT("AppSettings.ini");
constexpr const TCHAR* SINGLETON_MUTEX_NAME = TEXT("COM.HNSFNET.KEYLOCK");
constexpr const TCHAR* AUTO_START_KEY_PATH = TEXT(R"(Software\Microsoft\Windows\CurrentVersion\Run)");
