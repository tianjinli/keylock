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
static HHOOK ghKeyboardHook{nullptr};

// 数字锁定键配置
static RUNTIMEPARAMS gNumLockSetup{IDB_NUMLOCK};
// 大写锁定键配置
static RUNTIMEPARAMS gCapsLockSetup{IDB_CAPSLOCK};

// 大写锁定计时器
static UINT_PTR gCapsTimerId;

// 指示器窗口句柄
static HWND ghWndIndicator;

// 指示器托盘菜单
static HMENU ghMenuIndicator;

// 数字键锁菜单文字
static TCHAR gszFileNum[MAX_PATH];
// 大写键锁菜单文字
static TCHAR gszFileCaps[MAX_PATH];

// 当前工作目录
static TCHAR gszWorkDir[MAX_PATH];

// 临时配置文件路径（需要清理）
static CString gstrTempConfigPath;

static int gnLastVkCode;

void KeyLockLogic(int vkCode) {
  bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

  // 使用 GetKeyState 更可靠（钩子场景下推荐）
  SHORT wNumLockSet = (GetKeyState(VK_NUMLOCK) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;
  SHORT wCapsLockSet = (GetKeyState(VK_CAPITAL) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;

  if (gNumLockSetup.IDEvent) {
    KillTimer(ghWndIndicator, gNumLockSetup.IDEvent);
    gNumLockSetup.IDEvent = 0;
  }
  if (gCapsLockSetup.IDEvent) {
    KillTimer(ghWndIndicator, gCapsLockSetup.IDEvent);
    gCapsLockSetup.IDEvent = 0;
  }

  if (altDown && vkCode == '1') {
    DrawImageIcon(&gNumLockSetup);
    return;
  }
  if (altDown && vkCode == '2') {
    DrawImageIcon(&gCapsLockSetup);
    return;
  }

  if (wNumLockSet == LSFW_UNLOCK && gNumLockSetup.Timeout > 0) {
    gNumLockSetup.IDEvent = SetTimer(ghWndIndicator, IDNUM_TIMEOUT, MILISECONDS(gNumLockSetup.Timeout), nullptr);
  }
  if (vkCode == VK_NUMLOCK) {
    DrawImageIcon(&gNumLockSetup, wNumLockSet);
  }

  if (wCapsLockSet == LSFW_LOCK && gCapsLockSetup.Timeout > 0) {
    gCapsLockSetup.IDEvent = SetTimer(ghWndIndicator, IDCAPS_TIMEOUT, MILISECONDS(gCapsLockSetup.Timeout), nullptr);
  }
  if (vkCode == VK_CAPITAL) {
    DrawImageIcon(&gCapsLockSetup, wCapsLockSet);
  }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    auto vkCode = ((KBDLLHOOKSTRUCT*) lParam)->vkCode;
    auto flags = ((KBDLLHOOKSTRUCT*) lParam)->flags;

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      // if (!(flags & LLKHF_INJECTED)) {
      // KillTimer(ghWndIndicator, IDHOOK_TIMEOUT);
      SetProp(ghWndIndicator, PROP_VKCODE, (HANDLE) (INT_PTR) vkCode);
      SetTimer(ghWndIndicator, IDHOOK_TIMEOUT, 10, nullptr);
      // }
    }
  }

  return CallNextHookEx(ghKeyboardHook, nCode, wParam, lParam);
}

