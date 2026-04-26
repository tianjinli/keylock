# KeyLock - Keyboard Lock Status Indicator

<div align="center">

![Windows](https://img.shields.io/badge/Platform-Windows-blue)
![C++](https://img.shields.io/badge/Language-C%2B%2B-orange)
![License](https://img.shields.io/badge/License-MIT-green)

[中文](README_CN.md) | English

A lightweight Win32 native keyboard lock state indicator that displays Num Lock, Caps Lock and Scroll Lock status in real-time.

</div>

---

## 📌 Features

✅ **Real-time State Display**
- Monitor Num Lock, Caps Lock and Scroll Lock status in real-time
- Customizable desktop floating window position and alignment
- Support for custom indicator icons (PNG format)

✅ **Keyboard Shortcuts**
- Customizable configuration file (AppSettings.ini)
- `ALT+1` Quick display of Num Lock status
- `ALT+2` Quick display of Caps Lock status
- `ALT+3` Quick display of Scroll Lock status

✅ **Multimedia Feedback**
- Support for custom sound effects (WAV/MP3 format)
- Different sounds for lock and unlock events
- Optional or customizable notification sounds

✅ **System Integration**
- Auto-start support
- System tray integration with right-click menu
- Minimal memory footprint (pure Win32 API implementation)

✅ **Advanced Features**
- Embedded resource configuration - works without external files
- Multi-language support (configurable)
- Supports both x64 and x86 architectures

---

## 🚀 Quick Start

### System Requirements
- **Operating System**: Windows 8.1 and above
- **Architecture**: x64
- **.NET Framework**: None required

### Download and Install

1. Download the latest version of `KeyLock.exe` from [Releases](../../releases)
2. Place `KeyLock.exe` in any directory
3. (Optional) Copy `AppSettings.ini` and `*.png` files to the same directory for customization

### First Run

```bash
KeyLock.exe
```

The program will automatically display the current Num Lock, Caps Lock and Scroll Lock status in the upper right corner of your desktop.

Right-click the system tray icon to:
- View current lock status
- Enable auto-start on boot
- Exit the program

---

## 🎮 Keyboard Shortcuts

| Shortcut | Function |
|----------|----------|
| `ALT+1` | Show Num Lock status |
| `ALT+2` | Show Caps Lock status |
| `ALT+3` | Show Scroll Lock status |
| Right-click tray icon | Open menu |

---

## 🛠️ Development and Building

### System Requirements
- Visual Studio 2019 or later
- Windows SDK
- CMake (optional)

### Build Steps

```bash
# Clone repository
git clone https://github.com/tianjinli/keylock.git
cd keylock

# Open with Visual Studio
start KeyLock.sln

# Or build with MSBuild
msbuild KeyLock.sln /p:Configuration=Release /p:Platform=x64
```

Build output is located in the `Bin/Release/` directory.

---

## 🔄 CI/CD Pipeline

This project uses GitHub Actions for automated building:

- **Build Workflow**: Automatically compiles Release x64 on every push/PR
- **Release Workflow**: Auto-publishes new versions when Git tags are pushed (v*.*.*)
- **Code Quality**: Code quality checks

See [.github/CI_README.md](.github/CI_README.md) for details

---

## 📝 License

This project is licensed under the MIT License.

**Note**: The images and sound effects in this project are sourced from Acer Quick Access and are for educational reference only. Do not use commercially.

---

## 🤝 Contributing

Issues and Pull Requests are welcome!

### Report a Bug

Please describe in [Issues](../../issues):
- Windows version
- Program version
- Steps to reproduce
- Expected vs actual behavior

### Submit Improvements

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📚 Resources

- [Windows API Documentation](https://docs.microsoft.com/en-us/windows/win32/api/)
- [GDI+ Reference](https://docs.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-start)
- [Virtual Key Codes](https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)

---

## 🙋 FAQ

**Q: Why implement using pure Win32 API?**  
A: Lower resource usage, no external dependencies, faster startup.

**Q: Which Windows versions are supported?**  
A: Windows 8.1 and above. Windows 10 or newer is recommended.

**Q: Can I disable sound effects?**  
A: Yes, comment out or leave empty SoundOn and SoundOff in the configuration file.

**Q: How do I customize the indicator icon?**  
A: Edit AppSettings.ini, modify DeskImage and TrayIcon parameters to point to your PNG files.

---

## 📧 Contact

For questions or issues, please submit an Issue or Pull Request.

---

<div align="center">

**[Back to Top](#keylock---keyboard-lock-status-indicator)**

Made with ❤️ by [tianjinli](https://github.com/tianjinli)

</div>