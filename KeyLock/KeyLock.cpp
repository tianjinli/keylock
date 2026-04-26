#include "KeyLock.h"

#include <atlstr.h>
#include <atltypes.h>
#include <shlwapi.h>

#include <fstream>
#include <unordered_set>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

#define PACKAGE_NAME TEXT("HNSFNETKL")

#define PROP_VKCODE TEXT("VKCODE")

#define CONFIG_NAME TEXT("AppSettings.ini")

// 检查开机启动ID
#define IDACTION_CHECK (WM_USER + 0)
// 添加开机启动ID
#define IDACTION_APPEND (WM_USER + 1)
// 取消开机启动ID
#define IDACTION_CANCEL (WM_USER + 2)

// 数字超时事件ID
#define IDNUM_TIMEOUT (WM_USER + 1)
// 大写超时事件ID
#define IDCAPS_TIMEOUT (WM_USER + 2)
// 显示超时事件ID
#define IDSHOW_TIMEOUT (WM_USER + 3)
// 键盘钩子处理事件ID
#define IDHOOK_TIMEOUT (WM_USER + 4)

// 数字锁定托盘图标ID
#define IDNOTIFY_NUMLOCK (WM_USER + 1)
// 大写锁定托盘图标ID
#define IDNOTIFY_CAPSLOCK (WM_USER + 2)

// 数字锁定托盘事件ID
#define UMNOTIFY_NUMLOCK (WM_APP + 1)
// 大写锁定托盘事件ID
#define UMNOTIFY_CAPSLOCK (WM_APP + 2)

// 秒数转毫秒数
#define MILISECONDS(SECONDS) (SECONDS * 1000)

// 键盘处理钩子
static HHOOK keyboard_hook{nullptr};

// 数字锁定键配置
static RUNTIMEPARAMS num_lock_setup{IDB_NUMLOCK};
// 大写锁定键配置
static RUNTIMEPARAMS caps_lock_setup{IDB_CAPSLOCK};

// 大写锁定计时器
static UINT_PTR caps_timer_id;

// 指示器窗口句柄
static HWND window_indicator;

// 指示器托盘菜单
static HMENU menu_indicator;

// 数字键锁菜单文字
static CString file_num_text;
// 大写键锁菜单文字
static CString file_caps_text;

// 当前工作目录
static TCHAR work_dir[MAX_PATH];

// 临时配置文件路径（需要清理）
static CString temp_config_path;

static int last_vk_code;

