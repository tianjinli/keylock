#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <atlstr.h>

#include <memory>

#include "SimpleIni.h"

#include "resource.h"

/**
 * 运行时参数列表
 */
typedef struct {
  UINT resource_id; // 内部资源ID
  POINT position; // 桌面图像显示位置
  std::unique_ptr<Gdiplus::Image> lock_desk_image; // 锁定时桌面图像
  std::unique_ptr<Gdiplus::Image> unlock_desk_image; // 未锁定时桌面图像
  SHORT lock_set; // 当前锁定状态ID
  SHORT showing; // 桌面图像显示时间
  SHORT timeout; // 自动开启/关闭时间
  NOTIFYICONDATA lock_tray_icon; // 锁定时托盘图标
  NOTIFYICONDATA unlock_tray_icon; // 未锁定时托盘图标
  TCHAR* key_node; // 按键节点
  bool enable_mute; // 静音状态
  CString sound_on; // 锁定时音效
  CString sound_off; // 未锁定时音效
  UINT_PTR id_event; // 计时器ID
} RUNTIMEPARAMS;

// 绘制桌面图像与托盘图标
VOID DrawImageIcon(RUNTIMEPARAMS* runtime_params, SHORT current_lock_set = 0);

std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const TCHAR* file_path, HINSTANCE instance, UINT resource_id);

// 从 INI 文件读取语言
VOID LoadLanguage(const std::unique_ptr<CSimpleIni>& ini);

// 从 INI 文件读取参数
VOID LoadParameters(const std::unique_ptr<CSimpleIni>& ini, RUNTIMEPARAMS* runtime_params);

// 指示器窗口处理函数
INT_PTR CALLBACK IndicatorWndProc(HWND dialog_handle, UINT message, WPARAM w_param, LPARAM l_param);

// 0: 检查开机启动 1: 设置开机启动 2: 取消开机启动
BOOL CurrentVersionRun(UINT message);
