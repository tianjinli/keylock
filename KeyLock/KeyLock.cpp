#include "KeyLock.h"

#include <atlstr.h>
#include <atltypes.h>
#include <shlwapi.h>

#include <fstream>
#include <memory>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

#define PACKAGE_NAME TEXT("HNSFNETKL")

#define CONFIG_NAME TEXT("AppSettings.ini")

// 检查开机启动ID
#define IDACTION_CHECK (WM_USER + 0)
// 添加开机启动ID
#define IDACTION_APPEND (WM_USER + 1)
// 取消开机启动ID
#define IDACTION_CANCEL (WM_USER + 2)

// 显示超时事件ID
#define IDEVENT_DISPLAY (WM_USER + 1)
// 按键扫描事件ID
#define IDEVENT_SCANNING (WM_USER + 2)
// 大写超时事件ID
#define IDCAPS_TIMEOUT (WM_USER + 3)
// 数字超时事件ID
#define IDNUM_TIMEOUT (WM_USER + 4)

// 数字锁定托盘图标ID
#define IDNOTIFY_NUMLOCK (WM_USER + 1)
// 大写锁定托盘图标ID
#define IDNOTIFY_CAPSLOCK (WM_USER + 2)

// 数字锁定托盘事件ID
#define UMNOTIFY_NUMLOCK (WM_USER + 1)
// 大写锁定托盘事件ID
#define UMNOTIFY_CAPSLOCK (WM_USER + 2)

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

// ALT 已按下
static bool gbAltDown = false;
// ALT+1 已按下
static bool gbAlt1Down = false;
// ALT+2 已按下
static bool gbAlt2Down = false;

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
  
  // 删除临时配置文件
  if (!gstrTempConfigPath.IsEmpty() && PathFileExists(gstrTempConfigPath)) {
    DeleteFile(gstrTempConfigPath);
    gstrTempConfigPath.Empty();
  }
  
  // 注销窗口类
  UnregisterClass(TEXT("KEYLOCK"), GetModuleHandle(nullptr));
}

