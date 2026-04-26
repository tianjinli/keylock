#pragma once

#include <atlstr.h>

#include "Constants.h"

/**
 * @brief 开机启动管理器类
 * 负责检查、设置和取消开机启动
 */
struct AutoStartManager {
  // 开机启动管理
  static bool CheckAutoStart();
  static bool EnableAutoStart();
  static bool RemoveAutoStart();
  static bool GetModulePath(CString& module_path);
  static bool GetModuleFolder(CString& module_folder);

private:
  // 注册表操作
  static HKEY OpenRunKey(uint32_t access);
  static bool ComparePaths(const CString& path1, const CString& path2);
};
