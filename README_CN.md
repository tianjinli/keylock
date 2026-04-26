# KeyLock - 键盘锁定状态指示器

<div align="center">

![Windows](https://img.shields.io/badge/Platform-Windows-blue)
![C++](https://img.shields.io/badge/Language-C%2B%2B-orange)
![License](https://img.shields.io/badge/License-MIT-green)

[English](README.md) | 中文

一个轻量级的 Win32 原生键盘锁定状态指示器，实时显示数字锁定 (Num Lock)、大写锁定 (Caps Lock) 和滚动锁定 (Scroll Lock) 的状态。

</div>

---

## 📌 功能特性

✅ **实时状态显示**
- 实时监控 Num Lock、Caps Lock 和 Scroll Lock 状态
- 自定义桌面浮窗位置和对齐方式
- 支持自定义指示器图标（PNG 格式）

✅ **快捷键支持**
- 支持自定义配置文件 (AppSettings.ini)
- `ALT+1` 快速显示数字锁定状态
- `ALT+2` 快速显示大写锁定状态
- `ALT+3` 快速显示滚动锁定状态

✅ **多媒体反馈**
- 支持自定义音效（WAV/MP3 格式）
- 锁定和解锁时分别播放不同音效
- 可禁用或自定义提示音

✅ **系统集成**
- 支持开机自启动设置
- 系统托盘集成，右键菜单快捷操作
- 极低内存占用（纯 Win32 API 实现）

✅ **高级特性**
- 支持内嵌资源配置，无需额外文件也能运行
- 多语言支持（配置文件可切换）
- 支持 x64 和 x86 架构

---

## 🚀 快速开始

### 系统要求
- **操作系统**: Windows XP 及以上版本
- **架构**: x64
- **.NET Framework**: 无需依赖

### 下载和安装

1. 从 [Releases](../../releases) 页面下载最新版本的 `KeyLock.exe`
2. 将 `KeyLock.exe` 放到任意目录
3. （可选）复制 `AppSettings.ini` 和 `*.png` 文件到同一目录来自定义设置

### 首次运行

```bash
KeyLock.exe
```

程序会自动在桌面右上角显示当前的 Num Lock、Caps Lock 和 Scroll Lock 状态。

右键点击托盘图标可以：
- 查看当前锁定状态
- 设置开机自启动
- 退出程序

---

## 🎮 快捷键

| 快捷键 | 功能 |
|------|------|
| `ALT+1` | 显示 Num Lock 状态 |
| `ALT+2` | 显示 Caps Lock 状态 |
| `ALT+3` | 显示 Scroll Lock 状态 |
| 右键单击托盘图标 | 打开菜单 |

---

## 🛠️ 开发和编译

### 系统要求
- Visual Studio 2019 或更高版本
- Windows SDK
- CMake（可选）

### 编译步骤

```bash
# 克隆仓库
git clone https://github.com/tianjinli/keylock.git
cd keylock

# 使用 Visual Studio 打开
start KeyLock.sln

# 或使用 MSBuild 编译
msbuild KeyLock.sln /p:Configuration=Release /p:Platform=x64
```

编译输出位于 `Bin/Release/` 目录。

---

## 🔄 CI/CD 流程

本项目使用 GitHub Actions 自动化构建流程：

- **Build Workflow**: 每次 push/PR 时自动编译 Release x64 版本
- **Release Workflow**: 推送 Git 标签 (v*.*.*) 时自动发布新版本
- **Code Quality**: 代码质量检查

详见 [.github/CI_README.md](.github/CI_README.md)

---

## 📝 许可证

本项目采用 MIT 许可证。

**注意**: 本项目中包含的图片和音效素材来自 Acer Quick Access，仅供学习参考使用，不得用于商业目的。

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 报告 Bug

请在 [Issues](../../issues) 中描述：
- 操作系统版本
- 程序版本
- 复现步骤
- 预期行为和实际行为

### 提交改进

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

---

## 📚 相关资源

- [Windows API 文档](https://docs.microsoft.com/en-us/windows/win32/api/)
- [GDI+ 参考](https://docs.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-start)
- [虚拟键码表](https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)

---

## 🙋 常见问题

**Q: 为什么要用纯 Win32 API 实现？**  
A: 更低的资源占用，无额外依赖，启动速度快。

**Q: 支持哪些 Windows 版本？**  
A: Windows XP 及以上版本。建议使用 Windows 7 或更新版本。

**Q: 可以关闭音效吗？**  
A: 可以，在配置文件中注释掉 SoundOn 和 SoundOff，或将其设置为空。

**Q: 如何自定义指示器图标？**  
A: 编辑 AppSettings.ini，修改 DeskImage 和 TrayIcon 参数，指向你的 PNG 文件。

---

## 📧 联系方式

如有问题，欢迎提交 Issue 或 Pull Request。

---

<div align="center">

**[返回顶部](#keylock---键盘锁定状态指示器)**

Made with ❤️ by [tianjinli](https://github.com/tianjinli)

</div>