// 从资源中提取AppSettings.ini到临时目录，并返回配置文件路径
// 如果本地存在AppSettings.ini则返回本地路径，否则从资源中提取
VOID GetConfigFilePath(CONST TCHAR* lpCurrentDirectory, CString& strOutConfigPath) {
  strOutConfigPath = CString(lpCurrentDirectory) + CString(TEXT("\\")) + CString(CONFIG_NAME);

  // 检查本地AppSettings.ini是否存在
  if (PathFileExists(strOutConfigPath)) {
    return; // 本地文件存在，直接使用
  }

  // 本地文件不存在，从资源中提取
  HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(IDR_APPSETTINGS), RT_RCDATA);
  if (!hResource) {
    return; // 资源不存在，保持使用本地路径（会使用默认值）
  }

  HGLOBAL hGlobal = LoadResource(nullptr, hResource);
  if (!hGlobal) {
    return;
  }

  DWORD dwResourceSize = SizeofResource(nullptr, hResource);
  if (dwResourceSize == 0) {
    return;
  }

  LPVOID pResourceData = LockResource(hGlobal);
  if (!pResourceData) {
    return;
  }

  // 获取临时文件路径
  TCHAR szTempPath[MAX_PATH];
  TCHAR szTempFile[MAX_PATH];

  if (!GetTempPath(MAX_PATH, szTempPath)) {
    return;
  }

  if (!GetTempFileName(szTempPath, TEXT("KL"), 0, szTempFile)) {
    return;
  }

  // 将临时文件重命名为AppSettings.ini
  TCHAR szTempConfigPath[MAX_PATH];
  _tcscpy_s(szTempConfigPath, MAX_PATH, szTempFile);
  TCHAR* pszLastBackslash = _tcsrchr(szTempConfigPath, TEXT('\\'));
  if (pszLastBackslash) {
    _tcscpy_s(pszLastBackslash + 1, MAX_PATH - (pszLastBackslash - szTempConfigPath + 1), CONFIG_NAME);
  } else {
    _tcscpy_s(szTempConfigPath, MAX_PATH, CONFIG_NAME);
  }

  // 创建临时配置文件
  HANDLE hFile = CreateFile(szTempConfigPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile != INVALID_HANDLE_VALUE) {
    DWORD dwBytesWritten;
    if (WriteFile(hFile, pResourceData, dwResourceSize, &dwBytesWritten, nullptr)) {
      strOutConfigPath = szTempConfigPath;
      gstrTempConfigPath = szTempConfigPath; // 保存临时文件路径用于清理
    }
    CloseHandle(hFile);
  } else {
    // 如果写文件失败，使用原始的本地路径（会使用默认值）
  }
}

// 清理托盘图标
VOID CleanupTrayIcons(RUNTIMEPARAMS* pRuntimeParams) {
  if (pRuntimeParams->LockTrayIcon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &pRuntimeParams->LockTrayIcon);
    DestroyIcon(pRuntimeParams->LockTrayIcon.hIcon);
    pRuntimeParams->LockTrayIcon.hIcon = nullptr;
  }
  if (pRuntimeParams->UnlockTrayIcon.hIcon) {
    Shell_NotifyIcon(NIM_DELETE, &pRuntimeParams->UnlockTrayIcon);
    DestroyIcon(pRuntimeParams->UnlockTrayIcon.hIcon);
    pRuntimeParams->UnlockTrayIcon.hIcon = nullptr;
  }
}

// 清理所有资源
VOID CleanupResources() {
  CleanupTrayIcons(&gNumLockSetup);
  CleanupTrayIcons(&gCapsLockSetup);

  // ⭐ 主动释放 GDI+ 相关对象，确保在 GdiplusShutdown 之前释放
  gNumLockSetup.LockDeskImage.reset();
  gNumLockSetup.UnlockDeskImage.reset();
  gCapsLockSetup.LockDeskImage.reset();
  gCapsLockSetup.UnlockDeskImage.reset();

  if (ghMenuIndicator) {
    DestroyMenu(ghMenuIndicator);
    ghMenuIndicator = nullptr;
  }

  if (ghKeyboardHook) {
    UnhookWindowsHookEx(ghKeyboardHook);
    ghKeyboardHook = nullptr;
  }

  // 删除临时配置文件
  if (!gstrTempConfigPath.IsEmpty() && PathFileExists(gstrTempConfigPath)) {
    DeleteFile(gstrTempConfigPath);
    gstrTempConfigPath.Empty();
  }

  // 注销窗口类
  UnregisterClass(TEXT("KEYLOCK"), GetModuleHandle(nullptr));
}

