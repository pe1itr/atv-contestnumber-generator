#include "datv.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
int main(int argc,char **argv) {
    if (argc!=6) return 1;
    DatvUdpSettings s=datv_udp_defaults(); std::strcpy(s.ip,"127.0.0.1");
    s.port=std::atoi(argv[1]); double duration=std::atof(argv[2]);
    s.video.bitrate=std::atoi(argv[3]); s.video.fps=std::atoi(argv[4]); s.video.gop=std::atoi(argv[5]);
    std::vector<uint32_t> pixels(160*120);
    for (int y=0;y<120;++y) for (int x=0;x<160;++x)
        pixels[y*160+x]=((x/23+y/29)%2)?0xffff00:0x000080;
    char error[256];
    DatvStream *stream=datv_udp_start(pixels.data(),160,120,160*4,"PE1ITR",s,error);
    if (!stream) { std::fprintf(stderr,"%s\n",error); return 1; }
    auto start=std::chrono::steady_clock::now();
    DatvUdpStatus status;
    for (;;) {
        datv_udp_status(stream,&status);
        if (status.state==DATV_FAILED) {
            std::fprintf(stderr,"%s\n",status.error); datv_udp_destroy(stream); return 2;
        }
        if (status.state==DATV_RUNNING && status.seconds>=duration) break;
        if (std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()>duration+20) {
            datv_udp_destroy(stream); return 3;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto stop=std::chrono::steady_clock::now();
    datv_udp_stop(stream);
    for (;;) {
        datv_udp_status(stream,&status);
        if (status.state==DATV_STOPPED || status.state==DATV_FAILED) break;
        if (std::chrono::steady_clock::now()-stop>std::chrono::seconds(2)) return 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    datv_udp_destroy(stream);
    double stop_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stop).count();
    std::printf("{\"packets\":%llu,\"stop_ms\":%.3f,\"qp\":%d}\n",(unsigned long long)status.packets,stop_ms,status.qp);
    // Cancellation while preparing and repeated destruction must not hang.
    for (int n=0;n<3;++n) {
        s.video.gop=250;
        stream=datv_udp_start(pixels.data(),160,120,160*4,"PE1ITR",s,error);
        if (!stream) return 5;
        datv_udp_stop(stream); datv_udp_destroy(stream);
    }
    return 0;
}
