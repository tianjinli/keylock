#pragma once

#include <windows.h>

#include <memory>

#include "MainWindow.h"

/**
 * @brief 应用程序类
 * 应用程序的主入口点，负责初始化和运行
 */
class Application {
public:
  Application(HINSTANCE instance);
  ~Application();

  // 禁用拷贝构造和赋值
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  // 移动构造和赋值
  Application(Application&&) noexcept = default;
  Application& operator=(Application&&) noexcept = default;

  // 应用程序运行
  int Run();

private:
  HINSTANCE instance_;

  // GDI+ 初始化
  bool InitializeGdiPlus();
  void ShutdownGdiPlus();

  // 单实例检查
  bool CheckSingleInstance();

  ULONG_PTR gdiplus_token_;
};