VOID DrawImageIcon(RUNTIMEPARAMS* pRuntimeParams, SHORT wCurrentLockSet) {
  Gdiplus::Image* pDeskImage = nullptr;
  NOTIFYICONDATA* pTrayIcon = nullptr;
  SHORT wTargetLockSet = wCurrentLockSet;
  if (wCurrentLockSet == 0) {
    wTargetLockSet = (pRuntimeParams == &gNumLockSetup) ? (GetAsyncKeyState(VK_NUMLOCK) & 1 ? LSFW_LOCK : LSFW_UNLOCK)
                                                        : (GetAsyncKeyState(VK_CAPITAL) & 1 ? LSFW_LOCK : LSFW_UNLOCK);
  } else if (pRuntimeParams->LockSet == wCurrentLockSet) {
    return; // 状态没变，直接返回
  } else {
    pRuntimeParams->LockSet = wCurrentLockSet; // 先更新状态
  }

  CString strSoundEffect;
  if (wTargetLockSet == LSFW_LOCK) {
    pDeskImage = pRuntimeParams->LockDeskImage.get();
    pTrayIcon = &pRuntimeParams->LockTrayIcon;
    strSoundEffect = pRuntimeParams->SoundOn;
  } else {
    pDeskImage = pRuntimeParams->UnlockDeskImage.get();
    pTrayIcon = &pRuntimeParams->UnlockTrayIcon;
    strSoundEffect = pRuntimeParams->SoundOff;
  }

  if (wCurrentLockSet && !pRuntimeParams->EnableMute) { // 单击托盘不播放音效
    if (strSoundEffect.IsEmpty()) {
      switch (wCurrentLockSet) {
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
      CString strCommand;
      strCommand.Format(TEXT("play %s from 0"), strSoundEffect);
      mciSendString(strCommand, nullptr, 0, nullptr);
    }
  }

  if (pDeskImage) {
    LONG nWidth = pDeskImage->GetWidth();
    LONG nHeight = pDeskImage->GetHeight();

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);
    HBITMAP hNewBitmap = CreateCompatibleBitmap(hdcScreen, nWidth, nHeight);
    HBITMAP hOldBitmap = (HBITMAP) SelectObject(hdcMemory, hNewBitmap);

    Gdiplus::Graphics gdiGraphics(hdcMemory);

    gdiGraphics.DrawImage(pDeskImage, 0, 0, nWidth, nHeight);

    BLENDFUNCTION BlendFunc{0};
    BlendFunc.BlendOp = AC_SRC_OVER;
    BlendFunc.SourceConstantAlpha = 0xff;
    BlendFunc.AlphaFormat = AC_SRC_ALPHA;
    CSize Size(nWidth, nHeight);
    CPoint SrcPoint(0, 0);
    UpdateLayeredWindow(ghWndIndicator, hdcScreen, &pRuntimeParams->Position, &Size, hdcMemory, &SrcPoint, 0, &BlendFunc, ULW_ALPHA);

    ShowWindowAsync(ghWndIndicator, SW_SHOW);

    SelectObject(hdcMemory, hOldBitmap);
    DeleteObject(hNewBitmap);
    DeleteDC(hdcMemory);
    ReleaseDC(nullptr, hdcScreen);

    if (pRuntimeParams->Showing > 0) {
      SetTimer(ghWndIndicator, IDSHOW_TIMEOUT, MILISECONDS(pRuntimeParams->Showing), nullptr);
    }
  }

  if (pTrayIcon && pTrayIcon->hIcon) {
    Shell_NotifyIcon(NIM_MODIFY, pTrayIcon);
  }
}

std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const TCHAR* lpFilePath, HINSTANCE hInstance, UINT nResourceID) {
  {
    std::unique_ptr<Gdiplus::Image> pfsImage(Gdiplus::Image::FromFile(lpFilePath));
    if (pfsImage && pfsImage->GetLastStatus() == Gdiplus::Ok) {
      return std::unique_ptr<Gdiplus::Image>(pfsImage.release()); // 转交给 shared_ptr
    }
  }

  HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(nResourceID), L"PNG");
  if (!hResource)
    return nullptr;

  HGLOBAL hResourceData = LoadResource(hInstance, hResource);
  if (!hResourceData)
    return nullptr;

  void* pResourceData = LockResource(hResourceData);
  DWORD dwResourceSize = SizeofResource(hInstance, hResource);
  if (!pResourceData || dwResourceSize == 0)
    return nullptr;

  HGLOBAL hResourceBuffer = GlobalAlloc(GMEM_MOVEABLE, dwResourceSize);
  if (!hResourceBuffer)
    return nullptr;

  std::unique_ptr<void, void (*)(void*)> hBufferPtr(hResourceBuffer, [](void* buffer) {
    GlobalFree(buffer);
  });

  void* pResourceBuffer = GlobalLock(hResourceBuffer);
  memcpy(pResourceBuffer, pResourceData, dwResourceSize);
  GlobalUnlock(hResourceBuffer);

  IStream* rawStream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(hResourceBuffer, FALSE, &rawStream)))
    return nullptr;

  std::unique_ptr<IStream, void (*)(IStream*)> pfsStream(rawStream, [](IStream* stream) {
    stream->Release();
  });

  std::unique_ptr<Gdiplus::Image> pfsImage(Gdiplus::Image::FromStream(pfsStream.get()));
  if (pfsImage && pfsImage->GetLastStatus() == Gdiplus::Ok) {
    return std::unique_ptr<Gdiplus::Image>(pfsImage.release());
  }

  return nullptr;
}