void KeyLockLogic(int vk_code) {
  bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

  // 使用 GetKeyState 更可靠（钩子场景下推荐）
  SHORT num_lock_set = (GetKeyState(VK_NUMLOCK) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;
  SHORT caps_lock_set = (GetKeyState(VK_CAPITAL) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;

  // 查看数字/大写锁定状态
  if (alt_down && vk_code == '1') {
    DrawImageIcon(&num_lock_setup);
    return;
  }
  if (alt_down && vk_code == '2') {
    DrawImageIcon(&caps_lock_setup);
    return;
  }

  // 启动超时打开数字锁定
  if (num_lock_set == LSFW_UNLOCK && num_lock_setup.id_event == 0 && num_lock_setup.timeout > 0) {
    num_lock_setup.id_event = SetTimer(window_indicator, IDNUM_TIMEOUT, MILISECONDS(num_lock_setup.timeout), nullptr);
  }
  if (vk_code == VK_NUMLOCK) {
    DrawImageIcon(&num_lock_setup, num_lock_set);
  }

  // 启动超时关闭大写锁定
  if (caps_lock_set == LSFW_LOCK && caps_lock_setup.id_event == 0 && caps_lock_setup.timeout > 0) {
    caps_lock_setup.id_event = SetTimer(window_indicator, IDCAPS_TIMEOUT, MILISECONDS(caps_lock_setup.timeout), nullptr);
  }
  if (vk_code == VK_CAPITAL) {
    DrawImageIcon(&caps_lock_setup, caps_lock_set);
  }
}

// 键盘钩子处理函数
LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param) {
  if (code == HC_ACTION) {
    auto vk_code = ((KBDLLHOOKSTRUCT*) l_param)->vkCode;
    auto flags = ((KBDLLHOOKSTRUCT*) l_param)->flags;

    if (w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN) {
      if (!(flags & LLKHF_INJECTED)) {
        // 任意按键都清除定时器
        if (num_lock_setup.id_event) {
          KillTimer(window_indicator, num_lock_setup.id_event);
          num_lock_setup.id_event = 0;
        }
        if (caps_lock_setup.id_event) {
          KillTimer(window_indicator, caps_lock_setup.id_event);
          caps_lock_setup.id_event = 0;
        }
      }
      SetProp(window_indicator, PROP_VKCODE, (HANDLE) (INT_PTR) vk_code);
      SetTimer(window_indicator, IDHOOK_TIMEOUT, 10, nullptr);
    }
  }

  return CallNextHookEx(keyboard_hook, code, w_param, l_param);
}

std::unique_ptr<CSimpleIni> LoadConfigWithFallback(const TCHAR* file_path, HINSTANCE instance, UINT resource_id) {
  auto ini = std::make_unique<CSimpleIni>();
  SI_Error rc = ini->LoadFile(file_path);
  if (rc != SI_OK) {
    HRSRC resource_handle = FindResource(instance, MAKEINTRESOURCE(resource_id), RT_RCDATA);
    if (!resource_handle)
      return nullptr;

    HGLOBAL resource_data_handle = LoadResource(instance, resource_handle);
    if (!resource_data_handle)
      return nullptr;

    void* resource_data = LockResource(resource_data_handle);
    DWORD resource_size = SizeofResource(instance, resource_handle);
    if (!resource_data || resource_size == 0)
      return nullptr;
    rc = ini->LoadData((const char*) resource_data, resource_size);
    if (rc != SI_OK)
      return nullptr;
  }
  return ini;
}

// 清理托盘图标
VOID CleanupTrayIcons(RUNTIMEPARAMS* runtime_params) {
  if (runtime_params->lock_tray_icon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &runtime_params->lock_tray_icon);
    DestroyIcon(runtime_params->lock_tray_icon.hIcon);
    runtime_params->lock_tray_icon.hIcon = nullptr;
  }
  if (runtime_params->unlock_tray_icon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &runtime_params->unlock_tray_icon);
    DestroyIcon(runtime_params->unlock_tray_icon.hIcon);
    runtime_params->unlock_tray_icon.hIcon = nullptr;
  }
}

// 清理所有资源
VOID CleanupResources() {
  CleanupTrayIcons(&num_lock_setup);
  CleanupTrayIcons(&caps_lock_setup);

  // ⭐ 主动释放 GDI+ 相关对象，确保在 GdiplusShutdown 之前释放
  num_lock_setup.lock_desk_image.reset();
  num_lock_setup.unlock_desk_image.reset();
  caps_lock_setup.lock_desk_image.reset();
  caps_lock_setup.unlock_desk_image.reset();

  if (menu_indicator) {
    DestroyMenu(menu_indicator);
    menu_indicator = nullptr;
  }

  if (keyboard_hook) {
    UnhookWindowsHookEx(keyboard_hook);
    keyboard_hook = nullptr;
  }

  // 删除临时配置文件
  if (!temp_config_path.IsEmpty() && PathFileExists(temp_config_path)) {
    DeleteFile(temp_config_path);
    temp_config_path.Empty();
  }

  // 注销窗口类
  UnregisterClass(TEXT("KEYLOCK"), GetModuleHandle(nullptr));
}

