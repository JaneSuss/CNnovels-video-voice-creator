#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <urlmon.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <random>
#include <regex>
#include <cwctype>
#include <cmath>

#pragma comment(lib, "urlmon.lib")
namespace fs = std::filesystem;

constexpr int ID_TEXT = 101, ID_OPEN_TEXT = 102, ID_VIDEO_FOLDER = 103, ID_PICK_FOLDER = 104;
constexpr int ID_MUSIC = 105, ID_ADD_MUSIC = 106, ID_RATIO = 107, ID_RATE = 108, ID_VOLUME = 109;
constexpr int ID_URL = 110, ID_DOWNLOAD_VIDEO = 111, ID_DOWNLOAD_MUSIC = 112, ID_START = 113;
constexpr int ID_OPEN_OUTPUT = 114, ID_LOG = 115, ID_FILENAME = 117, ID_VOICE = 118;
constexpr int ID_LABEL_TEXT = 130, ID_LABEL_RATIO = 131, ID_LABEL_RATE = 132, ID_LABEL_VOLUME = 133;
constexpr int ID_LABEL_VIDEO = 134, ID_LABEL_MUSIC = 135, ID_LABEL_URL = 136, ID_LABEL_LOG = 137, ID_LABEL_VOICE = 138;
constexpr UINT WM_APP_LOG = WM_APP + 1, WM_APP_DONE = WM_APP + 2, WM_APP_REVEAL_OUTPUT = WM_APP + 3;

struct Material { std::wstring name; fs::path path; };
struct VoicePreset { const wchar_t* label; const wchar_t* id; };
constexpr VoicePreset kVoices[] = {
  {L"Microsoft Xiaoxiao Online (Nature) - chinese mainland", L"zh-CN-XiaoxiaoNeural"},
  {L"Microsoft Yunxi Online - chinese mainland", L"zh-CN-YunxiNeural"},
  {L"Microsoft Yunjian Online - chinese mainland", L"zh-CN-YunjianNeural"},
  {L"Microsoft Xiaoyi Online - chinese mainland", L"zh-CN-XiaoyiNeural"},
};
struct JobOptions {
  std::wstring text, videoFolder, music, voice, rate;
  double musicVolume = 0.30;
  bool vertical = false;
};
struct Ui {
  HWND window{}, text{}, filename{}, videoFolder{}, music{}, ratio{}, rate{}, volume{}, voice{}, url{}, start{}, log{};
  std::vector<Material> musicItems;
} g;

std::wstring ErrorText(DWORD code = GetLastError()) {
  wchar_t* buffer = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  std::wstring message = buffer ? buffer : L"未知 Windows 错误";
  if (buffer) LocalFree(buffer);
  while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) message.pop_back();
  return message;
}
std::wstring ToLower(std::wstring value) { std::transform(value.begin(), value.end(), value.begin(), towlower); return value; }
std::wstring ReadControl(HWND control) { const int n = GetWindowTextLengthW(control); std::wstring value(n + 1, L'\0'); GetWindowTextW(control, value.data(), n + 1); value.resize(n); return value; }
void SetText(HWND control, const std::wstring& value) { SetWindowTextW(control, value.c_str()); }
std::string ToUtf8(const std::wstring& value) { if (value.empty()) return {}; int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr); std::string out(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), n, nullptr, nullptr); return out; }
std::wstring FromUtf8(const std::string& value) { if (value.empty()) return {}; int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0); if (!n) n = MultiByteToWideChar(CP_ACP, 0, value.data(), (int)value.size(), nullptr, 0); std::wstring out(n, L'\0'); MultiByteToWideChar(n && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0) ? CP_UTF8 : CP_ACP, 0, value.data(), (int)value.size(), out.data(), n); return out; }

fs::path AppDataDir() { PWSTR raw = nullptr; fs::path result; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) { result = fs::path(raw) / L"CNnovels-video changer"; CoTaskMemFree(raw); } else result = fs::temp_directory_path() / L"CNnovels-video changer"; return result; }
fs::path MusicDir() { return AppDataDir() / L"materials" / L"music"; }
fs::path JobsDir() { return AppDataDir() / L"jobs"; }
fs::path ExeDir() { wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH); return fs::path(path).parent_path(); }
fs::path FfmpegPath() { const fs::path bundled = ExeDir() / L"resources" / L"ffmpeg.exe"; return fs::exists(bundled) ? bundled : ExeDir() / L"ffmpeg.exe"; }
fs::path PythonPath() { return ExeDir() / L"resources" / L"python" / L"python.exe"; }
fs::path EdgeRunnerPath() { return ExeDir() / L"resources" / L"edge_tts_runner.py"; }
void EnsureDirectories() { fs::create_directories(MusicDir()); fs::create_directories(JobsDir()); }

void PostLog(const std::wstring& message) { PostMessageW(g.window, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(new std::wstring(message + L"\r\n"))); }
void AppendLog(const std::wstring& text) { SendMessageW(g.log, EM_SETSEL, -1, -1); SendMessageW(g.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str())); }
std::wstring Quote(const std::wstring& input) { std::wstring out = L"\""; size_t slashes = 0; for (wchar_t c : input) { if (c == L'\\') { ++slashes; continue; } if (c == L'\"') { out.append(slashes * 2 + 1, L'\\'); out.push_back(c); slashes = 0; continue; } out.append(slashes, L'\\'); slashes = 0; out.push_back(c); } out.append(slashes * 2, L'\\'); out.push_back(L'\"'); return out; }

