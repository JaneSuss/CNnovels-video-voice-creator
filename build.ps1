[CmdletBinding()]
param(
  [string]$Output = "build\\CNnovels-video changer.exe"
)

$ErrorActionPreference = 'Stop'
$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) { throw "未找到 MinGW-w64 g++。请先安装并加入 PATH。" }

$outputDir = Split-Path -Parent $Output
New-Item -ItemType Directory -Force $outputDir | Out-Null
& $compiler.Source -std=c++20 -O2 -mwindows -municode -static -static-libgcc -static-libstdc++ `
  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX `
  src\main.cpp -o $Output `
  -lole32 -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lurlmon -luuid
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$resourcesOut = Join-Path $outputDir 'resources'
New-Item -ItemType Directory -Force $resourcesOut | Out-Null
Copy-Item -LiteralPath resources\ffmpeg.exe -Destination (Join-Path $resourcesOut 'ffmpeg.exe') -Force
Copy-Item -LiteralPath resources\edge_tts_runner.py -Destination (Join-Path $resourcesOut 'edge_tts_runner.py') -Force
if (Test-Path -LiteralPath resources\python) {
  Copy-Item -LiteralPath resources\python -Destination (Join-Path $resourcesOut 'python') -Recurse -Force
} else {
  Write-Warning "尚未找到 resources\python；EXE 已构建，但运行 Edge TTS 前需执行 tools\prepare_edge_tts_runtime.ps1 并重新构建。"
}
New-Item -ItemType Directory -Force (Join-Path $outputDir 'LICENSES') | Out-Null
Copy-Item -LiteralPath LICENSES\FFmpeg-GPL-3.0.txt -Destination (Join-Path $outputDir 'LICENSES\FFmpeg-GPL-3.0.txt') -Force
if (Test-Path -LiteralPath LICENSES\edge-tts-LICENSE.txt) { Copy-Item -LiteralPath LICENSES\edge-tts-LICENSE.txt -Destination (Join-Path $outputDir 'LICENSES\edge-tts-LICENSE.txt') -Force }
Copy-Item -LiteralPath THIRD_PARTY_NOTICES.md -Destination (Join-Path $outputDir 'THIRD_PARTY_NOTICES.md') -Force
Write-Host "Build complete: $Output"