VOID LoadParameters(CONST TCHAR* lpCurrentDirectory, RUNTIMEPARAMS* pRuntimeParams) {
  ATLASSERT(ghWndIndicator != nullptr);

  // 获取配置文件路径（本地优先，不存在则从资源中提取）
  CString strConfigPath;
  GetConfigFilePath(lpCurrentDirectory, strConfigPath);

  BOOL fNumLockLoading = (pRuntimeParams == &gNumLockSetup); // 数字锁定参数加载中
  UINT nTrayID = fNumLockLoading ? IDNOTIFY_NUMLOCK : IDNOTIFY_CAPSLOCK;
  UINT nTrayUM = fNumLockLoading ? UMNOTIFY_NUMLOCK : UMNOTIFY_CAPSLOCK;
  SHORT wKeyState = fNumLockLoading ? GetKeyState(VK_NUMLOCK) : GetKeyState(VK_CAPITAL);
  pRuntimeParams->KeyNode = fNumLockLoading ? const_cast<TCHAR*>(TEXT("NumLock")) : const_cast<TCHAR*>(TEXT("CapsLock"));

  TCHAR szTemporaryData[MAX_PATH]{0};
  // # Lock 与 Unlock 图标位置(默认左 30px 上 30px)
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("Position"), TEXT("0,0"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  _stscanf_s(szTemporaryData, TEXT("%d,%d"), &pRuntimeParams->Position.x, &pRuntimeParams->Position.y);

  // 对齐方式(HL: 水平居左 HC: 水平居中 HR: 水平居右 VT: 垂直居上 VC: 垂直居中 VB: 垂直居下)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("Alignment"), TEXT("HC|VC"), szTemporaryData, _countof(szTemporaryData), strConfigPath);

  TCHAR* pszContexts[2];
  pszContexts[0] = _tcstok_s(szTemporaryData, TEXT("|"), (TCHAR**) &pszContexts[1]);
  CString strContexts[2]{pszContexts[0], pszContexts[1]};

  // 开启静音状态
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("EnableMute"), TEXT("true"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  pRuntimeParams->EnableMute = _tcsstr(szTemporaryData, TEXT("true")) != nullptr;

  // # Lock 与 Unlock 置顶图标(左 Lock 右 Unlock)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("DeskImage"), CString(pRuntimeParams->KeyNode) + CString(".png"), szTemporaryData,
                          _countof(szTemporaryData), strConfigPath);

  CString strDeskImagePath = (_tcsstr(szTemporaryData, TEXT(":\\")) || _tcsstr(szTemporaryData, TEXT(":/")))
                                 ? szTemporaryData
                                 : (CString(lpCurrentDirectory) + CString("\\") + szTemporaryData);
  std::shared_ptr<Gdiplus::Image> pfsDeskImage = LoadImageWithFallback(strDeskImagePath, GetModuleHandle(nullptr), pRuntimeParams->ResourceID);
  if (pfsDeskImage != nullptr) {
    int nDeskImageWidth = pfsDeskImage->GetWidth() / 2;
    int nDeskImageHeight = pfsDeskImage->GetHeight();

    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    for (size_t i = 0; i < _countof(strContexts); i++) {
      strContexts[i].Trim().MakeUpper();
      if (TEXT("HC") == strContexts[i]) {
        pRuntimeParams->Position.x += (nScreenWidth - nDeskImageWidth) / 2;
      } else if (TEXT("HR") == strContexts[i]) {
        pRuntimeParams->Position.x += (nScreenWidth - nDeskImageWidth);
      } else if (TEXT("VC") == strContexts[i]) {
        pRuntimeParams->Position.y += (nScreenHeight - nDeskImageHeight) / 2;
      } else if (TEXT("VB") == strContexts[i]) {
        pRuntimeParams->Position.y += (nScreenHeight - nDeskImageHeight);
      }
    }

    pRuntimeParams->LockDeskImage = std::unique_ptr<Gdiplus::Image>(new Gdiplus::Bitmap(nDeskImageWidth, nDeskImageHeight, PixelFormat32bppARGB));
    pRuntimeParams->UnlockDeskImage = std::unique_ptr<Gdiplus::Image>(new Gdiplus::Bitmap(nDeskImageWidth, nDeskImageHeight, PixelFormat32bppARGB));
    std::shared_ptr<Gdiplus::Graphics> pgDeskImageLock(Gdiplus::Graphics::FromImage(pRuntimeParams->LockDeskImage.get()));
    std::shared_ptr<Gdiplus::Graphics> pgDeskImageUnlock(Gdiplus::Graphics::FromImage(pRuntimeParams->UnlockDeskImage.get()));

    pgDeskImageLock->DrawImage(pfsDeskImage.get(), Gdiplus::Rect(0, 0, nDeskImageWidth, nDeskImageHeight), 0, 0, nDeskImageWidth, nDeskImageHeight,
                               Gdiplus::Unit::UnitPixel);
    pgDeskImageUnlock->DrawImage(pfsDeskImage.get(), Gdiplus::Rect(0, 0, nDeskImageWidth, nDeskImageHeight), nDeskImageWidth, 0, nDeskImageWidth,
                                 nDeskImageHeight, Gdiplus::Unit::UnitPixel);
  }
  // # Lock 与 Unlock 窗口隐藏时间(单位：秒)
  pRuntimeParams->Showing = GetPrivateProfileInt(pRuntimeParams->KeyNode, TEXT("Showing"), 1, strConfigPath);

  // # 大写锁定与数字锁定关闭或开启时间(单位：秒)
  pRuntimeParams->Timeout = GetPrivateProfileInt(pRuntimeParams->KeyNode, TEXT("Timeout"), 0, strConfigPath);

  // # Lock 与 Unlock 托盘图标(左边 Lock 右边 Unlock)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("TrayIcon"), CString(pRuntimeParams->KeyNode) + CString(".png"), szTemporaryData,
                          _countof(szTemporaryData), strConfigPath);

  CString strTrayIconPath = (_tcsstr(szTemporaryData, TEXT(":\\")) || _tcsstr(szTemporaryData, TEXT(":/")))
                                ? szTemporaryData
                                : (CString(lpCurrentDirectory) + CString("\\") + szTemporaryData);
  std::shared_ptr<Gdiplus::Image> pfsTrayIcon = LoadImageWithFallback(strTrayIconPath, GetModuleHandle(nullptr), pRuntimeParams->ResourceID);
  if (pfsTrayIcon != nullptr) {
    int nTrayIconWidth = pfsTrayIcon->GetWidth() / 2;
    int nTrayIconHeight = pfsTrayIcon->GetHeight();
    Gdiplus::Bitmap LockTrayIcon(nTrayIconWidth, nTrayIconHeight, PixelFormat32bppARGB);
    Gdiplus::Bitmap UnlockTrayIcon(nTrayIconWidth, nTrayIconHeight, PixelFormat32bppARGB);
    std::shared_ptr<Gdiplus::Graphics> pgTrayIconLock(Gdiplus::Graphics::FromImage(&LockTrayIcon));
    std::shared_ptr<Gdiplus::Graphics> pgTrayIconUnlock(Gdiplus::Graphics::FromImage(&UnlockTrayIcon));

    Gdiplus::Rect destTrayIconRect{0, 0, nTrayIconWidth, nTrayIconHeight};
    pgTrayIconLock->DrawImage(pfsTrayIcon.get(), destTrayIconRect, 0, 0, nTrayIconWidth, nTrayIconHeight, Gdiplus::Unit::UnitPixel);
    pgTrayIconUnlock->DrawImage(pfsTrayIcon.get(), destTrayIconRect, nTrayIconWidth, 0, nTrayIconWidth, nTrayIconHeight, Gdiplus::Unit::UnitPixel);

    pRuntimeParams->LockTrayIcon.cbSize = sizeof(NOTIFYICONDATA);
    pRuntimeParams->LockTrayIcon.hWnd = ghWndIndicator;
    pRuntimeParams->LockTrayIcon.uID = nTrayID;
    pRuntimeParams->LockTrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    pRuntimeParams->LockTrayIcon.uCallbackMessage = nTrayUM;
    LockTrayIcon.GetHICON(&pRuntimeParams->LockTrayIcon.hIcon);
    pRuntimeParams->UnlockTrayIcon.cbSize = sizeof(NOTIFYICONDATA);
    pRuntimeParams->UnlockTrayIcon.hWnd = ghWndIndicator;
    pRuntimeParams->UnlockTrayIcon.uID = nTrayID;
    pRuntimeParams->UnlockTrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    pRuntimeParams->UnlockTrayIcon.uCallbackMessage = nTrayUM;
    UnlockTrayIcon.GetHICON(&pRuntimeParams->UnlockTrayIcon.hIcon);
  }

  // # Lock 音效(支持 wav mp3 格式)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("SoundOn"), CString(pRuntimeParams->KeyNode) + CString(TEXT("On.wav")), szTemporaryData,
                          _countof(szTemporaryData), strConfigPath);

  CString strSoundOnPath = (_tcsstr(szTemporaryData, TEXT(":\\")) || _tcsstr(szTemporaryData, TEXT(":/")))
                               ? szTemporaryData
                               : (CString(lpCurrentDirectory) + CString("\\") + szTemporaryData);

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(strSoundOnPath)) {
    // CString strCommand;
    // strCommand.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(pRuntimeParams->LockNode));
    // mciSendString(strCommand, nullptr, 0, nullptr);
    pRuntimeParams->SoundOn = strSoundOnPath;
  }

  // # Unlock 音效(支持 wav mp3 格式)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->KeyNode, TEXT("SoundOff"), CString(pRuntimeParams->KeyNode) + CString(TEXT("Off.wav")), szTemporaryData,
                          _countof(szTemporaryData), strConfigPath);

  CString strSoundOffPath = (_tcsstr(szTemporaryData, TEXT(":\\")) || _tcsstr(szTemporaryData, TEXT(":/")))
                                ? szTemporaryData
                                : (CString(lpCurrentDirectory) + CString("\\") + szTemporaryData);

  // 打开音乐-播放音乐-停止音乐-关闭音乐
  if (PathFileExists(strSoundOffPath)) {
    // CString strCommand;
    // strCommand.Format(TEXT("open \"%s\" alias %s"), strSoundEffectPath, CString(pRuntimeParams->LockNode));
    // mciSendString(strCommand, nullptr, 0, nullptr);
    pRuntimeParams->SoundOff = strSoundOffPath;
  }

  if (wKeyState == 1) {
    pRuntimeParams->LockSet = LSFW_LOCK;
    if (pRuntimeParams->LockTrayIcon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &pRuntimeParams->LockTrayIcon);
    }
  } else if (wKeyState == 0) {
    pRuntimeParams->LockSet = LSFW_UNLOCK;
    if (pRuntimeParams->LockTrayIcon.hIcon) {
      Shell_NotifyIcon(NIM_ADD, &pRuntimeParams->UnlockTrayIcon);
    }
  }
}