bool RunProcess(const fs::path& application, const std::vector<std::wstring>& args, const fs::path& cwd, std::wstring& error, std::wstring* capturedOutput = nullptr, bool allowNonZeroExit = false) {
  std::wstring command = Quote(application.wstring()); for (const auto& arg : args) command += L" " + Quote(arg);
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE}; HANDLE readPipe{}, writePipe{};
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) { error = ErrorText(); return false; }
  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; si.wShowWindow = SW_HIDE; si.hStdOutput = writePipe; si.hStdError = writePipe; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{}; std::vector<wchar_t> cmd(command.begin(), command.end()); cmd.push_back(L'\0');
  const BOOL made = CreateProcessW(application.c_str(), cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
  CloseHandle(writePipe);
  if (!made) { CloseHandle(readPipe); error = L"无法启动 " + application.filename().wstring() + L"：" + ErrorText(); return false; }
  std::string bytes; char buffer[4096]; DWORD got{}; while (ReadFile(readPipe, buffer, sizeof(buffer), &got, nullptr) && got) bytes.append(buffer, got);
  CloseHandle(readPipe); WaitForSingleObject(pi.hProcess, INFINITE); DWORD code{}; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  const std::wstring detail = FromUtf8(bytes); if (capturedOutput) *capturedOutput = detail;
  if (code != 0 && !allowNonZeroExit) { std::wstring clipped = detail; if (clipped.size() > 1500) clipped = clipped.substr(clipped.size() - 1500); error = application.filename().wstring() + L" 执行失败（退出码 " + std::to_wstring(code) + L"）：\n" + clipped; return false; }
  return true;
}

bool ReadFileText(const fs::path& path, std::wstring& text, std::wstring& error) { std::ifstream in(path, std::ios::binary); if (!in) { error = L"无法打开 TXT 文件。"; return false; } std::string raw((std::istreambuf_iterator<char>(in)), {}); if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) raw.erase(0, 3); text = FromUtf8(raw); if (text.empty()) { error = L"TXT 中没有可读取的 UTF-8 文本。"; return false; } return true; }
bool WriteUtf8(const fs::path& path, const std::wstring& text, std::wstring& error) { std::ofstream out(path, std::ios::binary); if (!out) { error = L"无法写入：" + path.wstring(); return false; } const auto utf8 = ToUtf8(text); out.write(utf8.data(), (std::streamsize)utf8.size()); return bool(out); }
std::wstring Timestamp(double seconds, wchar_t separator = L',') { long long total = (long long)(seconds * 1000.0 + 0.5); long long ms = total % 1000; total /= 1000; long long sec = total % 60; total /= 60; long long min = total % 60; total /= 60; wchar_t out[32]; swprintf_s(out, L"%02lld:%02lld:%02lld%c%03lld", total, min, sec, separator, ms); return out; }
std::vector<std::wstring> SplitScenes(const std::wstring& text) { std::vector<std::wstring> scenes; std::wstring current; auto flush = [&] { const auto begin = current.find_first_not_of(L" \t\r\n"); if (begin != std::wstring::npos) { std::wstring value = current.substr(begin); while (value.size() > 70) { scenes.push_back(value.substr(0, 70)); value.erase(0, 70); } if (!value.empty()) scenes.push_back(value); } current.clear(); }; for (wchar_t c : text) { current.push_back(c); if (c == L'。' || c == L'！' || c == L'？' || c == L'!' || c == L'?' || (c == L'\n' && current.size() > 40)) flush(); } flush(); return scenes; }
std::wstring BuildSrt(const std::vector<std::wstring>& scenes, double total) { size_t totalWeight{}; for (const auto& scene : scenes) totalWeight += std::max<size_t>(1, scene.size()); std::wstringstream result; double now{}; for (size_t i = 0; i < scenes.size(); ++i) { const double duration = std::max(1.4, total * std::max<size_t>(1, scenes[i].size()) / std::max<size_t>(1, totalWeight)); const double end = i + 1 == scenes.size() ? total : std::min(total, now + duration); result << i + 1 << L"\n" << Timestamp(now) << L" --> " << Timestamp(end) << L"\n" << scenes[i] << L"\n\n"; now = end; } return result.str(); }
bool ParseSrtTime(const std::wstring& line, double& end) { const auto arrow = line.find(L"-->"); if (arrow == std::wstring::npos) return false; int h{}, m{}, s{}, ms{}; if (swscanf_s(line.substr(arrow + 3).c_str(), L" %d:%d:%d%*[, .]%d", &h, &m, &s, &ms) == 4) { end = h * 3600.0 + m * 60.0 + s + ms / 1000.0; return true; } return false; }
bool ParseSrtTimeRange(const std::wstring& line, double& begin, double& end) {
  int bh{}, bm{}, bs{}, bms{}, eh{}, em{}, es{}, ems{};
  if (swscanf_s(line.c_str(), L" %d:%d:%d%*[, .]%d --> %d:%d:%d%*[, .]%d", &bh, &bm, &bs, &bms, &eh, &em, &es, &ems) != 8) return false;
  begin = bh * 3600.0 + bm * 60.0 + bs + bms / 1000.0; end = eh * 3600.0 + em * 60.0 + es + ems / 1000.0;
  return end > begin;
}
std::wstring CompactSubtitleText(const std::wstring& input) {
  std::wstring result; bool pendingSpace = false;
  for (wchar_t ch : input) { if (iswspace(ch)) { pendingSpace = !result.empty(); continue; } if (pendingSpace) result.push_back(L' '); pendingSpace = false; result.push_back(ch); }
  return result;
}
std::wstring SubtitleLines(const std::wstring& text) {
  constexpr size_t kCharsPerLine = 13; std::wstring result;
  for (size_t i = 0; i < text.size(); ++i) { if (i && i % kCharsPerLine == 0) result += L'\n'; result += text[i]; }
  return result;
}
std::vector<std::wstring> SplitSubtitleCue(const std::wstring& text) {
  constexpr size_t kCharsPerCue = 26; std::vector<std::wstring> result; const auto clean = CompactSubtitleText(text);
  for (size_t position = 0; position < clean.size();) { size_t count = std::min(kCharsPerCue, clean.size() - position); if (position + count < clean.size()) { for (size_t candidate = count; candidate > 10; --candidate) { const wchar_t c = clean[position + candidate - 1]; if (c == L'。' || c == L'！' || c == L'？' || c == L'，' || c == L',' || c == L' ' || c == L'；') { count = candidate; break; } } } result.push_back(SubtitleLines(clean.substr(position, count))); position += count; while (position < clean.size() && clean[position] == L' ') ++position; }
  return result;
}
bool NormalizeSrt(const fs::path& srt, std::wstring& error) {
  std::ifstream input(srt, std::ios::binary); if (!input) { error = L"无法读取字幕文件。"; return false; }
  std::vector<std::wstring> lines; std::string raw; while (std::getline(input, raw)) { auto line = FromUtf8(raw); if (!line.empty() && line.back() == L'\r') line.pop_back(); lines.push_back(line); }
  struct Cue { double begin, end; std::wstring text; }; std::vector<Cue> cues;
  for (size_t i = 0; i < lines.size();) { double begin{}, end{}; if (!ParseSrtTimeRange(lines[i], begin, end)) { ++i; continue; } ++i; std::wstring text; while (i < lines.size() && !lines[i].empty()) { if (!text.empty()) text += L' '; text += lines[i++]; } const auto pieces = SplitSubtitleCue(text); if (pieces.empty()) continue; const double totalWeight = [&] { double weight{}; for (const auto& piece : pieces) weight += std::max<size_t>(1, CompactSubtitleText(piece).size()); return weight; }(); double now = begin; for (size_t piece = 0; piece < pieces.size(); ++piece) { const double weight = std::max<size_t>(1, CompactSubtitleText(pieces[piece]).size()); const double finish = piece + 1 == pieces.size() ? end : std::min(end, now + (end - begin) * weight / totalWeight); cues.push_back({now, finish, pieces[piece]}); now = finish; }
  }
  if (cues.empty()) { error = L"字幕文件没有有效时间轴。"; return false; }
  std::wstringstream output; for (size_t i = 0; i < cues.size(); ++i) output << i + 1 << L"\n" << Timestamp(cues[i].begin) << L" --> " << Timestamp(cues[i].end) << L"\n" << cues[i].text << L"\n\n";
  return WriteUtf8(srt, output.str(), error);
}
double SrtDuration(const fs::path& srt) { std::ifstream input(srt, std::ios::binary); std::string raw; double duration{}; while (std::getline(input, raw)) { double end{}; if (ParseSrtTime(FromUtf8(raw), end)) duration = std::max(duration, end); } return duration; }