VOID DrawImageIcon(RUNTIMEPARAMS* pRuntimeParams, const SHORT wCurrentLockSet) {
  Gdiplus::Image* pDeskImage = nullptr;
  NOTIFYICONDATA* pTrayIcon = nullptr;
  const SHORT wLastLockSet = pRuntimeParams->LockSet;
  CString strSoundEffect;
  if (wCurrentLockSet) {
    if (wCurrentLockSet == LSFW_LOCK && wLastLockSet != LSFW_LOCK) {
      pRuntimeParams->LockSet = LSFW_LOCK;
      pDeskImage = pRuntimeParams->LockDeskImage.get();
      pTrayIcon = &pRuntimeParams->LockTrayIcon;
      strSoundEffect = pRuntimeParams->SoundOn;
    } else if (wCurrentLockSet == LSFW_UNLOCK && wLastLockSet != LSFW_UNLOCK) {
      pRuntimeParams->LockSet = LSFW_UNLOCK;
      pDeskImage = pRuntimeParams->UnlockDeskImage.get();
      pTrayIcon = &pRuntimeParams->UnlockTrayIcon;
      strSoundEffect = pRuntimeParams->SoundOff;
    } else {
      return;
    }
  } else {
    switch (wLastLockSet) {
      case LSFW_LOCK:
        pDeskImage = pRuntimeParams->LockDeskImage.get();
        pTrayIcon = &pRuntimeParams->LockTrayIcon;
        break;
      case LSFW_UNLOCK:
        pDeskImage = pRuntimeParams->UnlockDeskImage.get();
        pTrayIcon = &pRuntimeParams->UnlockTrayIcon;
        break;
      default:
        return;
    }
  }

  if (wCurrentLockSet) { // 单击托盘不播放音效
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

    if (pRuntimeParams->Display > 0) {
      SetTimer(ghWndIndicator, IDEVENT_DISPLAY, MILISECONDS(pRuntimeParams->Display), nullptr);
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
  pRuntimeParams->LockNode = fNumLockLoading ? const_cast<TCHAR*>(TEXT("NumLock")) : const_cast<TCHAR*>(TEXT("CapsLock"));

  TCHAR szTemporaryData[MAX_PATH]{0};
  // # Lock 与 Unlock 图标位置(默认左 30px 上 30px)
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("Position"), TEXT("0,0"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  _stscanf_s(szTemporaryData, TEXT("%d,%d"), &pRuntimeParams->Position.x, &pRuntimeParams->Position.y);

  // 对齐方式(HL: 水平居左 HC: 水平居中 HR: 水平居右 VT: 垂直居上 VC: 垂直居中
  // VB: 垂直居下)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("Alignment"), TEXT("HC|VC"), szTemporaryData, _countof(szTemporaryData), strConfigPath);

  TCHAR* pszContexts[2];
  pszContexts[0] = _tcstok_s(szTemporaryData, TEXT("|"), (TCHAR**) &pszContexts[1]);
  CString strContexts[2]{pszContexts[0], pszContexts[1]};

  // # Lock 与 Unlock 置顶图标(左 Lock 右 Unlock)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("DeskImage"), CString(pRuntimeParams->LockNode) + CString(".png"), szTemporaryData,
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
  pRuntimeParams->Display = GetPrivateProfileInt(pRuntimeParams->LockNode, TEXT("Display"), 1, strConfigPath);

  // # 大写锁定与数字锁定关闭或开启时间(单位：秒)
  pRuntimeParams->Timeout = GetPrivateProfileInt(pRuntimeParams->LockNode, TEXT("Timeout"), 0, strConfigPath);

  // # Lock 与 Unlock 托盘图标(左边 Lock 右边 Unlock)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("TrayIcon"), CString(pRuntimeParams->LockNode) + CString(".png"), szTemporaryData,
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
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("SoundOn"), CString(pRuntimeParams->LockNode) + CString(TEXT("On.wav")), szTemporaryData,
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

  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("IgnoreList"), CString(TEXT("133")), szTemporaryData, _countof(szTemporaryData),
                          strConfigPath);
  int pos = 0;
  std::set<int> values;
  CString strIgnoreList(szTemporaryData);
  CString token = strIgnoreList.Tokenize(_T(","), pos);
  while (!token.IsEmpty()) {
    values.insert(_ttoi(token)); // 转 int 并放入 set
    token = strIgnoreList.Tokenize(_T(","), pos);
  }
  pRuntimeParams->IgnoreList = values;

  // # Unlock 音效(支持 wav mp3 格式)
  ZeroMemory(szTemporaryData, sizeof(szTemporaryData));
  GetPrivateProfileString(pRuntimeParams->LockNode, TEXT("SoundOff"), CString(pRuntimeParams->LockNode) + CString(TEXT("Off.wav")), szTemporaryData,
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
  GetPrivateProfileString(lpLanguageSection, TEXT("FileStart"), TEXT("Auto Start"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_START, MF_BYCOMMAND, ID_FILE_START, szTemporaryData);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileRelease"), TEXT("Release Files"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_RELEASE, MF_BYCOMMAND, ID_FILE_RELEASE, szTemporaryData);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileExit"), TEXT("Quick Exit"), szTemporaryData, _countof(szTemporaryData), strConfigPath);
  ModifyMenu(hMenuFile, ID_FILE_EXIT, MF_BYCOMMAND, ID_FILE_EXIT, szTemporaryData);

  GetPrivateProfileString(lpLanguageSection, TEXT("FileCaps"), TEXT("Caps Lock"), gszFileCaps, _countof(gszFileCaps), strConfigPath);
  GetPrivateProfileString(lpLanguageSection, TEXT("FileNum"), TEXT("Num Lock"), gszFileNum, _countof(gszFileNum), strConfigPath);
}

bool ExtractResourceToFile(HINSTANCE hInstance, UINT nResourceID, const TCHAR* szResourceType, const CString& strOutPath) {
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
      LoadParameters(gszWorkDir, &gCapsLockSetup);
      LoadParameters(gszWorkDir, &gNumLockSetup);

      SetTimer(hDlg, IDEVENT_SCANNING, 10, nullptr);
      return (INT_PTR) TRUE;
    }
    case WM_COMMAND: {
      if (wParam == ID_FILE_START) {
        HMENU hMenuFile = GetSubMenu(ghMenuIndicator, 0);
        DWORD dwChecked = CheckMenuItem(hMenuFile, ID_FILE_START, 0);
        if (dwChecked == MF_CHECKED) {
          CurrentVersionRun(IDACTION_CANCEL);
          CheckMenuItem(hMenuFile, ID_FILE_START, MF_UNCHECKED);
        } else if (dwChecked == MF_UNCHECKED) {
          CurrentVersionRun(IDACTION_APPEND);
          CheckMenuItem(hMenuFile, ID_FILE_START, MF_CHECKED);
        }
      } else if (wParam == ID_FILE_RELEASE) {
        CString strAbsolutePath = CString(gszWorkDir) + CString("\\AppSettings.ini");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDR_APPSETTINGS, RT_RCDATA, strAbsolutePath);
        strAbsolutePath = CString(gszWorkDir) + CString("\\CapsLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_CAPSLOCK, TEXT("PNG"), strAbsolutePath);
        strAbsolutePath = CString(gszWorkDir) + CString("\\NumLock.png");
        ExtractResourceToFile(GetModuleHandle(nullptr), IDB_NUMLOCK, TEXT("PNG"), strAbsolutePath);
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
      if (wParam == IDEVENT_DISPLAY) {
        KillTimer(hDlg, wParam);
        ShowWindowAsync(hDlg, SW_HIDE);
      } else if (wParam == IDEVENT_SCANNING) {
        // 原有 NumLock / CapsLock 自动检测逻辑
        SHORT wNumLockSet = GetKeyState(VK_NUMLOCK) ? LSFW_LOCK : LSFW_UNLOCK;
        SHORT wCapsLockSet = GetKeyState(VK_CAPITAL) ? LSFW_LOCK : LSFW_UNLOCK;

        bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000);

        bool key1Pressed = (GetAsyncKeyState('1') & 0x8000);
        bool key2Pressed = (GetAsyncKeyState('2') & 0x8000);

        // ALT+1：显示 NumLock 状态
        if (altPressed && key1Pressed && !gbAlt1Down) {
          gbAlt1Down = true;
          wNumLockSet = 0;
        }

        // ALT+2：显示 CapsLock 状态
        if (altPressed && key2Pressed && !gbAlt2Down) {
          gbAlt2Down = true;
          wCapsLockSet = 0;
        }

        if (gbAlt1Down || gbAlt2Down) {
          gbAlt1Down = false;
          gbAlt2Down = false;
        }

        if (gNumLockSetup.IDEvent) {
          if (wNumLockSet == LSFW_LOCK) {
            KillTimer(hDlg, gNumLockSetup.IDEvent);
            gNumLockSetup.IDEvent = 0;
          }
        } else if (gNumLockSetup.Timeout > 0) {
          gNumLockSetup.IDEvent = SetTimer(hDlg, IDNUM_TIMEOUT, MILISECONDS(gNumLockSetup.Timeout), nullptr);
        }
        DrawImageIcon(&gNumLockSetup, wNumLockSet);

        if (gCapsLockSetup.IDEvent) {
          if (wCapsLockSet == LSFW_UNLOCK) {
            KillTimer(hDlg, gCapsLockSetup.IDEvent);
            gCapsLockSetup.IDEvent = 0;
          }
        } else if (gCapsLockSetup.Timeout > 0) {
          gCapsLockSetup.IDEvent = SetTimer(hDlg, IDCAPS_TIMEOUT, MILISECONDS(gCapsLockSetup.Timeout), nullptr);
        }
        DrawImageIcon(&gCapsLockSetup, wCapsLockSet);

        // 检测是否有任意按键按下
        for (int vk = 0x08; vk <= 0xFE; vk++) {
          if (GetAsyncKeyState(vk) & 0x8000) {
            if (gNumLockSetup.IDEvent) {
              if (gNumLockSetup.IgnoreList.find(vk) != gNumLockSetup.IgnoreList.end())
                continue;
              KillTimer(hDlg, gNumLockSetup.IDEvent);
              gNumLockSetup.IDEvent = 0;
            }
            if (gCapsLockSetup.IDEvent) {
              if (gCapsLockSetup.IgnoreList.find(vk) != gCapsLockSetup.IgnoreList.end())
                continue;
              KillTimer(hDlg, gCapsLockSetup.IDEvent);
              gCapsLockSetup.IDEvent = 0;
            }
            break;
          }
        }
      } else if (wParam == IDNUM_TIMEOUT) {
        KillTimer(hDlg, wParam);
        gNumLockSetup.IDEvent = 0;
        keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
      } else if (wParam == IDCAPS_TIMEOUT) {
        KillTimer(hDlg, wParam);
        gCapsLockSetup.IDEvent = 0;
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 1);
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 1);
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
