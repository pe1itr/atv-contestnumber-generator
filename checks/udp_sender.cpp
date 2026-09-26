#include "datv.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
int main(int argc,char **argv) {
    if (argc!=6 && argc!=8 && (argc!=7 || (std::strcmp(argv[6],"eit") && std::strcmp(argv[6],"teletext")))) return 1;
    int width=argc==8?std::atoi(argv[6]):160, height=argc==8?std::atoi(argv[7]):120;
    if (width<16 || height<16 || width>1920 || height>1440 || width%2 || height%2) return 1;
    DatvUdpSettings s=datv_udp_defaults(); std::strcpy(s.ip,"127.0.0.1");
    s.port=std::atoi(argv[1]); double duration=std::atof(argv[2]);
    s.video.bitrate=std::atoi(argv[3]); s.video.fps=std::atoi(argv[4]); s.video.gop=std::atoi(argv[5]);
    if (argc==7 && !std::strcmp(argv[6],"teletext")) {
        s.video.teletext.enabled=1;
        if (teletext_from_text(&s.video.teletext,"ATV CONTEST\nPagina 100\n\n73 de PE1ITR")) return 1;
    }
    if (argc==7) {
        s.video.eit_enabled=1; std::strcpy(s.video.locator,"JO21QK");
        std::strcpy(s.video.station.city,"Eindhoven");
        std::strcpy(s.video.station.operator_name,"René");
        std::strcpy(s.video.station.description,"ATV-contest; 70 cm; antenne richting zuid.");
    }
    std::vector<uint32_t> pixels(width*height);
    // Scale the same reference pattern so resolution changes preserve the scene.
    for (int y=0;y<height;++y) for (int x=0;x<width;++x)
        pixels[y*width+x]=(((x*160/width)/23+(y*120/height)/29)%2)?0xffff00:0x000080;
    char error[256];
    DatvStream *stream=datv_udp_start(pixels.data(),width,height,width*4,"PE1ITR",s,error);
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
    char status_text[512]; datv_udp_status_text(s,status,status_text,sizeof(status_text));
    bool recent_warning=std::strstr(status_text,"gesloten UDP-poort")!=nullptr;
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
    std::printf("{\"packets\":%llu,\"refusals\":%llu,\"stop_ms\":%.3f,\"qp\":%d,\"recent_warning\":%s}\n",(unsigned long long)status.packets,(unsigned long long)status.refusals,stop_ms,status.qp,recent_warning?"true":"false");
    // Cancellation while preparing and repeated destruction must not hang.
    for (int n=0;n<3;++n) {
        s.video.gop=250;
        stream=datv_udp_start(pixels.data(),width,height,width*4,"PE1ITR",s,error);
        if (!stream) return 5;
        datv_udp_stop(stream); datv_udp_destroy(stream);
    }
    return 0;
}