struct SubtitleCue { double begin{}, end{}; std::wstring text; };

bool ReadSrtCues(const fs::path& srt, std::vector<SubtitleCue>& cues, std::wstring& error) {
  std::ifstream input(srt, std::ios::binary);
  if (!input) { error = L"无法读取字幕文件。"; return false; }
  std::vector<std::wstring> lines;
  std::string raw;
  while (std::getline(input, raw)) {
    auto line = FromUtf8(raw);
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    lines.push_back(line);
  }
  for (size_t i = 0; i < lines.size();) {
    double begin{}, end{};
    if (!ParseSrtTimeRange(lines[i], begin, end)) { ++i; continue; }
    ++i;
    std::wstring text;
    while (i < lines.size() && !lines[i].empty()) {
      if (!text.empty()) text += L'\n';
      text += lines[i++];
    }
    if (!text.empty()) cues.push_back({begin, end, text});
  }
  if (cues.empty()) { error = L"字幕文件没有有效时间轴。"; return false; }
  return true;
}

std::wstring AssTimestamp(double seconds) {
  const long long centiseconds = std::max(0LL, (long long)std::llround(seconds * 100.0));
  long long value = centiseconds;
  const long long cs = value % 100; value /= 100;
  const long long s = value % 60; value /= 60;
  const long long m = value % 60; value /= 60;
  wchar_t result[32];
  swprintf_s(result, L"%lld:%02lld:%02lld.%02lld", value, m, s, cs);
  return result;
}

std::wstring EscapeAssText(const std::wstring& text) {
  std::wstring result;
  for (wchar_t ch : text) {
    if (ch == L'\r') continue;
    if (ch == L'\n') { result += L"\\N"; continue; }
    // Keep novel text from being interpreted as ASS override tags or escapes.
    if (ch == L'{' ) { result += L'（'; continue; }
    if (ch == L'}' ) { result += L'）'; continue; }
    if (ch == L'\\') { result += L'＼'; continue; }
    result += ch;
  }
  return result;
}

bool WriteAssSubtitles(const fs::path& srt, const fs::path& ass, bool vertical, std::wstring& error) {
  std::vector<SubtitleCue> cues;
  if (!ReadSrtCues(srt, cues, error)) return false;

  const int width = vertical ? 1080 : 1920;
  const int height = vertical ? 1920 : 1080;
  // Alignment 8 anchors the top of the caption block. Thus the first line is
  // always at this Y coordinate and a second line only extends downward.
  const int firstLineY = vertical ? 1520 : 850;
  const int fontSize = 70; // 35 与 105 的折中值，可按需微调
  std::wstringstream output;
  output << L"[Script Info]\n"
         << L"; Generated by CNnovels-video changer\n"
         << L"ScriptType: v4.00+\n"
         << L"PlayResX: " << width << L"\n"
         << L"PlayResY: " << height << L"\n"
         << L"ScaledBorderAndShadow: yes\n\n"
         << L"[V4+ Styles]\n"
         << L"Format: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding\n"
         // BorderStyle=3 draws an opaque box whose fill color is OutlineColour
         // (semi-transparent black here); Outline is the box padding.
         << L"Style: Caption,PingFang SC," << fontSize
         << L",&H00FFFFFF,&H00FFFFFF,&H40000000,&H40000000,0,0,0,0,100,100,0,0,3,8,0,8,0,0,"
         << firstLineY << L",1\n\n"
         << L"[Events]\n"
         << L"Format: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text\n";
  for (const auto& cue : cues) {
    output << L"Dialogue: 0," << AssTimestamp(cue.begin) << L"," << AssTimestamp(cue.end)
           << L",Caption,,0,0,0,," << EscapeAssText(cue.text) << L"\n";
  }
  return WriteUtf8(ass, output.str(), error);
}

