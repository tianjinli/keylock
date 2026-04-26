#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <atlstr.h>

#include <memory>

#include "SimpleIni.h"

#include "resource.h"

// 运行时参数上下文
typedef struct {
  UINT lock_resource_id; // 锁定时资源ID
  UINT unlock_resource_id; // 未锁定时资源ID
  POINT hud_position; // 桌面图像显示位置
  std::unique_ptr<Gdiplus::Image> lock_hud_image; // 锁定时HUD图像
  std::unique_ptr<Gdiplus::Image> unlock_hud_image; // 未锁定时HUD图像
  SHORT indicator_status; // 当前指示器显示状态
  SHORT hud_display_duration; // HUD显示持续时间
  SHORT auto_restore_state; // 自动恢复锁定状态
  SHORT auto_restore_delay; // 自动恢复延时时间
  NOTIFYICONDATA lock_tray_icon; // 锁定时托盘图标
  NOTIFYICONDATA unlock_tray_icon; // 未锁定时托盘图标
  const TCHAR* indicator_key; // 指示器按键
  bool sound_muted; // 音效是否静音
  CString lock_key_sound; // 锁定时音效
  CString unlock_key_sound; // 未锁定时音效
  UINT_PTR auto_restore_timer_id; // 自动恢复计时器ID
} IndicatorContext;

// 绘制桌面图像与托盘图标
VOID DrawImageIcon(IndicatorContext* context, SHORT current_lock_set = 0);

std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const TCHAR* file_path, HINSTANCE instance, UINT resource_id);

// 从 INI 文件读取语言
VOID LoadMenuText(const std::unique_ptr<CSimpleIni>& ini);

// 从 INI 文件读取参数
VOID LoadParameters(const std::unique_ptr<CSimpleIni>& ini, uint32_t index);

// 指示器窗口处理函数
INT_PTR CALLBACK IndicatorWndProc(HWND dialog_handle, UINT message, WPARAM w_param, LPARAM l_param);

// 0: 检查开机启动 1: 设置开机启动 2: 取消开机启动
BOOL CurrentVersionRun(UINT message);
