#include "recorder.h"

#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
  #define POPEN  _popen
  #define PCLOSE _pclose
#else
  #define POPEN  popen
  #define PCLOSE pclose
#endif

namespace {
std::FILE*  g_pipe = nullptr;
int         g_w = 0, g_h = 0;
std::string g_out;
} // namespace

bool recActive() { return g_pipe != nullptr; }

bool recStart(const std::string& outPath, int w, int h, int fps, std::string* err)
{
    if (g_pipe) { if (err) *err = "recorder already running"; return false; }
    if (w <= 0 || h <= 0 || fps <= 0)
    {
        if (err) *err = "bad record parameters";
        return false;
    }

    // -y 覆盖旧文件；-loglevel error 只保留错误输出，不干扰控制台
    // -f rawvideo + -i - 表示从 stdin 读原始帧
    // -vf vflip 把 glReadPixels 的自下而上行序翻正
    // -pix_fmt yuv420p 是兼容性最好的像素格式（QuickTime / 播放器 / PPT 都能放）
    char cmd[1200];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -hide_banner -loglevel error "
        "-f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate %d -i - "
        "-vf vflip -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p \"%s\"",
        w, h, fps, outPath.c_str());

    g_pipe = POPEN(cmd, "wb");
    if (!g_pipe)
    {
        if (err) *err = "failed to start ffmpeg (is ffmpeg.exe on PATH?)";
        return false;
    }
    g_w = w;
    g_h = h;
    g_out = outPath;
    return true;
}

bool recWriteFrame(const unsigned char* rgb)
{
    if (!g_pipe || !rgb) return false;
    const size_t n = (size_t)g_w * (size_t)g_h * 3u;
    return std::fwrite(rgb, 1, n, g_pipe) == n;
}

bool recStop(std::string* err)
{
    if (!g_pipe) return false;

    std::FILE* p = g_pipe;
    g_pipe = nullptr;

    // 关掉 stdin 让 ffmpeg 收尾（写 moov atom），_pclose 会等它退出
    const int rc = PCLOSE(p);
    if (rc != 0)
    {
        if (err) *err = "ffmpeg exited with code " + std::to_string(rc);
        return false;
    }
    return true;
}
