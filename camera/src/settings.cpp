#include "engine.hpp"
#include <glib.h>
#include <arpa/inet.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <stdexcept>
namespace camera {
static void require(bool ok,const char *msg){if(!ok)throw std::runtime_error(msg);}
void preset(Settings &s,int index){
    const int widths[]={320,240,160},rates[]={2,4,6};
    require(index>=0&&index<3,"Onbekend beeldprofiel.");
    s.width=widths[index];s.fps=rates[index];s.gop=s.fps*5;s.video_kbps=0;
}
int budget_limit(const Settings &s){
    // ~9.6 SI packets/s; reserve 15 kbit/s, PES/final-packet losses
    // proportional to fps, then 25% headroom for live scene changes.
    // PCR can share video packets. Runtime scheduling still checks deadlines.
    return std::max(0,(dvb_bitrate(s.dvb)-15000-s.fps*900)*75/100000);
}
int video_budget(const Settings &s){return s.video_kbps?s.video_kbps:budget_limit(s);}
void validate(const Settings &s){
    require(dvb_bitrate(s.dvb)>=DATV_MIN_BITRATE,"DVB-profiel onder 30.080 bit/s; kies een hogere FEC of symbolrate/bandbreedte.");
    require(s.width>=160&&s.width<=640&&s.width%2==0&&(s.aspect==43||s.aspect==169),"Breedte: even getal 160–640; beeldverhouding 4:3 of 16:9.");
    require(s.fps>=2&&s.fps<=12&&s.gop>=s.fps&&s.gop<=s.fps*15,"Kies 2–12 beelden/s en een sleutelbeeldinterval van 1–15 seconden.");
    require(s.video_kbps>=0&&video_budget(s)>=3&&video_budget(s)<=budget_limit(s),"Te weinig videobudget: verlaag fps of verhoog de DVB-capaciteit. Video 0 = automatisch.");
    require(s.denoise>=0&&s.denoise<=2&&(s.monochrome==0||s.monochrome==1),"Ongeldige ruisfilter- of kleurkeuze.");
    require(s.seconds>=0&&s.seconds<=86400&&s.pattern>=0&&s.pattern<=2,"Ongeldige testduur of testbron.");
    require(!s.callsign.empty()&&s.callsign.size()<=16,"Roepnaam: 1–16 letters, cijfers, / of -.");
    for(char c:s.callsign)require((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-',"Roepnaam: gebruik hoofdletters, cijfers, / of -.");
    require(s.device.rfind("/dev/video",0)==0&&s.device.size()>10&&s.device.size()<40,"Camera-adres moet /dev/videoN zijn.");
    for(size_t i=10;i<s.device.size();++i)require(g_ascii_isdigit(s.device[i]),"Camera-adres moet /dev/videoN zijn.");
    in_addr addr{};require(inet_pton(AF_INET,s.ip.c_str(),&addr)==1,"Gebruik een numeriek IPv4-adres voor de zender.");
    unsigned a=ntohl(addr.s_addr);require((a>>24)>0&&(a>>24)<224&&a!=0xffffffff,"Gebruik een unicast IPv4-adres.");
    require(s.port>0&&s.port<=65535,"UDP-poort moet 1–65535 zijn.");
    require(!(s.preview_only&&!s.capture.empty()),"Kies preview of opname.");
}
static void fields(GKeyFile *k,Settings &s,bool save){
    auto number=[&](const char *key,int &v){
        if(save)g_key_file_set_integer(k,"camera",key,v);
        else if(g_key_file_has_key(k,"camera",key,nullptr)){
            GError *e=nullptr;int n=g_key_file_get_integer(k,"camera",key,&e);
            if(e){g_error_free(e);throw std::runtime_error(std::string("Ongeldige instelling: ")+key);}v=n;
        }
    };
    auto str=[&](const char *key,std::string &v){
        if(save)g_key_file_set_string(k,"camera",key,v.c_str());
        else if(g_key_file_has_key(k,"camera",key,nullptr)){
            GError *e=nullptr;char *p=g_key_file_get_string(k,"camera",key,&e);
            if(e){g_error_free(e);throw std::runtime_error(std::string("Ongeldige tekstinstelling: ")+key);}v=p;g_free(p);
        }
    };
    str("device",s.device);str("callsign",s.callsign);str("ip",s.ip);number("port",s.port);
    number("width",s.width);number("aspect",s.aspect);number("fps",s.fps);number("gop",s.gop);number("video_kbps",s.video_kbps);
    number("denoise",s.denoise);number("monochrome",s.monochrome);
    number("system",s.dvb.system);number("symbol_rate_id",s.dvb.symbol_rate);number("fec_id",s.dvb.fec);
    number("pilots",s.dvb.pilots);number("guard_id",s.dvb.guard);number("bandwidth_khz",s.dvb.bandwidth_khz);
}
Settings load_settings(const std::string &path){
    Settings s;GKeyFile *k=g_key_file_new();GError *e=nullptr;
    if(!g_key_file_load_from_file(k,path.c_str(),G_KEY_FILE_NONE,&e)){
        std::string msg=e->message;g_error_free(e);g_key_file_unref(k);throw std::runtime_error(msg);
    }
    try{fields(k,s,false);validate(s);}catch(...){g_key_file_unref(k);throw;}
    g_key_file_unref(k);return s;
}
void save_settings(const Settings &source,const std::string &path){
    validate(source);Settings s=source;GKeyFile *k=g_key_file_new();fields(k,s,true);
    gsize size=0;char *data=g_key_file_to_data(k,&size,nullptr);GError *e=nullptr;
    bool ok=g_file_set_contents_full(path.c_str(),data,size,G_FILE_SET_CONTENTS_CONSISTENT,0600,&e);
    g_free(data);g_key_file_unref(k);
    if(!ok){std::string msg=e->message;g_error_free(e);throw std::runtime_error(msg);}
}
std::vector<Device> devices(){
    std::vector<Device> result;
    for(int i=0;i<64;++i){
        std::string path="/dev/video"+std::to_string(i);int fd=open(path.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        v4l2_capability cap{};
        if(ioctl(fd,VIDIOC_QUERYCAP,&cap)==0){
            unsigned caps=cap.capabilities&V4L2_CAP_DEVICE_CAPS?cap.device_caps:cap.capabilities;
            if(caps&(V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_VIDEO_CAPTURE_MPLANE))result.push_back({path,reinterpret_cast<char*>(cap.card)});
        }close(fd);
    }return result;
}
}
