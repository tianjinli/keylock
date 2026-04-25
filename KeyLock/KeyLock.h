#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <atlstr.h>

#include <memory>

#include "resource.h"

/**
 * 运行时参数列表
 */
typedef struct {
  UINT ResourceID; // 内部资源ID
  POINT Position; // 桌面图像显示位置
  std::unique_ptr<Gdiplus::Image> LockDeskImage; // 锁定时桌面图像
  std::unique_ptr<Gdiplus::Image> UnlockDeskImage; // 未锁定时桌面图像
  SHORT LockSet; // 当前锁定状态ID
  SHORT Showing; // 桌面图像显示时间
  SHORT Timeout; // 自动开启/关闭时间
  NOTIFYICONDATA LockTrayIcon; // 锁定时托盘图标
  NOTIFYICONDATA UnlockTrayIcon; // 未锁定时托盘图标
  TCHAR* KeyNode; // 按键节点
  bool EnableMute; // 静音状态
  CString SoundOn; // 锁定时音效
  CString SoundOff; // 未锁定时音效
  UINT_PTR IDEvent; // 计时器ID
} RUNTIMEPARAMS;

// 绘制桌面图像与托盘图标
VOID DrawImageIcon(RUNTIMEPARAMS* pRuntimeParams, SHORT wCurrentLockSet = 0);

std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const TCHAR* lpFilePath, HINSTANCE hInstance, UINT nResourceID);

// 从 INI 文件读取参数
VOID LoadParameters(CONST TCHAR* lpCurrentDirectory, RUNTIMEPARAMS* pRuntimeParams);

// 从 INI 文件读取语言
VOID LoadLanguage(CONST TCHAR* lpCurrentDirectory);

// 指示器窗口处理函数
INT_PTR CALLBACK IndicatorWndProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 0: 检查开机启动 1: 设置开机启动 2: 取消开机启动
BOOL CurrentVersionRun(UINT uMsg);