VOID DrawImageIcon(RUNTIMEPARAMS* runtime_params, SHORT current_lock_set) {
  Gdiplus::Image* desk_image = nullptr;
  NOTIFYICONDATA* tray_icon = nullptr;
  SHORT target_lock_set = current_lock_set;
  if (current_lock_set == 0) {
    target_lock_set = (runtime_params == &num_lock_setup) ? (GetKeyState(VK_NUMLOCK) & 0x0001 ? LSFW_LOCK : LSFW_UNLOCK)
                                                          : (GetKeyState(VK_CAPITAL) & 0x0001 ? LSFW_LOCK : LSFW_UNLOCK);
  } else if (runtime_params->lock_set == current_lock_set) {
    return; // 状态没变直接返回
  } else {
    runtime_params->lock_set = current_lock_set;
  }

  CString sound_effect;
  if (target_lock_set == LSFW_LOCK) {
    desk_image = runtime_params->lock_desk_image.get();
    tray_icon = &runtime_params->lock_tray_icon;
    sound_effect = runtime_params->sound_on;
  } else {
    desk_image = runtime_params->unlock_desk_image.get();
    tray_icon = &runtime_params->unlock_tray_icon;
    sound_effect = runtime_params->sound_off;
  }

  if (current_lock_set && !runtime_params->enable_mute) { // 单击托盘不播放音效
    if (sound_effect.IsEmpty()) {
      switch (current_lock_set) {
        case LSFW_LOCK:
          Beep(900, 60); // 高频，短促
          break;
        case LSFW_UNLOCK:
          Beep(500, 60); // 低频，柔和
          break;
        default:
          break;
      }
    } else {
      CString command_string;
      command_string.Format(TEXT("play %s from 0"), sound_effect);
      mciSendString(command_string, nullptr, 0, nullptr);
    }
  }

  if (desk_image) {
    LONG width = desk_image->GetWidth();
    LONG height = desk_image->GetHeight();

    HDC screen_dc = GetDC(nullptr);
    HDC memory_dc = CreateCompatibleDC(screen_dc);
    HBITMAP new_bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    HBITMAP old_bitmap = (HBITMAP) SelectObject(memory_dc, new_bitmap);

    Gdiplus::Graphics graphics(memory_dc);

    graphics.DrawImage(desk_image, 0, 0, width, height);

    BLENDFUNCTION blend_function{0};
    blend_function.BlendOp = AC_SRC_OVER;
    blend_function.SourceConstantAlpha = 0xff;
    blend_function.AlphaFormat = AC_SRC_ALPHA;
    CSize size_struct(width, height);
    CPoint src_point(0, 0);
    UpdateLayeredWindow(window_indicator, screen_dc, &runtime_params->position, &size_struct, memory_dc, &src_point, 0, &blend_function, ULW_ALPHA);

    ShowWindowAsync(window_indicator, SW_SHOW);

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(new_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);

    if (runtime_params->showing > 0) {
      SetTimer(window_indicator, IDSHOW_TIMEOUT, MILISECONDS(runtime_params->showing), nullptr);
    }
  }

  if (tray_icon && tray_icon->hIcon) {
    Shell_NotifyIcon(NIM_MODIFY, tray_icon);
  }
}

std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const TCHAR* file_path, HINSTANCE instance, UINT resource_id) {
  {
    std::unique_ptr<Gdiplus::Image> file_image(Gdiplus::Image::FromFile(file_path));
    if (file_image && file_image->GetLastStatus() == Gdiplus::Ok) {
      return std::unique_ptr<Gdiplus::Image>(file_image.release()); // 转交给 shared_ptr
    }
  }

  HRSRC resource_handle = FindResource(instance, MAKEINTRESOURCE(resource_id), L"PNG");
  if (!resource_handle)
    return nullptr;

  HGLOBAL resource_data_handle = LoadResource(instance, resource_handle);
  if (!resource_data_handle)
    return nullptr;

  void* resource_data = LockResource(resource_data_handle);
  DWORD resource_size = SizeofResource(instance, resource_handle);
  if (!resource_data || resource_size == 0)
    return nullptr;

  HGLOBAL resource_buffer_handle = GlobalAlloc(GMEM_MOVEABLE, resource_size);
  if (!resource_buffer_handle)
    return nullptr;

  std::unique_ptr<void, void (*)(void*)> buffer_ptr(resource_buffer_handle, [](void* buffer) {
    GlobalFree(buffer);
  });

  void* resource_buffer = GlobalLock(resource_buffer_handle);
  memcpy(resource_buffer, resource_data, resource_size);
  GlobalUnlock(resource_buffer_handle);

  IStream* raw_stream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(resource_buffer_handle, FALSE, &raw_stream)))
    return nullptr;

  std::unique_ptr<IStream, void (*)(IStream*)> stream_ptr(raw_stream, [](IStream* stream) {
    stream->Release();
  });

  std::unique_ptr<Gdiplus::Image> file_image(Gdiplus::Image::FromStream(stream_ptr.get()));
  if (file_image && file_image->GetLastStatus() == Gdiplus::Ok) {
    return std::unique_ptr<Gdiplus::Image>(file_image.release());
  }

  return nullptr;
}

