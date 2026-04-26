#include "Application.h"

#include <gdiplus.h>
#include <ShellScalingApi.h>

#include "AutoStartManager.h"

Application::Application(HINSTANCE instance) : instance_(instance), gdiplus_token_(0) {
}

Application::~Application() {
  ShutdownGdiPlus();
}

int Application::Run() {
  if (!CheckSingleInstance()) {
    return 0;
  }

  if (!InitializeGdiPlus()) {
    return GetLastError();
  }

  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  CString module_folder;
  if (!AutoStartManager::GetModuleFolder(module_folder)) {
    return GetLastError();
  }
  SetCurrentDirectory(module_folder);

  auto main_window = MainWindow();
  if (!main_window.Create(nullptr)) {
    return GetLastError();
  }

  main_window.ShowWindow(SW_SHOW);

  // 消息循环
  MSG msg{};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (!main_window.IsDialogMessage(&msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}

bool Application::InitializeGdiPlus() {
  Gdiplus::GdiplusStartupInput gdi_startup_input;
  return Gdiplus::GdiplusStartup(&gdiplus_token_, &gdi_startup_input, nullptr) == Gdiplus::Ok;
}

void Application::ShutdownGdiPlus() {
  if (gdiplus_token_) {
    Gdiplus::GdiplusShutdown(gdiplus_token_);
    gdiplus_token_ = 0;
  }
}

bool Application::CheckSingleInstance() {
  HANDLE singleton_mutex = CreateMutex(nullptr, FALSE, SINGLETON_MUTEX_NAME);
  if (!singleton_mutex) {
    return false;
  }

  uint32_t wait_result = WaitForSingleObject(singleton_mutex, 0);
  if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
    // 保持互斥量直到程序结束
    return true;
  }

  CloseHandle(singleton_mutex);
  return false;
}
