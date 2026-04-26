#include "KeyboardHook.h"

HWND KeyboardHook::window_handle_ = nullptr;
HHOOK KeyboardHook::hook_handle_ = nullptr;

bool KeyboardHook::InstallHook() {
  hook_handle_ = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
  return hook_handle_ != nullptr;
}

void KeyboardHook::UninstallHook() {
  if (hook_handle_) {
    UnhookWindowsHookEx(hook_handle_);
    hook_handle_ = nullptr;
  }
}

void KeyboardHook::SetWindowHandle(HWND window_handle) {
  window_handle_ = window_handle;
}

LRESULT CALLBACK KeyboardHook::LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param, KeyboardHook* self) {
  if (code == HC_ACTION) {
    auto vk_code = ((KBDLLHOOKSTRUCT*) l_param)->vkCode;
    auto flags = ((KBDLLHOOKSTRUCT*) l_param)->flags;

    if (w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN) {
      if (window_handle_) {
        PostMessage(window_handle_, UM_KEYBOARD_HOOK, vk_code, flags);
      }
      return 0;
    }
  }

  return CallNextHookEx(self->hook_handle_, code, w_param, l_param);
}