std::wstring FfmpegSubtitlePath(const fs::path& path) { std::wstring p = path.wstring(); std::replace(p.begin(), p.end(), L'\\', L'/'); std::wstring out; for (wchar_t c : p) { if (c == L':' || c == L'\'' || c == L'\\') out.push_back(L'\\'); out.push_back(c); } return out; }
std::wstring FormatVolume(double value) { std::wostringstream output; output << std::fixed << std::setprecision(2) << std::clamp(value, 0.0, 1.0); return output.str(); }
double ParseVolume(std::wstring value) { std::replace(value.begin(), value.end(), L',', L'.'); wchar_t* end{}; const double result = wcstod(value.c_str(), &end); return end == value.c_str() || !std::isfinite(result) ? 0.30 : std::clamp(result, 0.0, 1.0); }
fs::path NewJobDir() { SYSTEMTIME st{}; GetLocalTime(&st); std::wstringstream name; name << std::setfill(L'0') << st.wYear << st.wMonth << st.wDay << L"-" << std::setw(2) << st.wHour << std::setw(2) << st.wMinute << std::setw(2) << st.wSecond << L"-" << GetTickCount64(); fs::path result = JobsDir() / name.str(); fs::create_directories(result); return result; }

std::vector<fs::path> CollectVideos(const fs::path& folder) { std::vector<fs::path> result; std::error_code ec; if (!fs::is_directory(folder, ec)) return result; for (const auto& entry : fs::recursive_directory_iterator(folder, fs::directory_options::skip_permission_denied, ec)) if (!ec && entry.is_regular_file(ec)) { const auto ext = ToLower(entry.path().extension().wstring()); if (ext == L".mp4" || ext == L".mov" || ext == L".mkv" || ext == L".webm" || ext == L".avi" || ext == L".m4v") result.push_back(entry.path()); } return result; }
std::wstring VideoFitFilter(int width, int height) { const std::wstring size = std::to_wstring(width) + L":" + std::to_wstring(height); return L"[0:v]split=2[bgsrc][fgsrc];[bgsrc]scale=" + size + L":force_original_aspect_ratio=increase,crop=" + size + L",boxblur=24:12,eq=brightness=-0.04:saturation=0.88[bg];[fgsrc]scale=" + size + L":force_original_aspect_ratio=decrease,format=yuva420p[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2:format=auto,format=yuv420p[v]"; }

