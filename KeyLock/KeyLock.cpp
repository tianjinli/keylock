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
// 滚动锁定超时事件ID
#define IDSCROLL_TIMEOUT (WM_USER + 3)

// HUD显示超时事件ID
#define IDHUD_TIMEOUT (WM_APP + 1)
// 键盘钩子处理事件ID
#define IDHOOK_TIMEOUT (WM_APP + 2)

// 数字锁定托盘图标ID
#define IDNOTIFY_NUMLOCK (WM_USER + 1)
// 大写锁定托盘图标ID
#define IDNOTIFY_CAPSLOCK (WM_USER + 2)
// 滚动锁定托盘图标ID
#define IDNOTIFY_SCROLLLOCK (WM_USER + 3)

// 数字锁定托盘事件ID
#define UMNOTIFY_NUMLOCK (WM_APP + 1)
// 大写锁定托盘事件ID
#define UMNOTIFY_CAPSLOCK (WM_APP + 2)
// 滚动锁定托盘事件ID
#define UMNOTIFY_SCROLLLOCK (WM_APP + 3)

// 秒数转毫秒数
#define MILISECONDS(SECONDS) (SECONDS * 1000)

// 键盘处理钩子
static HHOOK keyboard_hook{nullptr};

// 键盘按键列表
static const int vk_code_list[3] = {VK_NUMLOCK, VK_CAPITAL, VK_SCROLL};

// 键盘按键文字列表
static const TCHAR* indicator_key_list[3] = {TEXT("NumLock"), TEXT("CapsLock"), TEXT("ScrollLock")};

// 运行时参数列表
static IndicatorContext context_list[3] = {{IDB_NUMLOCKON, IDB_NUMLOCKOFF}, {IDB_CAPSLOCKON, IDB_CAPSLOCKOFF}, {IDB_SCROLLLOCKON, IDB_SCROLLLOCKOFF}};

// 指示器窗口句柄
static HWND indicator_window{nullptr};

// 指示器托盘菜单
static HMENU indicator_menu{nullptr};

// 菜单文字列表
static CString menu_text_list[3];

// 当前工作目录
static TCHAR work_dir[MAX_PATH];

// 上一次按键码
static int last_vk_code;

void KeyLockLogic(int vk_code) {
  bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

  // 使用 GetKeyState 更可靠（钩子场景下推荐）
  for (size_t i = 0; i < std::size(vk_code_list); i++) {
    auto lock_set = (GetKeyState(vk_code_list[i]) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;

    // 查看锁定状态
    auto context = &context_list[i];
    if (alt_down && vk_code == '1' + i) {
      DrawImageIcon(context);
      return;
    }

    // 启动超时打开/关闭锁定
    if (context->auto_restore_state) {
      if (lock_set != context->auto_restore_state && context->auto_restore_delay > 0 && context->auto_restore_timer_id == 0) {
        context->auto_restore_timer_id = SetTimer(indicator_window, IDNUM_TIMEOUT + i, MILISECONDS(context->auto_restore_delay) + i, nullptr);
      }
    }
    if (vk_code == vk_code_list[i]) {
      DrawImageIcon(context, lock_set);
    }
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
        for (auto& context: context_list) {
          if (context.auto_restore_timer_id) {
            KillTimer(indicator_window, context.auto_restore_timer_id);
            context.auto_restore_timer_id = 0;
          }
        }
      }
      SetProp(indicator_window, PROP_VKCODE, (HANDLE) (INT_PTR) vk_code);
      SetTimer(indicator_window, IDHOOK_TIMEOUT, 10, nullptr);
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
VOID CleanupTrayIcons(IndicatorContext* context) {
  if (context->lock_tray_icon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &context->lock_tray_icon);
    DestroyIcon(context->lock_tray_icon.hIcon);
    context->lock_tray_icon.hIcon = nullptr;
  }
  if (context->unlock_tray_icon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &context->unlock_tray_icon);
    DestroyIcon(context->unlock_tray_icon.hIcon);
    context->unlock_tray_icon.hIcon = nullptr;
  }
}

// 清理所有资源
VOID CleanupResources() {
  // ⭐ 主动释放 GDI+ 相关对象，确保在 GdiplusShutdown 之前释放
  for (auto& context: context_list) {
    CleanupTrayIcons(&context);
    context.lock_hud_image.reset();
    context.unlock_hud_image.reset();
  }

  if (indicator_menu) {
    DestroyMenu(indicator_menu);
    indicator_menu = nullptr;
  }

  if (keyboard_hook) {
    UnhookWindowsHookEx(keyboard_hook);
    keyboard_hook = nullptr;
  }

  // 注销窗口类
  UnregisterClass(TEXT("KEYLOCK"), GetModuleHandle(nullptr));
}

