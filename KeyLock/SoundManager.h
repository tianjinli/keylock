#pragma once

#include <atlstr.h>
#include <windows.h>

#include "Constants.h"

/**
 * @brief 音效管理器类
 * 负责播放系统音效和自定义音效文件
 */
struct SoundManager {
  // 播放按键音效
  static void PlayKeySound(LockState state);

  // 播放音效文件
  static void PlaySoundFile(const CString& sound_path);
};
