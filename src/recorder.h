#pragma once
// 视频录制：把逐帧 RGB 数据管进 ffmpeg 的 stdin，由它编码成 H.264 mp4。
// 全程不落临时文件。
//
// 用管道而不是「先存一堆图再调 ffmpeg」的原因：1280x800 一帧就是 3 MB，
// 6 秒 30fps 要 540 MB 临时空间，而管道是流式的。
#include <string>

// 启动 ffmpeg 子进程。w/h 必须与实际帧尺寸一致，fps 决定输出帧率。
bool recStart(const std::string& outPath, int w, int h, int fps, std::string* err);

// 写入一帧。rgb 为自下而上的 RGB24（glReadPixels 的原生行序），
// ffmpeg 侧用 -vf vflip 翻正。
bool recWriteFrame(const unsigned char* rgb);

// 关闭管道并等待 ffmpeg 结束。返回 false 表示编码失败（含 ffmpeg 不在 PATH）。
bool recStop(std::string* err);

bool recActive();
