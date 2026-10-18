<p align="center">
  <img src="Base/res/icons/thoth.png" alt="Thoth Browser" width="112">
</p>

<h1 align="center">Thoth Browser</h1>

<p align="center"><em>探索 AI 时代的人与浏览器交互。</em></p>

<p align="center"><a href="README.md">English</a> · <a href="README.zh-CN.md">简体中文</a></p>

<p align="center">
  <a href="https://github.com/Aoody743/thoth-browser/releases"><img src="https://img.shields.io/badge/版本-0.9.1-c49b59" alt="版本 0.9.1"></a>
  <img src="https://img.shields.io/badge/macOS-14.4%2B-252c35" alt="macOS 14.4 或更高版本">
  <img src="https://img.shields.io/badge/Apple%20Silicon-arm64-252c35" alt="Apple Silicon">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-BSD--2--Clause-252c35" alt="BSD 2-Clause 许可证"></a>
</p>

<p align="center">
  <a href="https://github.com/Aoody743/thoth-browser/releases">发行版本</a>
  &nbsp;·&nbsp;
  <a href="https://github.com/Aoody743/thoth-browser/issues/new">反馈问题</a>
  &nbsp;·&nbsp;
  <a href="https://ladybird.org/">Ladybird 浏览器引擎</a>
</p>

---

Thoth 是一款面向 macOS 的实验性浏览器，基于
[Ladybird](https://github.com/LadybirdBrowser/ladybird) 引擎构建。垂直侧边栏将工作区和网页整合在一处，保持界面安静紧凑，让网页拥有更多空间。

## 用不同方式整理网页

- **工作区**：按项目整理网页，每个工作区都可以设置自己的颜色和图标。
- **垂直侧边栏**：集中放置收藏、文件夹、固定网页和临时网页，不必在网页上方再增加一排标签。
- **分屏浏览**：在同一个侧边栏条目下并排查看两个网页。
- **恢复会话**：重启后继续使用原来的工作区和已打开的网页。
- **专注地址栏**：地址栏会跟随当前正在使用的网页。

## 下载

0.9.1 版本面向 **Apple Silicon（arm64）**，要求 **macOS 14.4 或更高版本**。经过 Developer ID 签名和 Apple 公证的 DMG 可在
[0.9.1 发行页](https://github.com/Aoody743/thoth-browser/releases/tag/v0.9.1) 下载。

安装时，打开下载的 `.dmg`，然后将 **Thoth Browser** 拖入 **Applications（应用程序）** 文件夹。

## 常用快捷键

| 操作 | 快捷键 |
| --- | --- |
| 聚焦地址栏 | `⌘L` |
| 切换分屏 | `⌘⇧S` |
| 关闭当前网页或分组 | `⌘W` |
| 恢复上次关闭的网页或分组 | `⌘⇧T` |

## 从源代码构建

先按照 [Ladybird 构建说明](Documentation/BuildInstructionsLadybird.md) 安装依赖，然后在 macOS 上构建并运行 Thoth：

```sh
python3 Meta/thoth.py build -j 6
python3 Meta/thoth.py run --no-build
```

如需生成可独立运行的本地应用：

```sh
python3 Meta/package_thoth.py
```

打包后的应用位于 `Build/0.9.1/Thoth Browser.app`。本地开发包使用临时签名；GitHub 发行版则使用 Developer ID 签名并经过 Apple 公证。

## 项目状态

这是一个早期预览版本。Ladybird 浏览器引擎仍在积极开发中，部分网站和浏览器功能可能无法正常工作。欢迎体验界面并反馈问题。

上游的原生 Windows 构建仍处于实验阶段，目前尚未针对 Thoth 验证。Windows 打包不包含在此 macOS 版本中。

## 致谢与许可证

Thoth 是一个独立项目，基于 Ladybird 开发者的工作构建，并使用 [BSD 2-Clause 许可证](LICENSE) 发布。第三方组件仍遵循各自的许可证。Thoth Browser 并非 Ladybird 官方发行版。
