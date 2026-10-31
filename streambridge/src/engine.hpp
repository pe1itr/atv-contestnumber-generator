#pragma once
#include "core.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
struct Settings {
    static constexpr int max_width=1920, max_height=1440;
    std::string uri="srt://:4001?mode=listener", ip="127.0.0.1", callsign="NOCALL", capture;
    DvbSettings dvb={DVB_S2,4,1,0,2,500};
    std::string rtsp_uri="rtsp://0.0.0.0:8554/live";
    int input=0; // 0 SRT, 1 RTSP RECORD receiver
    int port=10000, latency=200, width=320, aspect=43, fps=25, gop=25;
    int video_kbps=0, audio_kbps=-1; // video 0 = automatic budget; audio -1 = automatic, 0 = disabled
    // Round the raster height to the nearest even number; SAR preserves exact DAR.
    int height() const { return aspect==169 ? 2*((width*9+16)/32) : 2*((width*3+4)/8); }
    std::string pixel_aspect() const {
        return std::to_string((aspect==169?16:4)*height())+"/"+std::to_string((aspect==169?9:3)*width);
    }
    bool test_source=false;
    int seconds=0;
};
int audio_budget(const Settings &s);
int video_budget(const Settings &s);
void validate(const Settings &s);
void save_settings(const Settings &s,const std::string &path);
Settings load_settings(const std::string &path);
struct Status { bool running=false, done=false; uint64_t datagrams=0; std::string text; };
class Engine {
    struct Impl;
    std::unique_ptr<Impl> p;
public:
    explicit Engine(Settings settings);
    ~Engine();
    void stop();
    Status status();
};