VOID LoadLanguage(CONST TCHAR* lpCurrentDirectory) {
  ATLASSERT(ghMenuIndicator != nullptr);

  // 获取配置文件路径（本地优先，不存在则从资源中提取）
  CString strConfigPath;
  GetConfigFilePath(lpCurrentDirectory, strConfigPath);

  CONST TCHAR* lpLanguageSection = TEXT("Language");
  HMENU hMenuFile = GetSubMenu(ghMenuIndicator, 0);

  TCHAR szTemporaryData[MAX_PATH]{0};
  GetPrivateProfileString(lpLanguageSection, TEXT("FileCaps"), TEXT("Caps Lock"), gszFileCaps, _countof(gszFileCaps), strConfigPath);
  GetPrivateProfileString(lpLanguageSection, TEXT("FileNum"), TEXT("Num Lock"), gszFileNum, _countof(gszFileNum), strConfigPath);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileModify"), TEXT("Modify Settings"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_MODIFY, MF_BYCOMMAND, ID_FILE_MODIFY, szTemporaryData);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileStart"), TEXT("Auto Start"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_START, MF_BYCOMMAND, ID_FILE_START, szTemporaryData);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileExit"), TEXT("Quick Exit"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_EXIT, MF_BYCOMMAND, ID_FILE_EXIT, szTemporaryData);
}