VOID LoadLanguage(const std::unique_ptr<CSimpleIni>& ini) {
  ATLASSERT(menu_indicator != nullptr);

  CONST TCHAR* language_section = TEXT("Language");
  HMENU menu_file = GetSubMenu(menu_indicator, 0);

  file_num_text = ini->GetValue(language_section, TEXT("FileNum"), TEXT("Num Lock"));
  file_caps_text = ini->GetValue(language_section, TEXT("FileCaps"), TEXT("Caps Lock"));

  auto file_modify_text = ini->GetValue(language_section, TEXT("FileModify"), TEXT("Modify Settings"));
  ModifyMenu(menu_file, ID_FILE_MODIFY, MF_BYCOMMAND, ID_FILE_MODIFY, file_modify_text);

  auto file_start_text = ini->GetValue(language_section, TEXT("FileStart"), TEXT("Auto Start"));
  ModifyMenu(menu_file, ID_FILE_START, MF_BYCOMMAND, ID_FILE_START, file_start_text);

  auto file_exit_text = ini->GetValue(language_section, TEXT("FileExit"), TEXT("Quick Exit"));
  ModifyMenu(menu_file, ID_FILE_EXIT, MF_BYCOMMAND, ID_FILE_EXIT, file_exit_text);
}

VOID LoadParameters(const std::unique_ptr<CSimpleIni>& ini, RUNTIMEPARAMS* runtime_params) {
  ATLASSERT(window_indicator != nullptr);

  BOOL num_lock_loading = (runtime_params == &num_lock_setup); // 数字锁定参数加载中
  UINT tray_id = num_lock_loading ? IDNOTIFY_NUMLOCK : IDNOTIFY_CAPSLOCK;
  UINT tray_um = num_lock_loading ? UMNOTIFY_NUMLOCK : UMNOTIFY_CAPSLOCK;
  SHORT key_state = num_lock_loading ? GetKeyState(VK_NUMLOCK) : GetKeyState(VK_CAPITAL);
  runtime_params->key_node = num_lock_loading ? const_cast<TCHAR*>(TEXT("NumLock")) : const_cast<TCHAR*>(TEXT("CapsLock"));

  // # Lock 与 Unlock 图标位置(默认左 30px 上 30px)
  auto position = ini->GetValue(runtime_params->key_node, TEXT("Position"), TEXT("0,0"));
  _stscanf_s(position, TEXT("%d,%d"), &runtime_params->position.x, &runtime_params->position.y);

  // 对齐方式(HL: 水平居左 HC: 水平居中 HR: 水平居右 VT: 垂直居上 VC: 垂直居中 VB: 垂直居下)
  int start = 0;
  CString alignment = ini->GetValue(runtime_params->key_node, TEXT("Alignment"), TEXT("HC|VC"));
  CString contexts[2] = {alignment.Tokenize(TEXT("|"), start), alignment.Tokenize(TEXT("|"), start)};
  // 开启静音状态
  runtime_params->enable_mute = ini->GetBoolValue(runtime_params->key_node, TEXT("EnableMute"), true);

  // # Lock 与 Unlock 置顶图标(左 Lock 右 Unlock)
  auto desk_image_path = ini->GetValue(runtime_params->key_node, TEXT("DeskImage"), (CString(runtime_params->key_node) + CString(".png")));
  std::unique_ptr<Gdiplus::Image> desk_image_shared = LoadImageWithFallback(desk_image_path, GetModuleHandle(nullptr), runtime_params->resource_id);
  if (desk_image_shared != nullptr) {
    int desk_image_width = desk_image_shared->GetWidth() / 2;
    int desk_image_height = desk_image_shared->GetHeight();

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    for (size_t i = 0; i < _countof(contexts); i++) {
      contexts[i].Trim().MakeUpper();
      if (TEXT("HC") == contexts[i]) {
        runtime_params->position.x += (screen_width - desk_image_width) / 2;
      } else if (TEXT("HR") == contexts[i]) {
        runtime_params->position.x += (screen_width - desk_image_width);
      } else if (TEXT("VC") == contexts[i]) {
        runtime_params->position.y += (screen_height - desk_image_height) / 2;
      } else if (TEXT("VB") == contexts[i]) {
        runtime_params->position.y += (screen_height - desk_image_height);
      }
    }

    runtime_params->lock_desk_image = std::unique_ptr<Gdiplus::Image>(new Gdiplus::Bitmap(desk_image_width, desk_image_height, PixelFormat32bppARGB));
    runtime_params->unlock_desk_image =
        std::unique_ptr<Gdiplus::Image>(new Gdiplus::Bitmap(desk_image_width, desk_image_height, PixelFormat32bppARGB));
    std::shared_ptr<Gdiplus::Graphics> pgDeskImageLock(Gdiplus::Graphics::FromImage(runtime_params->lock_desk_image.get()));
    std::shared_ptr<Gdiplus::Graphics> pgDeskImageUnlock(Gdiplus::Graphics::FromImage(runtime_params->unlock_desk_image.get()));

    pgDeskImageLock->DrawImage(desk_image_shared.get(), Gdiplus::Rect(0, 0, desk_image_width, desk_image_height), 0, 0, desk_image_width,
                               desk_image_height, Gdiplus::Unit::UnitPixel);
    pgDeskImageUnlock->DrawImage(desk_image_shared.get(), Gdiplus::Rect(0, 0, desk_image_width, desk_image_height), desk_image_width, 0,
                                 desk_image_width, desk_image_height, Gdiplus::Unit::UnitPixel);
  }
  // # Lock 与 Unlock 窗口隐藏时间(单位：秒)
  runtime_params->showing = (SHORT) ini->GetLongValue(runtime_params->key_node, TEXT("Showing"), 1);

  // # 大写锁定与数字锁定关闭或开启时间(单位：秒)
  runtime_params->timeout = (SHORT) ini->GetLongValue(runtime_params->key_node, TEXT("Timeout"), 0);

  // # Lock 与 Unlock 托盘图标(左边 Lock 右边 Unlock)
  auto tray_icon_path = ini->GetValue(runtime_params->key_node, TEXT("TrayIcon"), (CString(runtime_params->key_node) + CString(".png")));
  std::shared_ptr<Gdiplus::Image> tray_icon_image = LoadImageWithFallback(tray_icon_path, GetModuleHandle(nullptr), runtime_params->resource_id);
  if (tray_icon_image != nullptr) {
    int tray_icon_width = tray_icon_image->GetWidth() / 2;
    int tray_icon_height = tray_icon_image->GetHeight();
    Gdiplus::Bitmap lock_tray_icon(tray_icon_width, tray_icon_height, PixelFormat32bppARGB);
    Gdiplus::Bitmap unlock_tray_icon(tray_icon_width, tray_icon_height, PixelFormat32bppARGB);
    std::shared_ptr<Gdiplus::Graphics> tray_icon_lock_graphics(Gdiplus::Graphics::FromImage(&lock_tray_icon));
    std::shared_ptr<Gdiplus::Graphics> tray_icon_unlock_graphics(Gdiplus::Graphics::FromImage(&unlock_tray_icon));

    Gdiplus::Rect tray_icon_rect{0, 0, tray_icon_width, tray_icon_height};
    tray_icon_lock_graphics->DrawImage(tray_icon_image.get(), tray_icon_rect, 0, 0, tray_icon_width, tray_icon_height, Gdiplus::Unit::UnitPixel);
    tray_icon_unlock_graphics->DrawImage(tray_icon_image.get(), tray_icon_rect, tray_icon_width, 0, tray_icon_width, tray_icon_height,
                                         Gdiplus::Unit::UnitPixel);

    runtime_params->lock_tray_icon.cbSize = sizeof(NOTIFYICONDATA);
    runtime_params->lock_tray_icon.hWnd = window_indicator;
    runtime_params->lock_tray_icon.uID = tray_id;
    runtime_params->lock_tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    runtime_params->lock_tray_icon.uCallbackMessage = tray_um;
    lock_tray_icon.GetHICON(&runtime_params->lock_tray_icon.hIcon);
    runtime_params->unlock_tray_icon.cbSize = sizeof(NOTIFYICONDATA);
    runtime_params->unlock_tray_icon.hWnd = window_indicator;
    runtime_params->unlock_tray_icon.uID = tray_id;
    runtime_params->unlock_tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    runtime_params->unlock_tray_icon.uCallbackMessage = tray_um;
    unlock_tray_icon.GetHICON(&runtime_params->unlock_tray_icon.hIcon);
  }

  // # Lock 音效(支持 wav mp3 格式)
  auto sound_on_path = ini->GetValue(runtime_params->key_node, TEXT("SoundOn"), (CString(runtime_params->key_node) + CString(TEXT("On.wav"))));

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(sound_on_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(runtime_params->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    runtime_params->sound_on = sound_on_path;
  }

  // # Unlock 音效(支持 wav mp3 格式)
  auto sound_off_path = ini->GetValue(runtime_params->key_node, TEXT("SoundOff"), (CString(runtime_params->key_node) + CString(TEXT("Off.wav"))));

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(sound_off_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(runtime_params->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    runtime_params->sound_off = sound_off_path;
  }

  if (key_state == 1) {
    runtime_params->lock_set = LSFW_LOCK;
    if (runtime_params->lock_tray_icon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &runtime_params->lock_tray_icon);
    }
  } else if (key_state == 0) {
    runtime_params->lock_set = LSFW_UNLOCK;
    if (runtime_params->lock_tray_icon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &runtime_params->unlock_tray_icon);
    }
  }
}

