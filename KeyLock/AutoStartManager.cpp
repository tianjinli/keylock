#include "AutoStartManager.h"

bool AutoStartManager::CheckAutoStart() {
  HKEY reg_key = OpenRunKey(KEY_READ);
  if (!reg_key) {
    return false;
  }

  bool result = false;
  DWORD reg_type = 0;
  TCHAR reg_value[MAX_PATH]{0};
  DWORD reg_value_len = _countof(reg_value) * sizeof(TCHAR);

  LSTATUS status = RegQueryValueEx(reg_key, PACKAGE_NAME, nullptr, &reg_type, (LPBYTE) reg_value, &reg_value_len);

  if (status == ERROR_SUCCESS) {
    CString module_path;
    if (GetModulePath(module_path)) {
      result = ComparePaths(module_path, reg_value);
    }
  }

  RegCloseKey(reg_key);
  return result;
}

bool AutoStartManager::EnableAutoStart() {
  HKEY reg_key = OpenRunKey(KEY_SET_VALUE);
  if (!reg_key) {
    return false;
  }

  bool result = false;
  CString module_path;
  if (GetModulePath(module_path)) {
    LSTATUS status = RegSetValueEx(reg_key, PACKAGE_NAME, 0, REG_SZ, (LPBYTE) (LPCTSTR) module_path, module_path.GetLength() * sizeof(TCHAR));
    result = (status == ERROR_SUCCESS);
  }

  RegCloseKey(reg_key);
  return result;
}

bool AutoStartManager::RemoveAutoStart() {
  HKEY reg_key = OpenRunKey(KEY_SET_VALUE);
  if (!reg_key) {
    return false;
  }

  LSTATUS status = RegDeleteValue(reg_key, PACKAGE_NAME);
  RegCloseKey(reg_key);
  return (status == ERROR_SUCCESS);
}

HKEY AutoStartManager::OpenRunKey(uint32_t access) {
  HKEY reg_key = nullptr;
  LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, AUTO_START_KEY_PATH, 0, access, &reg_key);
  return (status == ERROR_SUCCESS) ? reg_key : nullptr;
}

bool AutoStartManager::GetModulePath(CString& module_path) {
  TCHAR path[MAX_PATH];
  uint32_t length = GetModuleFileName(nullptr, path, _countof(path));
  if (length > 0) {
    module_path = path;
    return true;
  }
  return false;
}

bool AutoStartManager::GetModuleFolder(CString& module_folder) {
  CString module_path;
  if (GetModulePath(module_path)) {
    module_folder = module_path.Left(module_path.ReverseFind(L'\\'));
    return true;
  }
  return false;
}

bool AutoStartManager::ComparePaths(const CString& path1, const CString& path2) {
  return (_tcsicmp(path1, path2) == 0);
}