bool GenerateNarration(const JobOptions& options, const fs::path& job, fs::path& audio, fs::path& srt, double& seconds, std::wstring& error) {
  const fs::path python = PythonPath(), runner = EdgeRunnerPath();
  if (!fs::exists(python) || !fs::exists(runner)) { error = L"缺少内置 Edge TTS 运行环境。发布目录必须包含 resources\\python\\python.exe 与 resources\\edge_tts_runner.py。请先运行 tools\\prepare_edge_tts_runtime.ps1 后重新构建。"; return false; }
  const fs::path input = job / L"novel.txt"; audio = job / L"narration.mp3"; srt = job / L"captions.srt";
  PostLog(L"正在通过 Edge TTS 在线生成配音：" + options.voice + L"，语速 " + options.rate + L"…");
  if (!RunProcess(python, {runner.wstring(), L"--input", input.wstring(), L"--output", audio.wstring(), L"--srt", srt.wstring(), L"--voice", options.voice, L"--rate", options.rate}, job, error)) return false;
  if (!fs::exists(audio) || fs::file_size(audio) == 0) { error = L"Edge TTS 没有生成有效的 MP3 配音。请检查网络连接、音色和文本。"; return false; }
  seconds = SrtDuration(srt); if (seconds <= 0.01) { const auto scenes = SplitScenes(options.text); seconds = std::max(1.0, options.text.size() / 5.5); if (!WriteUtf8(srt, BuildSrt(scenes, seconds), error)) return false; PostLog(L"Edge TTS 未返回有效时间轴字幕，已使用文本切句字幕作为后备。" ); }
  std::wstring subtitleError; if (!NormalizeSrt(srt, subtitleError)) PostLog(L"字幕分行优化未生效，保留原始 Edge 时间轴：" + subtitleError);
  return true;
}
bool ReadMediaDuration(const fs::path& ffmpeg, const fs::path& source, double& seconds) {
  std::wstring report, ignored;
  if (!RunProcess(ffmpeg, {L"-hide_banner", L"-i", source.wstring()}, source.parent_path(), ignored, &report, true)) return false;
  static const std::wregex durationPattern(LR"(Duration:\s*(\d{2}):(\d{2}):(\d{2})[.,](\d{2,3}))");
  std::wsmatch match;
  if (!std::regex_search(report, match, durationPattern)) return false;
  seconds = std::stod(match[1]) * 3600.0 + std::stod(match[2]) * 60.0 + std::stod(match[3]) + std::stod(match[4]) / (match[4].str().size() == 2 ? 100.0 : 1000.0);
  return std::isfinite(seconds) && seconds > 0.05;
}
bool RenderSegment(const fs::path& ffmpeg, const fs::path& source, const fs::path& target, double duration, int width, int height, std::wstring& error) {
  return RunProcess(ffmpeg, {L"-y", L"-i", source.wstring(), L"-t", std::to_wstring(duration), L"-an", L"-filter_complex", VideoFitFilter(width, height), L"-map", L"[v]", L"-r", L"30", L"-c:v", L"libx264", L"-preset", L"veryfast", L"-crf", L"22", L"-pix_fmt", L"yuv420p", target.wstring()}, target.parent_path(), error);
}
bool CreateBaseVideo(const JobOptions& options, const fs::path& job, double seconds, const fs::path& ffmpeg, fs::path& base, std::wstring& error) {
  const int width = options.vertical ? 1080 : 1920, height = options.vertical ? 1920 : 1080;
  base = job / L"base.mp4";
  const auto videos = options.videoFolder.empty() ? std::vector<fs::path>{} : CollectVideos(options.videoFolder);
  if (videos.empty()) { PostLog(L"未选择或未发现视频素材，使用默认纯色背景。" ); const std::wstring size = std::to_wstring(width) + L"x" + std::to_wstring(height); return RunProcess(ffmpeg, {L"-y", L"-f", L"lavfi", L"-i", L"color=c=#111827:s=" + size + L":r=30", L"-t", std::to_wstring(seconds + 0.5), L"-an", L"-c:v", L"libx264", L"-pix_fmt", L"yuv420p", base.wstring()}, job, error); }
  const fs::path segments = job / L"segments"; fs::create_directories(segments);
  std::vector<fs::path> pool = videos; std::mt19937 engine((unsigned)(GetTickCount64() ^ std::random_device{}())); fs::path previous; double covered = 0.0; const double targetDuration = seconds + 0.5; size_t rendered = 0; std::wstring listText;
  PostLog(L"将按完整视频顺序随机拼接素材；仅最后一个素材会按朗读结尾裁剪。" );
  while (covered < targetDuration) {
    std::shuffle(pool.begin(), pool.end(), engine); if (pool.size() > 1 && !previous.empty() && pool.front() == previous) std::swap(pool.front(), pool[1]);
    bool usedInRound = false;
    for (const auto& source : pool) {
      if (covered >= targetDuration) break;
      double sourceDuration{};
      if (!ReadMediaDuration(ffmpeg, source, sourceDuration)) { PostLog(L"跳过无法读取时长的视频素材：" + source.filename().wstring()); continue; }
      const double take = std::min(sourceDuration, targetDuration - covered);
      if (take <= 0.05) continue;
      const fs::path part = segments / (L"segment-" + std::to_wstring(rendered) + L".mp4");
      PostLog(L"处理完整视频素材 " + std::to_wstring(rendered + 1) + L"：" + source.filename().wstring() + (take + 0.08 < sourceDuration ? L"（按朗读结尾裁剪）" : L""));
      if (!RenderSegment(ffmpeg, source, part, take, width, height, error)) return false;
      std::wstring p = part.wstring(); std::replace(p.begin(), p.end(), L'\\', L'/'); listText += L"file '" + p + L"'\n";
      covered += take; previous = source; ++rendered; usedInRound = true;
    }
    if (!usedInRound) { PostLog(L"视频素材均无法使用，改用默认纯色背景。" ); const std::wstring size = std::to_wstring(width) + L"x" + std::to_wstring(height); return RunProcess(ffmpeg, {L"-y", L"-f", L"lavfi", L"-i", L"color=c=#111827:s=" + size + L":r=30", L"-t", std::to_wstring(seconds + 0.5), L"-an", L"-c:v", L"libx264", L"-pix_fmt", L"yuv420p", base.wstring()}, job, error); }
  }
  const fs::path listPath = job / L"segments.txt"; if (!WriteUtf8(listPath, listText, error)) return false;
  return RunProcess(ffmpeg, {L"-y", L"-f", L"concat", L"-safe", L"0", L"-i", listPath.wstring(), L"-c", L"copy", base.wstring()}, job, error);
}

bool RenderJob(const JobOptions& options, fs::path& output, std::wstring& error) {
  EnsureDirectories(); const fs::path ffmpeg = FfmpegPath();
  if (!fs::exists(ffmpeg)) { error = L"未找到内置 FFmpeg：" + ffmpeg.wstring() + L"。请把 ffmpeg.exe 放进 resources 文件夹。"; return false; }
  const fs::path job = NewJobDir(); if (!WriteUtf8(job / L"novel.txt", options.text, error)) return false;
  fs::path audio, srt; double seconds{}; if (!GenerateNarration(options, job, audio, srt, seconds, error)) return false;
  PostLog(L"配音完成，时长约 " + std::to_wstring((int)std::ceil(seconds)) + L" 秒；字幕优先使用 Edge TTS 返回的句子/词语时间轴。" );
  const fs::path ass = job / L"captions.ass";
  if (!WriteAssSubtitles(srt, ass, options.vertical, error)) return false;
  PostLog(L"字幕样式已应用：固定首行、纯白字体与半透明黑色背景框。" );
  fs::path base; if (!CreateBaseVideo(options, job, seconds, ffmpeg, base, error)) return false;
  output = job / L"output.mp4"; std::vector<std::wstring> args{L"-y", L"-i", base.wstring(), L"-i", audio.wstring()}; std::wstring mix;
  if (!options.music.empty() && fs::exists(options.music)) { PostLog(L"混入音乐：" + fs::path(options.music).filename().wstring() + L"。已启用循环，若音乐短于朗读会重复播放至朗读结束。" ); args.insert(args.end(), {L"-stream_loop", L"-1", L"-i", options.music}); mix = L"[1:a]volume=1[narration];[2:a]volume=" + FormatVolume(options.musicVolume) + L"[music];[narration][music]amix=inputs=2:duration=first:dropout_transition=2[a]"; }
  else { PostLog(L"未选择音乐素材，导出纯配音版本。" ); mix = L"[1:a]anull[a]"; }
  // The ASS script uses the real output resolution as PlayRes. This avoids
  // SRT's 384x288 fallback coordinate system, which distorted vertical text.
  const std::wstring subtitles = L"subtitles='" + FfmpegSubtitlePath(ass) + L"'";
  args.insert(args.end(), {L"-filter_complex", mix, L"-vf", subtitles, L"-map", L"0:v:0", L"-map", L"[a]", L"-c:v", L"libx264", L"-preset", L"veryfast", L"-crf", L"23", L"-c:a", L"aac", L"-b:a", L"160k", L"-shortest", output.wstring()});
  PostLog(L"正在合成视频、音乐与字幕…"); if (!RunProcess(ffmpeg, args, job, error)) return false; PostLog(L"合成完成。" ); return true;
}