bool ExtractResourceToFile(HINSTANCE instance, UINT resource_id, const TCHAR* resource_type, const CString& out_path) {
  if (PathFileExists(out_path))
    return true;

  HRSRC resource_handle = FindResource(instance, MAKEINTRESOURCE(resource_id), resource_type);
  if (!resource_handle)
    return false;

  HGLOBAL resource_data_handle = LoadResource(instance, resource_handle);
  if (!resource_data_handle)
    return false;

  void* resource_data = LockResource(resource_data_handle);
  DWORD resource_size = SizeofResource(instance, resource_handle);
  if (!resource_data || resource_size == 0)
    return false;

  std::ofstream file(out_path, std::ios::binary);
  if (!file.is_open())
    return false;

  file.write(reinterpret_cast<const char*>(resource_data), resource_size);
  return true;
}

BOOL CurrentVersionRun(UINT message) {
  HKEY reg_key{nullptr};
  LSTATUS result_code = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &reg_key);
  if (result_code == ERROR_SUCCESS) /// 打开启动项
  {
    if (message == IDACTION_CANCEL) {
      result_code = RegDeleteValue(reg_key, PACKAGE_NAME);
    } else {
      TCHAR app_path[MAX_PATH]{0};
      GetModuleFileName(nullptr, app_path, MAX_PATH);
      if (message == IDACTION_APPEND) {
        result_code = RegSetValueEx(reg_key, PACKAGE_NAME, 0, REG_SZ, (LPBYTE) app_path, lstrlen(app_path) * sizeof(TCHAR));
      } else {
        DWORD reg_type = 0;
        TCHAR reg_value[MAX_PATH]{0};
        DWORD reg_value_len = _countof(reg_value) * sizeof(TCHAR);
        result_code = RegQueryValueEx(reg_key, PACKAGE_NAME, nullptr, &reg_type, (LPBYTE) reg_value,
                                      &reg_value_len); // 支持 XP 32 位系统

        if (result_code == ERROR_SUCCESS) {
          if (_tcsicmp(app_path, reg_value) != 0) {
            result_code = (LSTATUS) -1; // 注册表保存的并非当前应用程序路径
          }
        }
      }
    }

    RegCloseKey(reg_key);
  }
  return (result_code == ERROR_SUCCESS);
}