int GetContextIndex(IndicatorContext* context) {
  for (size_t i = 0; i < std::size(context_list); i++) {
    if (context == &context_list[i]) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

VOID DrawImageIcon(IndicatorContext* context, SHORT current_lock_set) {
  Gdiplus::Image* hud_image = nullptr;
  NOTIFYICONDATA* tray_icon = nullptr;
  SHORT target_lock_set = current_lock_set;
  if (current_lock_set == 0) {
    int index = GetContextIndex(context);
    if (index == -1) {
      return;
    }
    target_lock_set = (GetKeyState(vk_code_list[index]) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;
  } else if (context->indicator_status == current_lock_set) {
    return; // 状态没变直接返回
  } else {
    context->indicator_status = current_lock_set;
  }

  CString sound;
  if (target_lock_set == LSFW_LOCK) {
    hud_image = context->lock_hud_image.get();
    tray_icon = &context->lock_tray_icon;
    sound = context->lock_key_sound;
  } else {
    hud_image = context->unlock_hud_image.get();
    tray_icon = &context->unlock_tray_icon;
    sound = context->unlock_key_sound;
  }

  if (current_lock_set && !context->sound_muted) { // 单击托盘不播放音效
    if (sound.IsEmpty()) {
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
      command_string.Format(TEXT("play %s from 0"), sound);
      mciSendString(command_string, nullptr, 0, nullptr);
    }
  }

  if (hud_image) {
    LONG width = hud_image->GetWidth();
    LONG height = hud_image->GetHeight();

    HDC screen_dc = GetDC(nullptr);
    HDC memory_dc = CreateCompatibleDC(screen_dc);
    HBITMAP new_bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    HBITMAP old_bitmap = (HBITMAP) SelectObject(memory_dc, new_bitmap);

    Gdiplus::Graphics graphics(memory_dc);

    graphics.DrawImage(hud_image, 0, 0, width, height);

    BLENDFUNCTION blend_function{0};
    blend_function.BlendOp = AC_SRC_OVER;
    blend_function.SourceConstantAlpha = 0xff;
    blend_function.AlphaFormat = AC_SRC_ALPHA;
    CSize size_struct(width, height);
    CPoint src_point(0, 0);
    UpdateLayeredWindow(indicator_window, screen_dc, &context->hud_position, &size_struct, memory_dc, &src_point, 0, &blend_function, ULW_ALPHA);

    ShowWindowAsync(indicator_window, SW_SHOW);

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(new_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);

    if (context->hud_display_duration > 0) {
      SetTimer(indicator_window, IDHUD_TIMEOUT, MILISECONDS(context->hud_display_duration), nullptr);
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
      return file_image;
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
    return file_image;
  }

  return nullptr;
}

VOID LoadMenuText(const std::unique_ptr<CSimpleIni>& ini) {
  ATLASSERT(indicator_menu != nullptr);

  CONST TCHAR* menu_section = TEXT("MenuText");
  HMENU menu_file = GetSubMenu(indicator_menu, 0);

  menu_text_list[0] = ini->GetValue(menu_section, TEXT("FileNum"), TEXT("Num Lock"));
  menu_text_list[1] = ini->GetValue(menu_section, TEXT("FileCaps"), TEXT("Caps Lock"));
  menu_text_list[2] = ini->GetValue(menu_section, TEXT("FileScroll"), TEXT("Scroll Lock"));

  auto file_modify_text = ini->GetValue(menu_section, TEXT("FileModify"), TEXT("Modify Settings"));
  ModifyMenu(menu_file, ID_FILE_MODIFY, MF_BYCOMMAND, ID_FILE_MODIFY, file_modify_text);

  auto file_start_text = ini->GetValue(menu_section, TEXT("FileStart"), TEXT("Auto Start"));
  ModifyMenu(menu_file, ID_FILE_START, MF_BYCOMMAND, ID_FILE_START, file_start_text);

  auto file_exit_text = ini->GetValue(menu_section, TEXT("FileExit"), TEXT("Quick Exit"));
  ModifyMenu(menu_file, ID_FILE_EXIT, MF_BYCOMMAND, ID_FILE_EXIT, file_exit_text);
}

void LoadHudImage(const CString& file_path, UINT resource_id, const std::vector<CString>& alignment_list, POINT* out_hud_position,
                  std::unique_ptr<Gdiplus::Image>* out_hud_image_ptr) {
  std::unique_ptr<Gdiplus::Image> hud_image_ptr = LoadImageWithFallback(file_path, GetModuleHandle(nullptr), resource_id);
  if (hud_image_ptr != nullptr) {
    int hud_image_width = hud_image_ptr->GetWidth();
    int hud_image_height = hud_image_ptr->GetHeight();

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    if (out_hud_position != nullptr) {
      for (auto& alignment: alignment_list) {
        if (TEXT("HC") == alignment) {
          out_hud_position->x += (screen_width - hud_image_width) / 2;
        } else if (TEXT("HR") == alignment) {
          out_hud_position->x += (screen_width - hud_image_width);
        } else if (TEXT("VC") == alignment) {
          out_hud_position->y += (screen_height - hud_image_height) / 2;
        } else if (TEXT("VB") == alignment) {
          out_hud_position->y += (screen_height - hud_image_height);
        }
      }
    }

    *out_hud_image_ptr = std::make_unique<Gdiplus::Bitmap>(hud_image_width, hud_image_height, PixelFormat32bppARGB);
    std::unique_ptr<Gdiplus::Graphics> hud_graphics_ptr(Gdiplus::Graphics::FromImage(out_hud_image_ptr->get()));

    hud_graphics_ptr->DrawImage(hud_image_ptr.get(), Gdiplus::Rect(0, 0, hud_image_width, hud_image_height), 0, 0, hud_image_width, hud_image_height,
                                Gdiplus::Unit::UnitPixel);
  }
}

void LoadTrayIcon(const CString& file_path, UINT resource_id, UINT index, NOTIFYICONDATA* out_tray_icon) {
  UINT tray_id = IDNOTIFY_NUMLOCK + index;
  UINT tray_um = UMNOTIFY_NUMLOCK + index;
  std::unique_ptr<Gdiplus::Image> tray_icon_ptr = LoadImageWithFallback(file_path, GetModuleHandle(nullptr), resource_id);
  if (tray_icon_ptr != nullptr) {
    int tray_icon_width = tray_icon_ptr->GetWidth();
    int tray_icon_height = tray_icon_ptr->GetHeight();
    Gdiplus::Bitmap tray_icon_bitmap(tray_icon_width, tray_icon_height, PixelFormat32bppARGB);
    std::unique_ptr<Gdiplus::Graphics> tray_icon_graphics_ptr(Gdiplus::Graphics::FromImage(&tray_icon_bitmap));

    Gdiplus::Rect tray_icon_rect{0, 0, tray_icon_width, tray_icon_height};
    tray_icon_graphics_ptr->DrawImage(tray_icon_ptr.get(), tray_icon_rect, 0, 0, tray_icon_width, tray_icon_height, Gdiplus::Unit::UnitPixel);

    out_tray_icon->cbSize = sizeof(NOTIFYICONDATA);
    out_tray_icon->hWnd = indicator_window;
    out_tray_icon->uID = tray_id;
    out_tray_icon->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    out_tray_icon->uCallbackMessage = tray_um;
    tray_icon_bitmap.GetHICON(&out_tray_icon->hIcon);
  }
}

VOID LoadParameters(const std::unique_ptr<CSimpleIni>& ini, uint32_t index) {
  ATLASSERT(indicator_window != nullptr);

  auto context = &context_list[index];
  SHORT key_state = GetKeyState(vk_code_list[index]);
  context->indicator_key = indicator_key_list[index];

  // # Lock 与 Unlock 图标位置(默认左 30px 上 30px)
  auto offset = ini->GetValue(context->indicator_key, TEXT("HudOffset"), TEXT("0,0"));
  _stscanf_s(offset, TEXT("%d,%d"), &context->hud_position.x, &context->hud_position.y);

  // 对齐方式(HL: 水平居左 HC: 水平居中 HR: 水平居右 VT: 垂直居上 VC: 垂直居中 VB: 垂直居下)
  int start = 0;
  CString hud_alignment = ini->GetValue(context->indicator_key, TEXT("HudAlignment"), TEXT("HC|VC"));
  std::vector<CString> hud_alignment_list = {hud_alignment.Tokenize(TEXT("|"), start).Trim().MakeUpper(),
                                             hud_alignment.Tokenize(TEXT("|"), start).Trim().MakeUpper()};

  // # Lock 与 Unlock 置顶图标(左 Lock 右 Unlock)
  auto hud_image_on_path = ini->GetValue(context->indicator_key, TEXT("HudImageOn"), (CString(context->indicator_key) + CString("_On.png")));
  LoadHudImage(hud_image_on_path, context->lock_resource_id, hud_alignment_list, &context->hud_position, &context->lock_hud_image);
  auto hud_image_off_path = ini->GetValue(context->indicator_key, TEXT("HudImageOff"), (CString(context->indicator_key) + CString("_Off.png")));
  LoadHudImage(hud_image_off_path, context->unlock_resource_id, hud_alignment_list, nullptr, &context->unlock_hud_image);
  // # Lock 与 Unlock 窗口隐藏时间(单位：秒)
  context->hud_display_duration = (SHORT) ini->GetLongValue(context->indicator_key, TEXT("HudDisplayDuration"), 1);
  CString auto_restore_state = ini->GetValue(context->indicator_key, TEXT("AutoRestoreState"), TEXT("NONE"));
  auto_restore_state = auto_restore_state.Trim().MakeUpper();
  if (TEXT("LOCK") == auto_restore_state) {
    context->auto_restore_state = LSFW_LOCK;
  } else if (TEXT("UNLOCK") == auto_restore_state) {
    context->auto_restore_state = LSFW_UNLOCK;
  }
  context->auto_restore_delay = (SHORT) ini->GetLongValue(context->indicator_key, TEXT("AutoRestoreDelay"), 0);

  // # Lock 与 Unlock 托盘图标(左边 Lock 右边 Unlock)
  auto tray_icon_on_path = ini->GetValue(context->indicator_key, TEXT("TrayIconOn"), (CString(context->indicator_key) + CString("_On.png")));
  LoadTrayIcon(tray_icon_on_path, context->lock_resource_id, index, &context->lock_tray_icon);
  auto tray_icon_off_path = ini->GetValue(context->indicator_key, TEXT("TrayIconOff"), (CString(context->indicator_key) + CString("_Off.png")));
  LoadTrayIcon(tray_icon_off_path, context->unlock_resource_id, index, &context->unlock_tray_icon);

  // 开启静音状态
  context->sound_muted = ini->GetBoolValue(context->indicator_key, TEXT("SoundMuted"), true);

  // # Lock 音效(支持 wav mp3 格式)
  auto key_sound_on_path = ini->GetValue(context->indicator_key, TEXT("KeySoundOn"), (CString(context->indicator_key) + CString(TEXT("_On.wav"))));

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(key_sound_on_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(context->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    context->lock_key_sound = key_sound_on_path;
  }

  // # Unlock 音效(支持 wav mp3 格式)
  auto key_sound_off_path = ini->GetValue(context->indicator_key, TEXT("KeySoundOff"), (CString(context->indicator_key) + CString(TEXT("_Off.wav"))));

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(key_sound_off_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(context->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    context->unlock_key_sound = key_sound_off_path;
  }

  if (key_state == 1) {
    context->indicator_status = LSFW_LOCK;
    if (context->lock_tray_icon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &context->lock_tray_icon);
    }
  } else if (key_state == 0) {
    context->indicator_status = LSFW_UNLOCK;
    if (context->lock_tray_icon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &context->unlock_tray_icon);
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
      indicator_window = dialog_handle;

      DWORD extended_style = GetWindowLong(dialog_handle, GWL_EXSTYLE);
      SetWindowLong(dialog_handle, GWL_EXSTYLE, extended_style | WS_EX_NOACTIVATE);

      GetModuleFileName(nullptr, work_dir, MAX_PATH);
      *(_tcsrchr(work_dir, TEXT('\\'))) = 0; // 把最后一个 \ 替换为 0
      SetCurrentDirectory(work_dir);

      CString config_path = CString(work_dir) + CString(TEXT("\\")) + CString(CONFIG_NAME);
      if (auto ini = LoadConfigWithFallback(config_path, GetModuleHandle(nullptr), IDR_APPSETTINGS)) {
        LoadMenuText(ini);
        // 最后加载托盘图标在左边
        for (int i = std::size(context_list) - 1; i >= 0; i--) {
          auto context = &context_list[i];
          LoadParameters(ini, i);
          if (context->auto_restore_state) {
            SHORT lock_set = GetKeyState(vk_code_list[i]) ? LSFW_LOCK : LSFW_UNLOCK;
            if (lock_set != context->auto_restore_state && context->auto_restore_delay > 0) {
              context->auto_restore_timer_id = SetTimer(indicator_window, IDNUM_TIMEOUT + i, MILISECONDS(context->auto_restore_delay) + i, nullptr);
            }
          }
        }
      }

      // SetTimer(dialog_handle, IDEVENT_SCANNING, 10, nullptr);
      keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
      return (INT_PTR) TRUE;
    }
    case WM_COMMAND: {
      if (w_param == ID_FILE_MODIFY) {
        CString absolute_path;
        absolute_path = CString(work_dir) + CString("\\NumLock_On.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_NUMLOCKON, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\CapsLock_On.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_CAPSLOCKON, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\ScrollLock_On.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_SCROLLLOCKON, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\NumLock_Off.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_NUMLOCKOFF, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\CapsLock_Off.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_CAPSLOCKOFF, TEXT("PNG"), absolute_path);
        absolute_path = CString(work_dir) + CString("\\ScrollLock_Off.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_SCROLLLOCKOFF, TEXT("PNG"), absolute_path);

        absolute_path = CString(work_dir) + CString("\\AppSettings.ini");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDR_APPSETTINGS, RT_RCDATA, absolute_path);
        // 打开配置文件
        ShellExecute(nullptr, nullptr, absolute_path, nullptr, nullptr, SW_SHOW);
      } else if (w_param == ID_FILE_START) {
        HMENU menu_file = GetSubMenu(indicator_menu, 0);
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
    case UMNOTIFY_NUMLOCK:
    case UMNOTIFY_CAPSLOCK:
    case UMNOTIFY_SCROLLLOCK: {
      if (LOWORD(l_param) == WM_RBUTTONUP) {
        SetForegroundWindow(dialog_handle); // 没有这个菜单不会自动消失

        POINT popup_point;
        GetCursorPos(&popup_point);
        HMENU menu_file = GetSubMenu(indicator_menu, 0);
        CheckMenuItem(menu_file, ID_FILE_START, CurrentVersionRun(IDACTION_CHECK) ? MF_CHECKED : MF_UNCHECKED);

        int index = message - UMNOTIFY_NUMLOCK;
        ModifyMenu(menu_file, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, menu_text_list[index]);
        CheckMenuItem(menu_file, ID_FILE_ABOUT, GetKeyState(vk_code_list[index]) ? MF_CHECKED : MF_UNCHECKED);
        TrackPopupMenu(menu_file, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERTICAL, popup_point.x, popup_point.y, 0, dialog_handle, nullptr);
      } else if (LOWORD(l_param) == WM_LBUTTONUP) {
        int index = message - UMNOTIFY_NUMLOCK;
        DrawImageIcon(&context_list[index]);
      }
      return (INT_PTR) TRUE;
    }
    case WM_TIMER: {
      switch (w_param) {
        case IDNUM_TIMEOUT:
        case IDCAPS_TIMEOUT:
        case IDSCROLL_TIMEOUT: {
          KillTimer(dialog_handle, w_param);
          auto index = w_param - IDNUM_TIMEOUT;
          auto context = &context_list[index];
          context->auto_restore_timer_id = 0;
          SHORT lock_set = ((GetKeyState(vk_code_list[index]) & 0x0001) != 0) ? LSFW_LOCK : LSFW_UNLOCK;
          if (lock_set != context->auto_restore_state) {
            keybd_event(vk_code_list[index], 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
            keybd_event(vk_code_list[index], 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
          }
          // SendMessage(dialog_handle, UMNOTIFY_PROCESS, vk_code_list[index], TRUE);
          break;
        }
        case IDHUD_TIMEOUT: {
          KillTimer(dialog_handle, w_param);
          ShowWindowAsync(dialog_handle, SW_HIDE);
          break;
        }
        case IDHOOK_TIMEOUT: {
          KillTimer(dialog_handle, w_param);
          int vk_code = (int) (INT_PTR) GetProp(dialog_handle, PROP_VKCODE);
          KeyLockLogic(vk_code);
          break;
        }
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

      indicator_menu = LoadMenu(instance, MAKEINTRESOURCE(IDR_KEYLOCK));

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
