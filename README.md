# CNnovels-video changer

把中文小说 TXT 一键制作成「配音 + 字幕 + 视频」的 Windows 桌面工具。纯原生 Win32/C++ 编写，无需安装、不依赖浏览器或 Node.js，解压即用。


## 功能

- 导入或粘贴 UTF-8 TXT 小说正文，一键生成完整视频
- 内置 Edge TTS 在线配音：默认音色「晓晓（自然）」，可切换其他中文音色与语速
- 自动生成字幕并排好版：每行最多 13 字、每条最多两行，白字 + 半透明黑底框
- 选择视频素材文件夹后自动拼接素材，支持横屏 16:9 与竖屏 9:16（默认竖屏）
- 背景音乐可本地导入，或用 HTTP/HTTPS 直链下载到音乐库，自动循环、音量可调
- 一键合成并导出 MP4，完成后自动清理临时文件并选中输出文件

## 系统要求

- Windows 10 / 11（64 位）
- 配音时需联网（访问微软语音服务）

## 快速开始

1. 从 [Releases](../../releases) 下载最新发布包并解压（须解压整个文件夹，不能只复制 EXE）
2. 双击 `CNnovels-video changer.exe`
3. 导入 TXT 正文 → 按需选择视频素材文件夹和背景音乐 → 点击「开始生成」
4. 完成后程序会自动打开并选中输出的 `output.mp4`

## 使用说明

- **正文**：支持 UTF-8 编码的 TXT，可导入文件或直接粘贴
- **音色 / 语速**：默认晓晓（自然）+20%，可在界面下拉框切换
- **视频素材**：可选。不选时使用纯色背景；选了会随机拼接 MP4/MOV/MKV/WebM/AVI/M4V
- **背景音乐**：可选。支持本地导入，或粘贴拥有授权的 HTTP/HTTPS 音乐直链下载到音乐库
- **输出比例**：竖屏 9:16（默认）或横屏 16:9
- 生成结果与音乐库保存在 `%LOCALAPPDATA%\CNnovels-video changer\`

## 从源码构建（可选）

依赖 MinGW-w64 `g++`（C++20）。首次需联网准备内置 Edge TTS 运行环境：

```powershell
.\tools\prepare_edge_tts_runtime.ps1
.\build.ps1
```

## 许可证与版权

- 本项目代码采用 **GPL-3.0**，见 [LICENSE](LICENSE)
- 随包第三方组件（FFmpeg、edge-tts、Python 运行时等）的许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

> 请只使用拥有合法授权的文本、视频、音乐与字体内容。
> 本项目在开发过程中使用了 AI 辅助工具（OpenAI Codex、DeepSeek Harness）。