INT_PTR CALLBACK IndicatorWndProc(HWND dialog_handle, UINT message, WPARAM w_param, LPARAM l_param) {
  UNREFERENCED_PARAMETER(l_param);
  switch (message) {
    case WM_INITDIALOG: {
      window_indicator = dialog_handle;

      DWORD extended_style = GetWindowLong(dialog_handle, GWL_EXSTYLE);
      SetWindowLong(dialog_handle, GWL_EXSTYLE, extended_style | WS_EX_NOACTIVATE);

      GetModuleFileName(nullptr, work_dir, MAX_PATH);
      *(_tcsrchr(work_dir, TEXT('\\'))) = 0; // 把最后一个 \ 替换为 0
      SetCurrentDirectory(work_dir);

      CString config_path = CString(work_dir) + CString(TEXT("\\")) + CString(CONFIG_NAME);
      if (auto ini = LoadConfigWithFallback(config_path, GetModuleHandle(nullptr), IDR_APPSETTINGS)) {
        LoadLanguage(ini);
        // 最后加载托盘图标在左边
        LoadParameters(ini, &caps_lock_setup);
        LoadParameters(ini, &num_lock_setup);
      }

      // SetTimer(dialog_handle, IDEVENT_SCANNING, 10, nullptr);
      keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);

      SHORT num_lock_set = GetKeyState(VK_NUMLOCK) ? LSFW_LOCK : LSFW_UNLOCK;
      SHORT caps_lock_set = GetKeyState(VK_CAPITAL) ? LSFW_LOCK : LSFW_UNLOCK;
      // 当前是灯灭状态，就重置/启动计时器
      if (num_lock_set == LSFW_UNLOCK && num_lock_setup.timeout > 0) {
        num_lock_setup.id_event = SetTimer(window_indicator, IDNUM_TIMEOUT, MILISECONDS(num_lock_setup.timeout), nullptr);
      }

      // 当前是灯亮状态，就重置/启动计时器
      if (caps_lock_set == LSFW_LOCK && caps_lock_setup.timeout > 0) {
        caps_lock_setup.id_event = SetTimer(window_indicator, IDCAPS_TIMEOUT, MILISECONDS(caps_lock_setup.timeout), nullptr);
      }
      return (INT_PTR) TRUE;
    }
    case WM_COMMAND: {
      if (w_param == ID_FILE_MODIFY) {
        CString absolute_path;
        absolute_path = CString(work_dir) + CString("\\NumLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_NUMLOCK, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\CapsLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_CAPSLOCK, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\AppSettings.ini");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDR_APPSETTINGS, RT_RCDATA, absolute_path);
        // 打开配置文件
        ShellExecute(nullptr, nullptr, absolute_path, nullptr, nullptr, SW_SHOW);
      } else if (w_param == ID_FILE_START) {
        HMENU menu_file = GetSubMenu(menu_indicator, 0);
        DWORD checked_state = CheckMenuItem(menu_file, ID_FILE_START, 0);
        if (checked_state == MF_CHECKED) {
          CurrentVersionRun(IDACTION_CANCEL);
          CheckMenuItem(menu_file, ID_FILE_START, MF_UNCHECKED);
        } else if (checked_state == MF_UNCHECKED) {
          CurrentVersionRun(IDACTION_APPEND);
          CheckMenuItem(menu_file, ID_FILE_START, MF_CHECKED);
        }
      } else if (w_param == ID_FILE_EXIT) {
        EndDialog(dialog_handle, l_param);
      }
      return (INT_PTR) TRUE;
    }
    case UMNOTIFY_CAPSLOCK:
    case UMNOTIFY_NUMLOCK: {
      if (LOWORD(l_param) == WM_RBUTTONUP) {
        SetForegroundWindow(dialog_handle); // 没有这个菜单不会自动消失

        POINT popup_point;
        GetCursorPos(&popup_point);
        HMENU menu_file = GetSubMenu(menu_indicator, 0);
        CheckMenuItem(menu_file, ID_FILE_START, CurrentVersionRun(IDACTION_CHECK) ? MF_CHECKED : MF_UNCHECKED);
        if (message == UMNOTIFY_NUMLOCK) {
          ModifyMenu(menu_file, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, file_num_text);
          CheckMenuItem(menu_file, ID_FILE_ABOUT, GetKeyState(VK_NUMLOCK) ? MF_CHECKED : MF_UNCHECKED);
        } else if (message == UMNOTIFY_CAPSLOCK) {
          ModifyMenu(menu_file, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, file_caps_text);
          CheckMenuItem(menu_file, ID_FILE_ABOUT, GetKeyState(VK_CAPITAL) ? MF_CHECKED : MF_UNCHECKED);
        }
        TrackPopupMenu(menu_file, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERTICAL, popup_point.x, popup_point.y, 0, dialog_handle, nullptr);
      } else if (LOWORD(l_param) == WM_LBUTTONUP) {
        DrawImageIcon((message == UMNOTIFY_NUMLOCK) ? &num_lock_setup : &caps_lock_setup);
      }
      return (INT_PTR) TRUE;
    }
    case WM_TIMER: {
      if (w_param == IDNUM_TIMEOUT) {
        KillTimer(dialog_handle, w_param);
        num_lock_setup.id_event = 0;
        SHORT num_lock_set = ((GetKeyState(VK_NUMLOCK) & 0x0001) != 0) ? LSFW_LOCK : LSFW_UNLOCK;
        if (num_lock_set == LSFW_UNLOCK) {
          keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
          keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        }
        // SendMessage(window_indicator, UMNOTIFY_PROCESS, VK_NUMLOCK, TRUE);
      } else if (w_param == IDCAPS_TIMEOUT) {
        KillTimer(dialog_handle, w_param);
        caps_lock_setup.id_event = 0;
        SHORT caps_lock_set = (GetKeyState(VK_CAPITAL) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;
        if (caps_lock_set == LSFW_LOCK) {
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 1);
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 1);
        }
        // SendMessage(window_indicator, UMNOTIFY_PROCESS, VK_CAPITAL, TRUE);
      } else if (w_param == IDSHOW_TIMEOUT) {
        KillTimer(dialog_handle, w_param);
        ShowWindowAsync(dialog_handle, SW_HIDE);
      } else if (w_param == IDHOOK_TIMEOUT) {
        KillTimer(dialog_handle, w_param);
        int vk_code = (int) (INT_PTR) GetProp(dialog_handle, PROP_VKCODE);
        KeyLockLogic(vk_code);
      }
      return (INT_PTR) TRUE;
    }
    case WM_CLOSE: {
      EndDialog(dialog_handle, l_param);
      return (INT_PTR) TRUE;
    }
    case WM_NCDESTROY: {
      CleanupResources();
      PostQuitMessage(0);
      return (INT_PTR) TRUE;
    }
  }
  return (INT_PTR) FALSE;
}