void RemoveJobCacheExceptOutput(const fs::path& output) { std::error_code ec; for (const auto& entry : fs::directory_iterator(output.parent_path(), ec)) { if (entry.path().lexically_normal() != output.lexically_normal()) { fs::remove_all(entry.path(), ec); ec.clear(); } } }
void RevealOutputInExplorer(const fs::path& output) { const std::wstring parameters = L"/select," + Quote(output.wstring()); ShellExecuteW(g.window, L"open", L"explorer.exe", parameters.c_str(), output.parent_path().c_str(), SW_SHOWNORMAL); }
bool IsMusic(const fs::path& path) { const auto e = ToLower(path.extension().wstring()); return e == L".mp3" || e == L".wav" || e == L".m4a" || e == L".aac" || e == L".ogg" || e == L".flac"; }
void PopulateMusic() { g.musicItems.clear(); SendMessageW(g.music, CB_RESETCONTENT, 0, 0); SendMessageW(g.music, CB_ADDSTRING, 0, (LPARAM)L"不添加背景音乐"); std::error_code ec; for (const auto& entry : fs::directory_iterator(MusicDir(), ec)) if (entry.is_regular_file(ec) && IsMusic(entry.path())) g.musicItems.push_back({entry.path().filename().wstring(), entry.path()}); std::sort(g.musicItems.begin(), g.musicItems.end(), [](const Material& a, const Material& b) { return a.name < b.name; }); for (const auto& item : g.musicItems) SendMessageW(g.music, CB_ADDSTRING, 0, (LPARAM)item.name.c_str()); SendMessageW(g.music, CB_SETCURSEL, 0, 0); }
std::wstring SelectedMusic() { const int index = (int)SendMessageW(g.music, CB_GETCURSEL, 0, 0); return index > 0 && index <= (int)g.musicItems.size() ? g.musicItems[index - 1].path.wstring() : L""; }
bool ChooseFile(const wchar_t* filter, fs::path& path) { wchar_t name[MAX_PATH]{}; OPENFILENAMEW dialog{sizeof(dialog)}; dialog.hwndOwner = g.window; dialog.lpstrFile = name; dialog.nMaxFile = MAX_PATH; dialog.lpstrFilter = filter; dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; if (!GetOpenFileNameW(&dialog)) return false; path = name; return true; }
bool PickFolder(fs::path& folder) { BROWSEINFOW browse{}; browse.hwndOwner = g.window; browse.lpszTitle = L"选择包含视频素材的文件夹"; browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE; PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse); if (!item) return false; wchar_t path[MAX_PATH]{}; const bool ok = SHGetPathFromIDListW(item, path); CoTaskMemFree(item); if (ok) folder = path; return ok; }
std::wstring ExtensionFromUrl(const std::wstring& url, bool video) { const auto slash = url.find_last_of(L'/'); const auto dot = url.find_last_of(L'.'); if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) { std::wstring ext = url.substr(dot); const auto end = ext.find_first_of(L"?#"); ext.resize(end); if (ext.size() <= 8) return ext; } return video ? L".mp4" : L".mp3"; }
void DownloadMusic() { const std::wstring url = ReadControl(g.url); if (url.empty()) { MessageBoxW(g.window, L"请先输入拥有授权的音乐文件 HTTP/HTTPS 直链。", L"缺少直链", MB_ICONWARNING); return; } EnableWindow(g.start, FALSE); AppendLog(L"正在下载音乐，请稍候…\r\n"); std::thread([url] { EnsureDirectories(); const fs::path target = MusicDir() / (L"download-" + std::to_wstring(GetTickCount64()) + ExtensionFromUrl(url, false)); if (SUCCEEDED(URLDownloadToFileW(nullptr, url.c_str(), target.c_str(), 0, nullptr))) PostLog(L"音乐下载完成：" + target.filename().wstring()); else PostLog(L"音乐下载失败。请确认链接可访问并且拥有使用授权。" ); PostMessageW(g.window, WM_APP_DONE, 1, 0); }).detach(); }
void ImportMusic() { fs::path source; if (!ChooseFile(L"音乐文件\0*.mp3;*.wav;*.m4a;*.aac;*.ogg;*.flac\0所有文件\0*.*\0\0", source)) return; EnsureDirectories(); std::error_code ec; fs::copy_file(source, MusicDir() / source.filename(), fs::copy_options::overwrite_existing, ec); if (ec) MessageBoxW(g.window, L"无法导入音乐。请确认文件未被占用且目标目录可写入。", L"导入失败", MB_ICONERROR); else { PopulateMusic(); AppendLog(L"已导入音乐：" + source.filename().wstring() + L"\r\n"); } }

