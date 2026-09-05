[CmdletBinding()]
param(
  [string]$PythonVersion = '3.12.8',
  [switch]$Force
)

# 从 python.org 下载 Windows embeddable Python，并把 edge-tts 及其依赖放进
# resources\python。请在可访问互联网的机器上执行，然后再运行 .\build.ps1。
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$resources = Join-Path $root 'resources'
$pythonRoot = Join-Path $resources 'python'
$zip = Join-Path $env:TEMP "python-$PythonVersion-embed-amd64.zip"
$getPip = Join-Path $env:TEMP 'get-pip.py'

if ((Test-Path $pythonRoot) -and -not $Force) {
  throw "已存在 $pythonRoot。若要重新下载，请增加 -Force。"
}
if (Test-Path $pythonRoot) { Remove-Item -LiteralPath $pythonRoot -Force -Recurse }
New-Item -ItemType Directory -Force $pythonRoot | Out-Null

$embedUrl = "https://www.python.org/ftp/python/$PythonVersion/python-$PythonVersion-embed-amd64.zip"
Invoke-WebRequest -Uri $embedUrl -OutFile $zip
Expand-Archive -LiteralPath $zip -DestinationPath $pythonRoot -Force

$pth = Get-ChildItem -LiteralPath $pythonRoot -Filter 'python*._pth' | Select-Object -First 1
if (-not $pth) { throw 'Python embeddable _pth 文件不存在。' }
$lines = @("python$($PythonVersion.Split('.')[0])$($PythonVersion.Split('.')[1]).zip", '.', 'Lib/site-packages', 'import site')
Set-Content -LiteralPath $pth.FullName -Value $lines -Encoding ascii

Invoke-WebRequest -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile $getPip
& (Join-Path $pythonRoot 'python.exe') $getPip --no-warn-script-location
& (Join-Path $pythonRoot 'python.exe') -m pip install --upgrade pip edge-tts
if ($LASTEXITCODE -ne 0) { throw 'edge-tts 安装失败。' }

# 发布时保留 edge-tts 的许可证文本；其余 Python/PyPI 依赖的许可证应由发行方审查并一并分发。
$licenseUrl = 'https://raw.githubusercontent.com/rany2/edge-tts/master/LICENSE'
Invoke-WebRequest -Uri $licenseUrl -OutFile (Join-Path $root 'LICENSES\edge-tts-LICENSE.txt')
Write-Host "Edge TTS runtime is ready: $pythonRoot"
Write-Host '请重新执行 .\build.ps1，并分发完整 build 文件夹。'
