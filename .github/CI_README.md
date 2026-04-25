# GitHub Actions CI/CD 配置

本项目包含以下 GitHub Actions 工作流，用于自动化编译和发布流程。

## 工作流文件说明

### 1. 📦 Build Workflow (`.github/workflows/build.yml`)

**触发条件:**
- 推送到 `main` 或 `develop` 分支
- 提交 Pull Request 到 `main` 或 `develop` 分支

**功能:**
- 编译 Release x64 配置
- 上传编译产物为 GitHub Artifacts（保留 30 天）
- 验证编译输出

**使用场景:** 每次代码变更时自动验证编译是否成功

---

### 2. 🚀 Release Workflow (`.github/workflows/release.yml`)

**触发条件:**
- 推送标签，格式为 `v*`（例如：`v1.0.0`）

**功能:**
- 编译 Release x64 版本
- 创建 GitHub Release
- 自动上传可执行文件到 Release 页面

**使用方法:**
```bash
git tag v1.0.0
git push origin v1.0.0
```

**使用场景:** 发布新版本时自动创建发布和上传二进制文件

---

### 3. 🔍 Code Quality Workflow (`.github/workflows/code-quality.yml`)

**触发条件:**
- 推送到 `main` 或 `develop` 分支
- 提交 Pull Request 到 `main` 或 `develop` 分支

**功能:**
- 运行 Visual Studio 代码分析
- 检查编译警告和错误
- 生成代码质量报告

**使用场景:** 保证代码质量，在合并前检查问题

---

## 工作流状态

在 GitHub 仓库主页可以看到所有工作流的状态。点击 "Actions" 标签查看详细信息。

| 工作流 | 触发条件 | 功能 | 产物保留 |
|------|--------|------|--------|
| Build | Push/PR | Release x64 编译 | 30天 |
| Release | Git 标签 | 编译+发布 | 永久 |
| Code Quality | Push/PR | 代码分析 | - |

## 编译产物

### 常规编译 (Push/PR)
- Release x64 版本编译
- 保存在 GitHub Artifacts 中，保留 **30 天**
- 下载方法：
  1. 进入 Actions 标签
  2. 选择相应的工作流运行
  3. 滚动到 "Artifacts" 部分
  4. 下载 `KeyLock-Release-x64` 产物

### Release 编译
- Release x64 版本编译
- 保存在 GitHub Releases 页面，**永久保留**
- 自动上传可执行文件和 PDB 文件
- 可从仓库的 Releases 页面直接下载

---

## 配置详情

### 编译环境
- **操作系统:** Windows Latest (Windows Server 2022)
- **Visual Studio:** 16.0 (2019) 及更高版本
- **MSBuild:** 自动包含

### 输出路径
- Release 版本: `Bin/Release/`
- 临时文件: `Obj/Release/`

### 编译配置
仅编译 **Release|x64** 配置，生产就绪

---

## 常见问题

### Q: 编译失败了怎么办？
A: 检查工作流日志：
1. 进入 GitHub Actions
2. 找到失败的工作流运行
3. 查看详细日志了解错误原因

### Q: 如何手动运行工作流？
A: GitHub Actions 支持手动触发（需要在工作流文件中配置 `workflow_dispatch`）

### Q: 能否修改编译配置？
A: 可以编辑工作流文件中的 `matrix` 部分修改编译组合

---

## 相关文档
- [GitHub Actions 文档](https://docs.github.com/zh/actions)
- [MSBuild 文档](https://docs.microsoft.com/zh-cn/visualstudio/msbuild/msbuild)
- [setup-msbuild Action](https://github.com/microsoft/setup-msbuild)
