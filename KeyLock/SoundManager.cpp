#include "SoundManager.h"

#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

void SoundManager::PlayKeySound(LockState lock_state) {
  switch (lock_state) {
    case LockState::Lock:
      Beep(900, 60); // 高频，短促
      break;
    case LockState::Unlock:
      Beep(500, 60); // 低频，柔和
      break;
    default:
      break;
  }
}

void SoundManager::PlaySoundFile(const CString& sound_path) {
  CString command_string;
  command_string.Format(TEXT("play %s from 0"), sound_path);
  mciSendString(command_string, nullptr, 0, nullptr);
}