void StartRender() {
  JobOptions options; options.text = ReadControl(g.text); options.videoFolder = ReadControl(g.videoFolder); options.music = SelectedMusic(); options.vertical = SendMessageW(g.ratio, CB_GETCURSEL, 0, 0) == 1; options.musicVolume = ParseVolume(ReadControl(g.volume));
  const int voiceIndex = (int)SendMessageW(g.voice, CB_GETCURSEL, 0, 0); options.voice = kVoices[std::clamp(voiceIndex, 0, (int)std::size(kVoices) - 1)].id;
  const int rateIndex = (int)SendMessageW(g.rate, CB_GETCURSEL, 0, 0); constexpr const wchar_t* rates[] = {L"-20%", L"+0%", L"+20%", L"+40%", L"+60%"}; options.rate = rates[std::clamp(rateIndex, 0, 4)];
  if (options.text.size() < 2) { MessageBoxW(g.window, L"请导入 TXT 或粘贴至少两个字的正文。", L"缺少正文", MB_ICONWARNING); return; }
  if (!options.videoFolder.empty() && !fs::is_directory(options.videoFolder)) { options.videoFolder.clear(); }
  if (!fs::exists(FfmpegPath())) { MessageBoxW(g.window, (L"未找到内置 FFmpeg：\n" + FfmpegPath().wstring()).c_str(), L"环境未就绪", MB_ICONERROR); return; }
  if (!fs::exists(PythonPath()) || !fs::exists(EdgeRunnerPath())) { MessageBoxW(g.window, L"缺少内置 Edge TTS 运行环境。请先运行 tools\\prepare_edge_tts_runtime.ps1 后重新构建发布包。", L"环境未就绪", MB_ICONERROR); return; }
  SetText(g.volume, FormatVolume(options.musicVolume)); SetText(g.log, L""); EnableWindow(g.start, FALSE); AppendLog(L"========== 开始新任务 ==========\r\n");
  std::thread([options] { fs::path output; std::wstring error; if (RenderJob(options, output, error)) { RemoveJobCacheExceptOutput(output); PostLog(L"制作完成：" + output.wstring()); PostLog(L"已删除 output.mp4 以外的任务缓存，并自动打开输出位置。" ); PostMessageW(g.window, WM_APP_REVEAL_OUTPUT, 0, (LPARAM)new fs::path(output)); } else PostLog(L"任务失败：" + error); PostMessageW(g.window, WM_APP_DONE, 0, 0); }).detach();
}
HWND Control(DWORD style, const wchar_t* type, const wchar_t* text, int id) { return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, g.window, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr); }
void Layout(HWND hwnd) { RECT rect{}; GetClientRect(hwnd, &rect); const int left = 20, gap = 12, right = std::max(left + 400, (int)rect.right - 20); const int column = std::max(390, (right - left - gap) / 2), x2 = left + column + gap; auto move = [](HWND h, int x, int y, int w, int hgt) { SetWindowPos(h, nullptr, x, y, std::max(20, w), std::max(20, hgt), SWP_NOZORDER); };
  move(GetDlgItem(hwnd, ID_LABEL_TEXT), left, 18, 90, 20); move(g.filename, left + 92, 14, column - 194, 26); move(GetDlgItem(hwnd, ID_OPEN_TEXT), left + column - 92, 14, 92, 26); move(g.text, left, 46, column, 354);
  move(GetDlgItem(hwnd, ID_LABEL_RATIO), left, 414, 130, 20); move(GetDlgItem(hwnd, ID_LABEL_RATE), left + 143, 414, 112, 20); move(GetDlgItem(hwnd, ID_LABEL_VOLUME), left + 270, 414, 130, 20); move(g.ratio, left, 438, 130, 150); move(g.rate, left + 143, 438, 112, 150); move(g.volume, left + 270, 438, 115, 150); move(g.start, left, 485, 180, 34);
  move(GetDlgItem(hwnd, ID_LABEL_VIDEO), x2, 18, 150, 20); move(g.videoFolder, x2, 42, column - 125, 26); move(GetDlgItem(hwnd, ID_PICK_FOLDER), right - 115, 42, 115, 26); move(GetDlgItem(hwnd, ID_LABEL_MUSIC), x2, 94, 100, 20); move(g.music, x2, 118, column - 125, 180); move(GetDlgItem(hwnd, ID_ADD_MUSIC), right - 115, 118, 115, 26); move(GetDlgItem(hwnd, ID_LABEL_VOICE), x2, 170, 100, 20); move(g.voice, x2, 194, column, 220);
  move(GetDlgItem(hwnd, ID_LABEL_URL), x2, 246, 100, 20); move(g.url, x2, 270, column, 26); move(GetDlgItem(hwnd, ID_DOWNLOAD_MUSIC), x2, 306, 145, 28); move(GetDlgItem(hwnd, ID_OPEN_OUTPUT), x2 + 155, 306, 145, 28);
  move(GetDlgItem(hwnd, ID_LABEL_LOG), left, 534, 100, 20); move(g.log, left, 556, right - left, std::max(120, (int)rect.bottom - 576));
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      g.window = hwnd; const DWORD listStyle = CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP; const DWORD editStyle = CBS_DROPDOWN | CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP;
      Control(SS_LEFT, L"STATIC", L"小说正文", ID_LABEL_TEXT); Control(SS_LEFT, L"STATIC", L"输出比例", ID_LABEL_RATIO); Control(SS_LEFT, L"STATIC", L"Edge 语速", ID_LABEL_RATE); Control(SS_LEFT, L"STATIC", L"配乐音量（0 ~ 1）", ID_LABEL_VOLUME); Control(SS_LEFT, L"STATIC", L"视频素材文件夹", ID_LABEL_VIDEO); Control(SS_LEFT, L"STATIC", L"音乐素材", ID_LABEL_MUSIC); Control(SS_LEFT, L"STATIC", L"Edge TTS 音色", ID_LABEL_VOICE); Control(SS_LEFT, L"STATIC", L"音乐直链", ID_LABEL_URL); Control(SS_LEFT, L"STATIC", L"任务日志", ID_LABEL_LOG);
      g.filename = Control(WS_BORDER | ES_AUTOHSCROLL, L"EDIT", L"我的小说.txt", ID_FILENAME); Control(BS_PUSHBUTTON, L"BUTTON", L"导入 TXT", ID_OPEN_TEXT); g.text = Control(WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, L"EDIT", L"", ID_TEXT);
      g.videoFolder = Control(WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, L"EDIT", L"", ID_VIDEO_FOLDER); Control(BS_PUSHBUTTON, L"BUTTON", L"选择文件夹", ID_PICK_FOLDER); g.music = Control(listStyle, L"COMBOBOX", L"", ID_MUSIC); Control(BS_PUSHBUTTON, L"BUTTON", L"导入音乐", ID_ADD_MUSIC);
      g.ratio = Control(listStyle, L"COMBOBOX", L"", ID_RATIO); SendMessageW(g.ratio, CB_ADDSTRING, 0, (LPARAM)L"横屏 16:9"); SendMessageW(g.ratio, CB_ADDSTRING, 0, (LPARAM)L"竖屏 9:16"); SendMessageW(g.ratio, CB_SETCURSEL, 1, 0);
      g.rate = Control(listStyle, L"COMBOBOX", L"", ID_RATE); for (const auto* label : {L"较慢 (-20%)", L"正常 (0%)", L"较快 (+20%)", L"很快 (+40%)", L"最快 (+60%)"}) SendMessageW(g.rate, CB_ADDSTRING, 0, (LPARAM)label); SendMessageW(g.rate, CB_SETCURSEL, 2, 0);
      g.volume = Control(editStyle, L"COMBOBOX", L"", ID_VOLUME); for (int i = 0; i <= 20; ++i) { const auto value = FormatVolume(i * .05); SendMessageW(g.volume, CB_ADDSTRING, 0, (LPARAM)value.c_str()); } SetText(g.volume, L"0.30");
      g.voice = Control(listStyle, L"COMBOBOX", L"", ID_VOICE); for (const auto& voice : kVoices) SendMessageW(g.voice, CB_ADDSTRING, 0, (LPARAM)voice.label); SendMessageW(g.voice, CB_SETCURSEL, 0, 0);
      g.url = Control(WS_BORDER | ES_AUTOHSCROLL, L"EDIT", L"粘贴拥有授权的音乐 HTTP/HTTPS 直链", ID_URL); Control(BS_PUSHBUTTON, L"BUTTON", L"下载到音乐库", ID_DOWNLOAD_MUSIC); g.start = Control(BS_DEFPUSHBUTTON, L"BUTTON", L"开始生成", ID_START); Control(BS_PUSHBUTTON, L"BUTTON", L"打开输出文件夹", ID_OPEN_OUTPUT); g.log = Control(WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, L"EDIT", L"", ID_LOG);
      const HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT); EnumChildWindows(hwnd, [](HWND child, LPARAM p) -> BOOL { SendMessageW(child, WM_SETFONT, p, TRUE); return TRUE; }, (LPARAM)font);
      EnsureDirectories(); PopulateMusic(); AppendLog(fs::exists(FfmpegPath()) ? L"环境就绪：已找到内置 FFmpeg。\r\n" : L"缺少 resources\\ffmpeg.exe。\r\n"); AppendLog(fs::exists(PythonPath()) && fs::exists(EdgeRunnerPath()) ? L"环境就绪：已找到内置 Edge TTS 运行环境。\r\n" : L"提示：尚未准备 Edge TTS 运行环境，请运行 tools\\prepare_edge_tts_runtime.ps1。\r\n"); return 0;
    }
    case WM_SIZE: Layout(hwnd); return 0;
    case WM_COMMAND: { const int id = LOWORD(wParam); if (id == ID_OPEN_TEXT) { fs::path file; if (ChooseFile(L"文本文件\0*.txt\0所有文件\0*.*\0\0", file)) { std::wstring text, error; if (ReadFileText(file, text, error)) { SetText(g.text, text); SetText(g.filename, file.filename().wstring()); } else MessageBoxW(hwnd, error.c_str(), L"读取失败", MB_ICONERROR); } } else if (id == ID_PICK_FOLDER) { fs::path folder; if (PickFolder(folder)) { SetText(g.videoFolder, folder.wstring()); AppendLog(L"已选择视频文件夹：" + folder.wstring() + L"\r\n"); } } else if (id == ID_ADD_MUSIC) ImportMusic(); else if (id == ID_DOWNLOAD_MUSIC) DownloadMusic(); else if (id == ID_START) StartRender(); else if (id == ID_OPEN_OUTPUT) { EnsureDirectories(); ShellExecuteW(hwnd, L"open", JobsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); } return 0; }
    case WM_APP_LOG: { auto* text = (std::wstring*)lParam; AppendLog(*text); delete text; return 0; }
    case WM_APP_REVEAL_OUTPUT: { auto* output = (fs::path*)lParam; RevealOutputInExplorer(*output); delete output; return 0; }
    case WM_APP_DONE: EnableWindow(g.start, TRUE); if (wParam) PopulateMusic(); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  } return DefWindowProcW(hwnd, message, wParam, lParam);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) { SetProcessDPIAware(); WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.hInstance = instance; wc.lpszClassName = L"CNnovelsVideoChangerNative"; wc.lpfnWndProc = WindowProc; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); RegisterClassExW(&wc); HWND window = CreateWindowExW(0, wc.lpszClassName, L"CNnovels-video changer", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 80, 60, 1160, 780, nullptr, nullptr, instance, nullptr); ShowWindow(window, show); UpdateWindow(window); MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); } return 0; }




