#pragma once
#include "core.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace camera {
struct Settings {
    std::string device="/dev/video0", callsign="NOCALL", ip="127.0.0.1", capture;
    DvbSettings dvb={DVB_S,8,0,0,2,35};
    int port=10000, width=240, aspect=43, fps=4, gop=20, video_kbps=0;
    int denoise=1, monochrome=0, pattern=0, seconds=0;
    bool test_source=false, preview_only=false;
    int height() const {return aspect==169?2*((width*9+16)/32):2*((width*3+4)/8);}
};
void preset(Settings &s,int index);
int budget_limit(const Settings &s);
int video_budget(const Settings &s);
void validate(const Settings &s);
Settings load_settings(const std::string &path);
void save_settings(const Settings &s,const std::string &path);
struct Device {std::string path,name;};
std::vector<Device> devices();
struct Picture {int width=0,height=0;uint64_t serial=0;std::vector<uint8_t> rgb;};
struct Status {bool done=false,failed=false;uint64_t datagrams=0,frames=0;std::string text="Camera en encoder starten…";};
class Engine {
    struct Impl;std::unique_ptr<Impl> p;
public:
    explicit Engine(Settings s);
    ~Engine();
    void stop();
    Status status();
    Picture picture();
};
}
