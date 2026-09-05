# Third-party notices

本程序随包分发或引用以下第三方组件，各组件受其自身许可证约束。商业分发前请完成相应许可证与服务条款的合规审查。

## FFmpeg

发布包包含 `resources/ffmpeg.exe`（GPL 构建，启用了 libx264），因此该二进制按 **GNU General Public License v3.0** 分发。

- 许可证全文：[LICENSES/FFmpeg-GPL-3.0.txt](LICENSES/FFmpeg-GPL-3.0.txt)
- 源码与对应源码获取：https://ffmpeg.org/download.html
- Windows 构建来源：https://www.gyan.dev/ffmpeg/builds/

本程序以独立进程调用 `ffmpeg.exe`，不链接其库文件。分发该二进制时须遵守 GPL 及所启用编解码器的许可要求。

## edge-tts

发布包包含 `resources/edge_tts_runner.py` 与 `resources/python/`，运行时以独立 Python 进程调用 [edge-tts](https://github.com/rany2/edge-tts) 生成配音与字幕。

- edge-tts 许可证：**LGPL-3.0**（其中 `src/edge_tts/srt_composer.py` 为 **MIT**）
- 许可证全文：[LICENSES/edge-tts-LICENSE.txt](LICENSES/edge-tts-LICENSE.txt)

## Python 嵌入式运行时

`resources/python/` 为 Windows embeddable Python，按 **Python Software Foundation License (PSF)** 分发。

## 其他 Python 依赖

`resources/python/Lib/site-packages/` 内各包分别受其自身许可证约束：

| 包 | 许可证 |
|---|---|
| aiohttp | Apache-2.0 AND MIT |
| aiosignal、frozenlist、multidict、propcache、yarl | Apache-2.0 |
| aiohappyeyeballs、typing_extensions | PSF-2.0 |
| certifi | MPL-2.0 |
| idna | BSD-3-Clause |
| attrs、tabulate、pip | MIT |

各包完整许可证见其安装目录下的 `LICENSE*` 文件。发布方应保留实际打包版本的许可证与声明。

## 字体

字幕样式引用字体名「PingFang SC」。该字体为 Apple 专有字体，**未随包分发**；Windows 上通常不可用，渲染时会回退到系统中文字体。

## 内容与服务的授权

- Edge TTS 使用微软在线语音服务，受微软服务条款与区域可用性约束
- 请只使用拥有合法授权的文本、视频、音乐与字体内容，并按其许可证要求使用