bool ExtractResourceToFile(HINSTANCE hInstance, UINT nResourceID, const TCHAR* szResourceType, const CString& strOutPath) {
  if (PathFileExists(strOutPath))
    return true;

  HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(nResourceID), szResourceType);
  if (!hResource)
    return false;

  HGLOBAL hResourceData = LoadResource(hInstance, hResource);
  if (!hResourceData)
    return false;

  void* pResourceData = LockResource(hResourceData);
  DWORD nResourceSize = SizeofResource(hInstance, hResource);
  if (!pResourceData || nResourceSize == 0)
    return false;

  std::ofstream file(strOutPath, std::ios::binary);
  if (!file.is_open())
    return false;

  file.write(reinterpret_cast<const char*>(pResourceData), nResourceSize);
  return true;
}

INT_PTR CALLBACK IndicatorWndProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (uMsg) {
    case WM_INITDIALOG: {
      ghWndIndicator = hDlg;

      DWORD dwExStyle = GetWindowLong(hDlg, GWL_EXSTYLE);
      SetWindowLong(hDlg, GWL_EXSTYLE, dwExStyle | WS_EX_NOACTIVATE);

      GetModuleFileName(nullptr, gszWorkDir, MAX_PATH);
      *(_tcsrchr(gszWorkDir, TEXT('\\'))) = 0; // 把最后一个 \ 替换为 0

      LoadLanguage(gszWorkDir);
      LoadParameters(gszWorkDir, &gNumLockSetup);
      LoadParameters(gszWorkDir, &gCapsLockSetup);

      // SetTimer(hDlg, IDEVENT_SCANNING, 10, nullptr);
      ghKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);

      SHORT wNumLockSet = GetKeyState(VK_NUMLOCK) ? LSFW_LOCK : LSFW_UNLOCK;
      SHORT wCapsLockSet = GetKeyState(VK_CAPITAL) ? LSFW_LOCK : LSFW_UNLOCK;
      // 当前是灯灭状态，就重置/启动计时器
      if (wNumLockSet == LSFW_UNLOCK && gNumLockSetup.Timeout > 0) {
        gNumLockSetup.IDEvent = SetTimer(ghWndIndicator, IDNUM_TIMEOUT, MILISECONDS(gNumLockSetup.Timeout), nullptr);
      }

      // 当前是灯亮状态，就重置/启动计时器
      if (wCapsLockSet == LSFW_LOCK && gCapsLockSetup.Timeout > 0) {
        gCapsLockSetup.IDEvent = SetTimer(ghWndIndicator, IDCAPS_TIMEOUT, MILISECONDS(gCapsLockSetup.Timeout), nullptr);
      }
      return (INT_PTR) TRUE;
    }
    case WM_COMMAND: {
      if (wParam == ID_FILE_MODIFY) {
        CString strAbsolutePath;
        strAbsolutePath = CString(gszWorkDir) + CString("\\NumLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_NUMLOCK, TEXT("PNG"), strAbsolutePath);
        strAbsolutePath = CString(gszWorkDir) + CString("\\CapsLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_CAPSLOCK, TEXT("PNG"), strAbsolutePath);
        strAbsolutePath = CString(gszWorkDir) + CString("\\AppSettings.ini");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDR_APPSETTINGS, RT_RCDATA, strAbsolutePath);
        // 打开配置文件
        ShellExecute(nullptr, nullptr, strAbsolutePath, nullptr, nullptr, SW_SHOW);
      } else if (wParam == ID_FILE_START) {
        HMENU hMenuFile = GetSubMenu(ghMenuIndicator, 0);
        DWORD dwChecked = CheckMenuItem(hMenuFile, ID_FILE_START, 0);
        if (dwChecked == MF_CHECKED) {
          CurrentVersionRun(IDACTION_CANCEL);
          CheckMenuItem(hMenuFile, ID_FILE_START, MF_UNCHECKED);
        } else if (dwChecked == MF_UNCHECKED) {
          CurrentVersionRun(IDACTION_APPEND);
          CheckMenuItem(hMenuFile, ID_FILE_START, MF_CHECKED);
        }
      } else if (wParam == ID_FILE_EXIT) {
        EndDialog(hDlg, lParam);
      }
      return (INT_PTR) TRUE;
    }
    case UMNOTIFY_CAPSLOCK:
    case UMNOTIFY_NUMLOCK: {
      if (LOWORD(lParam) == WM_RBUTTONUP) {
        SetForegroundWindow(hDlg); // 没有这个菜单不会自动消失

        POINT destPopupPoint;
        GetCursorPos(&destPopupPoint);
        HMENU hMenuFile = GetSubMenu(ghMenuIndicator, 0);
        CheckMenuItem(hMenuFile, ID_FILE_START, CurrentVersionRun(IDACTION_CHECK) ? MF_CHECKED : MF_UNCHECKED);
        if (uMsg == UMNOTIFY_NUMLOCK) {
          ModifyMenu(hMenuFile, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, gszFileNum);
          CheckMenuItem(hMenuFile, ID_FILE_ABOUT, GetKeyState(VK_NUMLOCK) ? MF_CHECKED : MF_UNCHECKED);
        } else if (uMsg == UMNOTIFY_CAPSLOCK) {
          ModifyMenu(hMenuFile, ID_FILE_ABOUT, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED, ID_FILE_ABOUT, gszFileCaps);
          CheckMenuItem(hMenuFile, ID_FILE_ABOUT, GetKeyState(VK_CAPITAL) ? MF_CHECKED : MF_UNCHECKED);
        }
        TrackPopupMenu(hMenuFile, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERTICAL, destPopupPoint.x, destPopupPoint.y, 0, hDlg, nullptr);
      } else if (LOWORD(lParam) == WM_LBUTTONUP) {
        DrawImageIcon((uMsg == UMNOTIFY_NUMLOCK) ? &gNumLockSetup : &gCapsLockSetup);
      }
      return (INT_PTR) TRUE;
    }
    case WM_TIMER: {
      if (wParam == IDNUM_TIMEOUT) {
        KillTimer(hDlg, wParam);
        gNumLockSetup.IDEvent = 0;
        SHORT wNumLockSet = ((GetKeyState(VK_NUMLOCK) & 0x0001) != 0) ? LSFW_LOCK : LSFW_UNLOCK;
        if (wNumLockSet == LSFW_UNLOCK) {
          keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
          keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        }
        // SendMessage(ghWndIndicator, UMNOTIFY_PROCESS, VK_NUMLOCK, TRUE);
      } else if (wParam == IDCAPS_TIMEOUT) {
        KillTimer(hDlg, wParam);
        gCapsLockSetup.IDEvent = 0;
        SHORT wCapsLockSet = (GetKeyState(VK_CAPITAL) & 0x0001) ? LSFW_LOCK : LSFW_UNLOCK;
        if (wCapsLockSet == LSFW_LOCK) {
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 1);
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 1);
        }
        // SendMessage(ghWndIndicator, UMNOTIFY_PROCESS, VK_CAPITAL, TRUE);
      } else if (wParam == IDSHOW_TIMEOUT) {
        KillTimer(hDlg, wParam);
        ShowWindowAsync(hDlg, SW_HIDE);
      } else if (wParam == IDHOOK_TIMEOUT) {
        KillTimer(hDlg, wParam);
        int vkCode = (int) (INT_PTR) GetProp(hDlg, PROP_VKCODE);
        KeyLockLogic(vkCode);
      }
      return (INT_PTR) TRUE;
    }
    case WM_CLOSE: {
      EndDialog(hDlg, lParam);
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

BOOL CurrentVersionRun(UINT uMsg) {
  HKEY hRegKey{nullptr};
  LSTATUS lRetValue = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hRegKey);
  if (lRetValue == ERROR_SUCCESS) /// 打开启动项
  {
    if (uMsg == IDACTION_CANCEL) {
      lRetValue = RegDeleteValue(hRegKey, PACKAGE_NAME);
    } else {
      TCHAR szAppPath[MAX_PATH]{0};
      GetModuleFileName(nullptr, szAppPath, MAX_PATH);
      if (uMsg == IDACTION_APPEND) {
        lRetValue = RegSetValueEx(hRegKey, PACKAGE_NAME, 0, REG_SZ, (LPBYTE) szAppPath, lstrlen(szAppPath) * sizeof(TCHAR));
      } else {
        DWORD dwRegType = 0;
        TCHAR szRegValue[MAX_PATH]{0};
        DWORD dwRegValueLen = _countof(szRegValue) * sizeof(TCHAR);
        lRetValue = RegQueryValueEx(hRegKey, PACKAGE_NAME, nullptr, &dwRegType, (LPBYTE) szRegValue,
                                    &dwRegValueLen); // 支持 XP 32 位系统

        if (lRetValue == ERROR_SUCCESS) {
          if (_tcsicmp(szAppPath, szRegValue) != 0) {
            lRetValue = (LSTATUS) -1; // 注册表保存的并非当前应用程序路径
          }
        }
      }
    }

    RegCloseKey(hRegKey);
  }
  return (lRetValue == ERROR_SUCCESS);
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  WPARAM dwExitCode = 0;
  HANDLE hSingletonMutex = CreateMutex(nullptr, FALSE, TEXT("COM.HNSFNET.KEYLOCK"));
  if (hSingletonMutex) {
    DWORD dwWaitResult = WaitForSingleObject(hSingletonMutex, 0);
    if (dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_ABANDONED) {
      // GDI+ Startup
      ULONG_PTR gdiStartupToken;
      Gdiplus::GdiplusStartupInput gdiStartupInput;
      Gdiplus::GdiplusStartup(&gdiStartupToken, &gdiStartupInput, nullptr);

      ghMenuIndicator = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_KEYLOCK));

      WNDCLASSEX WndClassEx;
      WndClassEx.cbSize = sizeof(WNDCLASSEX);
      GetClassInfoEx(nullptr, WC_DIALOG, &WndClassEx);
      WndClassEx.lpszClassName = TEXT("KEYLOCK");
      WndClassEx.style &= ~CS_GLOBALCLASS;
      RegisterClassEx(&WndClassEx);

      DialogBox(hInstance, MAKEINTRESOURCE(IDD_KEYLOCK), nullptr, IndicatorWndProc);

      MSG msgThread;
      while (GetMessage(&msgThread, nullptr, 0, 0)) {
        TranslateMessage(&msgThread);
        DispatchMessage(&msgThread);
      }
      dwExitCode = msgThread.wParam;

      //  GDI+ Shutdown
      Gdiplus::GdiplusShutdown(gdiStartupToken);

      ReleaseMutex(hSingletonMutex);
    }
    CloseHandle(hSingletonMutex);
  }
  return (int) dwExitCode;
}