int APIENTRY _tWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE previous_instance, _In_ LPTSTR command_line, _In_ int cmd_show) {
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(command_line);

  WPARAM exit_code = 0;
  HANDLE singleton_mutex = CreateMutex(nullptr, FALSE, TEXT("COM.HNSFNET.KEYLOCK"));
  if (singleton_mutex) {
    DWORD wait_result = WaitForSingleObject(singleton_mutex, 0);
    if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
      // GDI+ Startup
      ULONG_PTR gdi_startup_token;
      Gdiplus::GdiplusStartupInput gdi_startup_input;
      Gdiplus::GdiplusStartup(&gdi_startup_token, &gdi_startup_input, nullptr);

      menu_indicator = LoadMenu(instance, MAKEINTRESOURCE(IDR_KEYLOCK));

      WNDCLASSEX wnd_class_ex;
      wnd_class_ex.cbSize = sizeof(WNDCLASSEX);
      GetClassInfoEx(nullptr, WC_DIALOG, &wnd_class_ex);
      wnd_class_ex.lpszClassName = TEXT("KEYLOCK");
      wnd_class_ex.style &= ~CS_GLOBALCLASS;
      RegisterClassEx(&wnd_class_ex);

      DialogBox(instance, MAKEINTRESOURCE(IDD_KEYLOCK), nullptr, IndicatorWndProc);

      MSG msg_thread;
      while (GetMessage(&msg_thread, nullptr, 0, 0)) {
        TranslateMessage(&msg_thread);
        DispatchMessage(&msg_thread);
      }
      exit_code = msg_thread.wParam;

      //  GDI+ Shutdown
      Gdiplus::GdiplusShutdown(gdi_startup_token);

      ReleaseMutex(singleton_mutex);
    }
    CloseHandle(singleton_mutex);
  }
  return (int) exit_code;
}
