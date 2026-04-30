#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atluser.h>

#include <atlstr.h>
#include <atltypes.h>
#include <gdiplus.h>

#include <memory>
#include <vector>

#include "Constants.h"
#include "SimpleIni.h"

enum class Alignment : uint32_t {
  HC = 0x02, // 水平居中
  HR = 0x04, // 水平居右
  VC = 0x20, // 垂直居中
  VB = 0x40, // 垂直居下
};

/**
 * @brief 指示器上下文类
 * 封装单个键盘指示器的所有状态和配置信息
 */
class IndicatorContext {
public:
  // 构造函数和析构函数
  IndicatorContext(KeyType indicator_type, HWND window_handle) : indicator_type_(indicator_type), window_handle_(window_handle) {
  }
  ~IndicatorContext() = default;

  // 禁用拷贝构造和赋值
  IndicatorContext(const IndicatorContext&) = delete;
  IndicatorContext& operator=(const IndicatorContext&) = delete;

  // 移动构造和赋值
  IndicatorContext(IndicatorContext&&) noexcept = default;
  IndicatorContext& operator=(IndicatorContext&&) noexcept = default;

  // 获取器
  KeyType GetIndicatorType() const {
    return indicator_type_;
  }
  const TCHAR* GetIndicatorKey() const {
    return INDICATOR_NAMES[static_cast<int>(indicator_type_)];
  }
  LockState GetAutoRestoreState() const {
    return auto_restore_state_;
  }
  SHORT GetAutoRestoreDelay() const {
    return auto_restore_delay_;
  }
  SHORT GetHudDisplayDuration() const {
    return hud_display_duration_;
  }
  bool IsSoundMuted() const {
    return sound_muted_;
  }
  const CPoint& GetHudOffset() const {
    return hud_offset_;
  }
  uint32_t GetHudAlignment() const {
    return hud_alignment_;
  }
  UINT_PTR GetAutoRestoreTimerId() const {
    return auto_restore_timer_id_;
  }

  // 设置器
  void SetAutoRestoreState(LockState state) {
    auto_restore_state_ = state;
  }
  void SetAutoRestoreDelay(SHORT delay) {
    auto_restore_delay_ = delay;
  }
  void SetHudDisplayDuration(SHORT duration) {
    hud_display_duration_ = duration;
  }
  void SetSoundMuted(bool muted) {
    sound_muted_ = muted;
  }
  void SetHudOffset(const CPoint& offset) {
    hud_offset_ = offset;
  }
  void SetHudAlignment(uint32_t alignment) {
    hud_alignment_ = alignment;
  }
  void AddHudAlignment(Alignment alignment) {
    hud_alignment_ |= static_cast<uint32_t>(alignment);
  }
  void SetAutoRestoreTimerId(UINT_PTR timer_id) {
    auto_restore_timer_id_ = timer_id;
  }

  // 音效设置
  void SetLockSound(const CString& sound_path) {
    lock_key_sound_ = sound_path;
  }
  void SetUnlockSound(const CString& sound_path) {
    unlock_key_sound_ = sound_path;
  }

  LockState GetCurrentLockState() {
    uint32_t key_state = GetKeyState(INDICATOR_KEYS[static_cast<int>(indicator_type_)]);
    return (key_state & 0x0001) ? LockState::Lock : LockState::Unlock;
  }

  void StopAutoRestoreTimer() {
    if (auto_restore_timer_id_ > 0) {
      KillTimer(window_handle_, auto_restore_timer_id_);
      auto_restore_timer_id_ = 0;
    }
  }

  // 安排自动恢复定时器
  void ScheduleAutoRestore(LockState lock_state) {
    if (auto_restore_state_ != LockState::None && auto_restore_state_ != lock_state && auto_restore_delay_ >= 0 && auto_restore_timer_id_ == 0) {
      auto_restore_timer_id_ = kNumLockTimerID + static_cast<int>(indicator_type_);
      SetTimer(window_handle_, auto_restore_timer_id_, SECONDS_TO_MILLISECONDS(auto_restore_delay_), nullptr);
    }
  }

  void ScheduleHudAutoHide() {
    if (hud_display_duration_ > 0) {
      SetTimer(window_handle_, kHudAutoHideTimerID, SECONDS_TO_MILLISECONDS(hud_display_duration_), nullptr);
    }
  }

  // 加载指示器配置
  void LoadIndicators(const std::unique_ptr<CSimpleIni>& ini_handle, HWND indicator_window);
  // 处理指示器状态变化
  void HandleIndicator();
  // 显示当前状态指示器
  void DisplayIndicator(bool tray_clicked = false);

  void PlaySoundEffect(LockState lock_state);
  void ModifyTrayIcon(LockState lock_state);

  // 绘制HUD图像
  void RenderAutoAligned(LockState lock_state);
  void RenderNearTrayIcon(LockState lock_state);

private:
  // 渲染图像到指定位置
  void RenderImageAt(Gdiplus::Image* image, const CPoint& position);

  KeyType indicator_type_; // 指示器类型
  LockState auto_restore_state_{LockState::None}; // 自动恢复锁定状态
  SHORT auto_restore_delay_{0}; // 自动恢复延时时间
  SHORT hud_display_duration_{1}; // HUD显示持续时间
  CPoint hud_offset_{0, 0}; // 桌面图像显示偏移量
  uint32_t hud_alignment_ = static_cast<uint32_t>(Alignment::HC); // 指示器对齐方式
  bool sound_muted_{true}; // 音效是否静音
  CString lock_key_sound_; // 锁定时音效
  CString unlock_key_sound_; // 未锁定时音效
  UINT_PTR auto_restore_timer_id_{0}; // 自动恢复计时器ID
  HWND window_handle_; // 主窗口句柄

  std::unique_ptr<Gdiplus::Image> lock_hud_image_;
  std::unique_ptr<Gdiplus::Image> unlock_hud_image_;

  NOTIFYICONDATA lock_notify_icon_data_{sizeof(NOTIFYICONDATA)};
  NOTIFYICONDATA unlock_notify_icon_data_{sizeof(NOTIFYICONDATA)};

  WTL::CIcon lock_notify_icon_;
  WTL::CIcon unlock_notify_icon_;
};
