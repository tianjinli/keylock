#include "IndicatorContext.h"

#include <ShellScalingApi.h>

#include "ResourceManager.h"
#include "SoundManager.h"

void IndicatorContext::LoadIndicators(const std::unique_ptr<CSimpleIni>& ini_handle, HWND indicator_window) {
  auto indicator_key = GetIndicatorKey();
  /// ----------- 加载基础配置 ----------- ///
  auto offset = ini_handle->GetValue(indicator_key, TEXT("HudOffset"), TEXT("0,0"));
  _stscanf_s(offset, TEXT("%d,%d"), &hud_offset_.x, &hud_offset_.y);
  // 对齐方式(HL: 水平居左 HC: 水平居中 HR: 水平居右 VT: 垂直居上 VC: 垂直居中 VB: 垂直居下)
  int start = 0;
  CString token;
  CString hud_alignment = ini_handle->GetValue(indicator_key, TEXT("HudAlignment"), TEXT("HC|VT"));
  while (!(token = hud_alignment.Tokenize(TEXT("|"), start)).IsEmpty()) {
    auto alignment = token.Trim().MakeUpper();
    if (TEXT("HC") == alignment) {
      AddHudAlignment(Alignment::HC);
    } else if (TEXT("HR") == alignment) {
      AddHudAlignment(Alignment::HR);
    } else if (TEXT("VC") == alignment) {
      AddHudAlignment(Alignment::VC);
    } else if (TEXT("VB") == alignment) {
      AddHudAlignment(Alignment::VB);
    }
  }

  // # Lock 与 Unlock 窗口隐藏时间(单位：秒)
  hud_display_duration_ = (SHORT) ini_handle->GetLongValue(indicator_key, TEXT("HudDisplayDuration"), 1);
  CString auto_restore_state = ini_handle->GetValue(indicator_key, TEXT("AutoRestoreState"), TEXT("NONE"));
  auto_restore_state = auto_restore_state.Trim().MakeUpper();
  if (TEXT("LOCK") == auto_restore_state) {
    auto_restore_state_ = LockState::Lock;
  } else if (TEXT("UNLOCK") == auto_restore_state) {
    auto_restore_state_ = LockState::Unlock;
  }
  auto_restore_delay_ = (SHORT) ini_handle->GetLongValue(indicator_key, TEXT("AutoRestoreDelay"), 0);


  /// ----------- 加载图像配置 ----------- ///
  auto [lock_resource_id, unlock_resource_id] = RESOURCE_IDS[static_cast<int>(indicator_type_)];
  auto lock_hud_image_path = ini_handle->GetValue(indicator_key, TEXT("HudImageOn"), (CString(indicator_key) + CString("_On.png")));
  auto unlock_hud_image_path = ini_handle->GetValue(indicator_key, TEXT("HudImageOff"), (CString(indicator_key) + CString("_Off.png")));

  lock_hud_image_ = ResourceManager::LoadImageWithFallback(lock_hud_image_path, lock_resource_id, TEXT("PNG"));
  unlock_hud_image_ = ResourceManager::LoadImageWithFallback(unlock_hud_image_path, unlock_resource_id, TEXT("PNG"));

  auto lock_bitmap = ResourceManager::ConvertImageToBitmap(lock_hud_image_.get());
  auto unlock_bitmap = ResourceManager::ConvertImageToBitmap(unlock_hud_image_.get());

  uint32_t notify_id = kNotifyNumLockID + static_cast<int>(indicator_type_);
  uint32_t notify_um = UM_NOTIFY_NUMLOCK + static_cast<int>(indicator_type_);
  NOTIFYICONDATA notify_icon_data = {sizeof(notify_icon_data)};
  notify_icon_data.hWnd = indicator_window;
  notify_icon_data.uID = notify_id;
  notify_icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  notify_icon_data.uCallbackMessage = notify_um;

  lock_notify_icon_data_ = notify_icon_data;
  unlock_notify_icon_data_ = notify_icon_data;

  lock_bitmap->GetHICON(&lock_notify_icon_data_.hIcon);
  lock_notify_icon_.Attach(lock_notify_icon_data_.hIcon);

  unlock_bitmap->GetHICON(&unlock_notify_icon_data_.hIcon);
  unlock_notify_icon_.Attach(unlock_notify_icon_data_.hIcon);

  auto tray_tip_on = ini_handle->GetValue(indicator_key, TEXT("TrayTipOn"));
  auto tray_tip_off = ini_handle->GetValue(indicator_key, TEXT("TrayTipOff"));
  _tcscpy_s(lock_notify_icon_data_.szTip, sizeof(lock_notify_icon_data_.szTip), tray_tip_on);
  _tcscpy_s(unlock_notify_icon_data_.szTip, sizeof(unlock_notify_icon_data_.szTip), tray_tip_off);

  // 添加托盘图标
  auto current_state = GetCurrentLockState();
  if (current_state == LockState::Lock) {
    Shell_NotifyIcon(NIM_ADD, &lock_notify_icon_data_);
  } else {
    Shell_NotifyIcon(NIM_ADD, &unlock_notify_icon_data_);
  }


  /// ----------- 加载音效配置 ----------- ///

  // 开启静音状态
  sound_muted_ = ini_handle->GetBoolValue(indicator_key, TEXT("SoundMuted"), true);

  // # Lock 音效(支持 wav mp3 格式)
  auto key_sound_on_path = ini_handle->GetValue(indicator_key, TEXT("KeySoundOn"), (CString(indicator_key) + CString(TEXT("_On.wav"))));
  if (PathFileExists(key_sound_on_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(context->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    lock_key_sound_ = key_sound_on_path;
  }

  // # Unlock 音效(支持 wav mp3 格式)
  auto key_sound_off_path = ini_handle->GetValue(indicator_key, TEXT("KeySoundOff"), (CString(indicator_key) + CString(TEXT("_Off.wav"))));
  if (PathFileExists(key_sound_off_path)) {
    // CString command_string;
    // command_string.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(context->LockNode));
    // mciSendString(command_string, nullptr, 0, nullptr);
    unlock_key_sound_ = key_sound_off_path;
  }
}

void IndicatorContext::HandleIndicator() {
  LockState lock_state = GetCurrentLockState();

  // 播放按键音效
  PlaySoundEffect(lock_state);
  // 修改托盘图标
  ModifyTrayIcon(lock_state);
  // 显示HUD图像
  RenderAutoAligned(lock_state);
  // 安排自动恢复定时器
  ScheduleAutoRestore(lock_state);
}

void IndicatorContext::DisplayIndicator(bool tray_clicked) {
  auto lock_state = GetCurrentLockState();
  if (tray_clicked) {
    RenderNearTrayIcon(lock_state);
  } else {
    RenderAutoAligned(lock_state);
  }
}

void IndicatorContext::PlaySoundEffect(LockState lock_state) {
  if (sound_muted_) {
    return;
  }

  const CString& sound_path = (lock_state == LockState::Lock) ? lock_key_sound_ : unlock_key_sound_;
  if (!sound_path.IsEmpty()) {
    SoundManager::PlaySoundFile(sound_path);
  } else {
    SoundManager::PlayKeySound(lock_state);
  }
}

void IndicatorContext::ModifyTrayIcon(LockState lock_state) {
  if (lock_state == LockState::None) {
    return;
  }
  if (lock_state == LockState::Lock) {
    Shell_NotifyIcon(NIM_MODIFY, &lock_notify_icon_data_);
  } else {
    Shell_NotifyIcon(NIM_MODIFY, &unlock_notify_icon_data_);
  }
}

void IndicatorContext::RenderAutoAligned(LockState lock_state) {
  if (lock_state == LockState::None)
    return;

  Gdiplus::Image* hud_image = nullptr;
  if (lock_state == LockState::Lock) {
    hud_image = lock_hud_image_.get();
  } else {
    hud_image = unlock_hud_image_.get();
  }

  CPoint position = hud_offset_;
  int hud_image_width = hud_image->GetWidth();
  int hud_image_height = hud_image->GetHeight();
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  if (hud_alignment_ & static_cast<uint32_t>(Alignment::HC)) {
    position.x += (screen_width - hud_image_width) / 2;
  } else if (hud_alignment_ & static_cast<uint32_t>(Alignment::HR)) {
    position.x += (screen_width - hud_image_width);
  } else if (hud_alignment_ & static_cast<uint32_t>(Alignment::VC)) {
    position.y += (screen_height - hud_image_height) / 2;
  } else if (hud_alignment_ & static_cast<uint32_t>(Alignment::VB)) {
    position.y += (screen_height - hud_image_height);
  }
  RenderImageAt(hud_image, position);
  // 安排自动隐藏定时器
  ScheduleHudAutoHide();
}

void IndicatorContext::RenderNearTrayIcon(LockState lock_state) {
  if (lock_state == LockState::None)
    return;

  KillTimer(window_handle_, kHudAutoHideTimerID);

  Gdiplus::Image* hud_image = nullptr;
  uint32_t notify_icon_id = 0;
  if (lock_state == LockState::Lock) {
    hud_image = lock_hud_image_.get();
    notify_icon_id = lock_notify_icon_data_.uID;
  } else {
    hud_image = unlock_hud_image_.get();
    notify_icon_id = unlock_notify_icon_data_.uID;
  }

  CRect tray_rect{};
  NOTIFYICONIDENTIFIER tray_icon_identifier{sizeof(tray_icon_identifier)};
  tray_icon_identifier.hWnd = window_handle_; // 托盘图标所属窗口
  tray_icon_identifier.uID = notify_icon_id;

  Shell_NotifyIconGetRect(&tray_icon_identifier, &tray_rect);
  auto center = tray_rect.CenterPoint();

  APPBARDATA app_bar_data{sizeof(app_bar_data)};
  SHAppBarMessage(ABM_GETTASKBARPOS, &app_bar_data);

  CPoint pos{};
  const LONG width = hud_image->GetWidth();
  const LONG height = hud_image->GetHeight();
  switch (app_bar_data.uEdge) {
    case ABE_BOTTOM:
      pos.x = tray_rect.left + (tray_rect.Width() - width) / 2;
      pos.y = tray_rect.top - height - 8;
      break;

    case ABE_TOP:
      pos.x = tray_rect.left + (tray_rect.Width() - width) / 2;
      pos.y = tray_rect.bottom + 8;
      break;

    case ABE_LEFT:
      pos.x = tray_rect.right + 8;
      pos.y = tray_rect.top + (tray_rect.Height() - height) / 2;
      break;

    case ABE_RIGHT:
      pos.x = tray_rect.left - width - 8;
      pos.y = tray_rect.top + (tray_rect.Height() - height) / 2;
      break;
  }
  RenderImageAt(hud_image, pos);
}

void IndicatorContext::RenderImageAt(Gdiplus::Image* image, const CPoint& position) {
  const LONG width = image->GetWidth();
  const LONG height = image->GetHeight();

  // 屏幕 DC
  CClientDC screen_dc(nullptr);

  // 内存 DC
  CDC memory_dc;
  memory_dc.CreateCompatibleDC(screen_dc);

  // 创建兼容位图
  CBitmap new_bitmap;
  new_bitmap.CreateCompatibleBitmap(screen_dc, width, height);

  HBITMAP old_bitmap = memory_dc.SelectBitmap(new_bitmap);

  // GDI+ 绘制到内存 DC
  {
    Gdiplus::Graphics graphics(memory_dc);
    graphics.DrawImage(image, 0, 0, width, height);
  }

  // Alpha 混合
  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 0xFF;
  blend.AlphaFormat = AC_SRC_ALPHA;

  CSize size(width, height);
  CPoint src(0, 0);

  UpdateLayeredWindow(window_handle_, screen_dc, (POINT*) &position, &size, memory_dc, &src, 0, &blend, ULW_ALPHA);

  ShowWindowAsync(window_handle_, SW_SHOW);

  // 恢复位图
  memory_dc.SelectBitmap(old_bitmap);
}
