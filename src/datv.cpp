#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "datv.h"
#include "core.h"
#include <wels/codec_api.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <stdexcept>

namespace {
using Bytes = std::vector<unsigned char>;
constexpr int VIDEO = 0x100, PMT = 0x1000;
constexpr uint64_t CLOCK = 90000;
struct Frame { Bytes pes; uint64_t release, deadline; bool idr; };
struct EncoderDelete {
    void operator()(ISVCEncoder *p) const { if (p) { p->Uninitialize(); WelsDestroySVCEncoder(p); } }
};
unsigned char clip(int v) { return static_cast<unsigned char>(std::max(0, std::min(255, v))); }
Bytes yuv420(const uint32_t *rgb, int w, int h, int stride) {
    Bytes out(w*h*3/2);
    for (int y=0; y<h; ++y) {
        const uint32_t *row = reinterpret_cast<const uint32_t *>(reinterpret_cast<const unsigned char *>(rgb)+y*stride);
        for (int x=0; x<w; ++x) {
            int r=(row[x]>>16)&255, g=(row[x]>>8)&255, b=row[x]&255;
            out[y*w+x] = clip(((66*r+129*g+25*b+128)>>8)+16);
        }
    }
    for (int y=0; y<h; y+=2) for (int x=0; x<w; x+=2) {
        int r=0, g=0, b=0;
        for (int dy=0; dy<2; ++dy) {
            const uint32_t *row = reinterpret_cast<const uint32_t *>(reinterpret_cast<const unsigned char *>(rgb)+(y+dy)*stride);
            for (int dx=0; dx<2; ++dx) { uint32_t v=row[x+dx]; r+=(v>>16)&255; g+=(v>>8)&255; b+=v&255; }
        }
        r=(r+2)/4; g=(g+2)/4; b=(b+2)/4;
        int i=(y/2)*(w/2)+x/2;
        out[w*h+i] = clip(((-38*r-74*g+112*b+128)>>8)+128);
        out[w*h+w*h/4+i] = clip(((112*r-94*g-18*b+128)>>8)+128);
    }
    return out;
}
void timestamp(Bytes &b, uint64_t t) {
    t &= (UINT64_C(1)<<33)-1;
    b.push_back(0x21|((t>>29)&14)); b.push_back(t>>22);
    b.push_back(((t>>14)&254)|1); b.push_back(t>>7); b.push_back(((t<<1)&254)|1);
}
bool encode(Bytes &yuv, int w, int h, DatvSettings s, int qp,
            std::vector<Frame> &frames, int &largest, const std::atomic_bool *stop=nullptr, int limit=0) {
    ISVCEncoder *raw=nullptr;
    if (WelsCreateSVCEncoder(&raw) != 0) return false;
    std::unique_ptr<ISVCEncoder, EncoderDelete> encoder(raw);
    int log=WELS_LOG_QUIET;
    encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &log);
    SEncParamExt p{};
    if (encoder->GetDefaultParams(&p) != 0) return false;
    p.iUsageType=CAMERA_VIDEO_REAL_TIME;
    p.iPicWidth=w; p.iPicHeight=h; p.fMaxFrameRate=static_cast<float>(s.fps);
    /* For an offline static card, try fixed qualities and verify the actual
     * complete TS schedule. This avoids rate-control frame skipping. */
    p.iRCMode=RC_OFF_MODE; p.iTargetBitrate=s.bitrate;
    p.iTemporalLayerNum=1; p.iSpatialLayerNum=1;
    p.uiIntraPeriod=s.gop; p.bEnableFrameSkip=false;
    p.iMultipleThreadIdc=1; p.bEnableAdaptiveQuant=false;
    p.iMinQp=qp; p.iMaxQp=qp; p.eSpsPpsIdStrategy=CONSTANT_ID;
    p.bEnableSceneChangeDetect=false;
    auto &l=p.sSpatialLayers[0];
    l.iVideoWidth=w; l.iVideoHeight=h; l.fFrameRate=p.fMaxFrameRate;
    l.iSpatialBitrate=s.bitrate; l.iMaxSpatialBitrate=s.bitrate;
    l.uiProfileIdc=PRO_BASELINE; l.iDLayerQp=qp;
    l.bVideoSignalTypePresent=true; l.uiVideoFormat=5; l.bFullRange=false;
    l.bColorDescriptionPresent=true;
    l.uiColorPrimaries=6; l.uiTransferCharacteristics=6; l.uiColorMatrix=6; // BT.601 / SMPTE 170M
    l.bAspectRatioPresent=true; l.eAspectRatio=ASP_1x1;
    l.sSliceArgument.uiSliceMode=SM_SINGLE_SLICE;
    if (encoder->InitializeExt(&p) != 0) return false;
    SSourcePicture pic{};
    pic.iColorFormat=videoFormatI420; pic.iPicWidth=w; pic.iPicHeight=h;
    pic.iStride[0]=w; pic.iStride[1]=pic.iStride[2]=w/2;
    pic.pData[0]=yuv.data(); pic.pData[1]=yuv.data()+w*h; pic.pData[2]=pic.pData[1]+w*h/4;
    frames.clear(); largest=0;
    for (int i=0; i<(limit?limit:s.seconds*s.fps); ++i) {
        if (stop && stop->load()) return false;
        pic.uiTimeStamp=static_cast<long long>(i)*1000/s.fps;
        if (i%s.gop==0 && encoder->ForceIntraFrame(true) != 0) return false;
        SFrameBSInfo bs{};
        if (encoder->EncodeFrame(&pic, &bs) != 0 || bs.eFrameType==videoFrameTypeSkip) return false;
        Frame f{{0,0,1,0xe0,0,0,0x80,0x80,5}, uint64_t(i)*CLOCK/s.fps,
                 CLOCK+uint64_t(i)*CLOCK/s.fps, bs.eFrameType==videoFrameTypeIDR};
        timestamp(f.pes, f.deadline);
        // Access unit delimiter, followed by Annex B NAL units (SPS/PPS on IDR).
        f.pes.insert(f.pes.end(), {0,0,0,1,9,0xf0});
        for (int j=0; j<bs.iLayerNum; ++j) {
            const SLayerBSInfo &layer=bs.sLayerInfo[j];
            int size=0;
            for (int n=0; n<layer.iNalCount; ++n) size+=layer.pNalLengthInByte[n];
            f.pes.insert(f.pes.end(), layer.pBsBuf, layer.pBsBuf+size);
        }
        if (f.idr) largest=std::max(largest, static_cast<int>(f.pes.size())-14);
        frames.push_back(std::move(f));
    }
    return true;
}
void crc(Bytes &b) {
    uint32_t value=0xffffffff;
    for (unsigned char v:b) {
        value^=uint32_t(v)<<24;
        for (int i=0; i<8; ++i) value=(value<<1)^((value&0x80000000)?0x04c11db7:0);
    }
    for (int i=24; i>=0; i-=8) b.push_back(value>>i);
}
Bytes pat() {
    Bytes b={0,0xb0,13,0,1,0xc1,0,0,0,1,0xf0,0}; crc(b); return b;
}
Bytes pmt() {
    Bytes b={2,0xb0,18,0,1,0xc1,0,0,0xe1,0,0xf0,0,0x1b,0xe1,0,0xf0,0}; crc(b); return b;
}
Bytes sdt(const char *call) {
    size_t n=std::strlen(call);
    Bytes b={0x42,0xf0,0,0,1,0xc1,0,0,0,1,0xff,0,1,0xfc,0x80,
             static_cast<unsigned char>(5+2*n),0x48,static_cast<unsigned char>(3+2*n),0x16,
             static_cast<unsigned char>(n)};
    b.insert(b.end(), call, call+n); b.push_back(n); b.insert(b.end(),call,call+n);
    b[2]=b.size()+1; crc(b); return b;
}
void header(unsigned char *p, int pid, bool start, int cc) {
    std::memset(p,255,188); p[0]=0x47; p[1]=(pid>>8)|(start?0x40:0); p[2]=pid; p[3]=0x10|cc;
}
uint64_t byte_time(uint64_t bytes, uint64_t scale, int bitrate) {
    return (bytes/bitrate)*8*scale+(bytes%bitrate)*8*scale/bitrate;
}
void write_pcr(unsigned char *p, uint64_t packet, int bitrate) {
    // Reference the arrival of the last PCR byte, not the start of the packet.
    uint64_t ticks=byte_time(packet*188+11,27000000,bitrate), base=ticks/300;
    unsigned ext=ticks%300;
    p[0]=base>>25; p[1]=base>>17; p[2]=base>>9; p[3]=base>>1;
    p[4]=((base&1)<<7)|0x7e|(ext>>8); p[5]=ext;
}
class Mux {
    const std::vector<Frame> &frames;
    DatvSettings s;
    Bytes tables[3], pes;
    const int pids[3]={0,PMT,0x11};
    uint64_t due[3]={0,0,0}, last_pcr_packet=0, index=0;
    bool first_table[3]={true,true,true};
    int cc[3]={0,0,0}, video_cc=0;
    size_t offset=0;
    bool repeat;
public:
    uint64_t frame=0;
    Mux(const std::vector<Frame> &f, DatvSettings settings, const char *call, bool loop=false)
        : frames(f),s(settings),tables{pat(),pmt(),sdt(call)},repeat(loop) {}
    bool next(unsigned char packet[188]) {
        uint64_t now=byte_time(index*188,CLOCK,s.bitrate);
        uint64_t end=byte_time((index+1)*188,CLOCK,s.bitrate);
        uint64_t release=(frame/s.fps)*CLOCK+(frame%s.fps)*CLOCK/s.fps;
        bool ready=(repeat || frame<frames.size()) && release<=now;
        if (ready && end>release+CLOCK) return false;
        const Frame *f=ready?&frames[frame%frames.size()]:nullptr;
        const uint64_t pcr_packets=std::max(2,s.bitrate*40/(1504*1000));
        bool pcr=(index==0 || index-last_pcr_packet>=pcr_packets);
        int table=-1;
        for (int t=0; t<3; ++t) if (now>=due[t]) { table=t; break; }
        if (pcr || (ready && table<0)) {
            bool start=ready && offset==0;
            bool random=start && f->idr;
            if (start) {
                pes=f->pes;
                Bytes pts; timestamp(pts,release+CLOCK);
                std::copy(pts.begin(),pts.end(),pes.begin()+9);
            }
            header(packet,VIDEO,start,ready?video_cc:((video_cc+15)&15));
            int payload=ready?static_cast<int>(std::min<size_t>(pcr?176:(random?182:184),pes.size()-offset)):0;
            if (payload<184) {
                packet[3]=(packet[3]&15)|(payload?0x30:0x20);
                packet[4]=183-payload;
                if (packet[4]) packet[5]=(pcr?0x10:0)|(random?0x40:0)|(index==0?0x80:0);
                if (pcr) { write_pcr(packet+6,index,s.bitrate); last_pcr_packet=index; }
            }
            if (payload) {
                std::memcpy(packet+188-payload,pes.data()+offset,payload);
                video_cc=(video_cc+1)&15; offset+=payload;
                if (offset==pes.size()) { ++frame; offset=0; }
            }
        } else if (table>=0) {
            header(packet,pids[table],true,cc[table]); cc[table]=(cc[table]+1)&15;
            int payload=4;
            if (first_table[table]) {
                packet[3]|=0x20; packet[4]=1; packet[5]=0x80;
                payload=6; first_table[table]=false;
            }
            packet[payload]=0; std::memcpy(packet+payload+1,tables[table].data(),tables[table].size());
            due[table]=now+(table==2?CLOCK:CLOCK/5);
        } else header(packet,0x1fff,false,0);
        ++index;
        return true;
    }
};

bool multiplex(const std::vector<Frame> &frames, DatvSettings s, const char *call, Bytes &out) {
    Mux mux(frames,s,call);
    uint64_t count=(uint64_t(s.seconds+1)*s.bitrate+1504*7-1)/(1504*7)*7;
    out.clear(); out.reserve(count*188);
    for (uint64_t i=0; i<count; ++i) {
        unsigned char packet[188];
        if (!mux.next(packet)) return false;
        out.insert(out.end(),packet,packet+188);
    }
    return mux.frame==frames.size();
}
}

int datv_write(FILE *output, const uint32_t *rgb, int width, int height,
               int stride, const char *call, DatvSettings s, DatvResult *result, char error[256]) {
    error[0]=0;
    const char *invalid=datv_validate(s,width,height);
    if (invalid) { std::snprintf(error,256,"%s",invalid); return 0; }
    if (!output || !rgb || !call || stride<width*4 || stride%4 || std::strlen(call)<3 || std::strlen(call)>24) {
        std::snprintf(error,256,"Ongeldig beeld, roepnaam of uitvoerbestand."); return 0;
    }
    wchar_t wide_call[25]={0};
    for (size_t i=0; call[i]; ++i) wide_call[i]=static_cast<unsigned char>(call[i]);
    if (!valid_call(wide_call)) { std::snprintf(error,256,"Ongeldige roepnaam."); return 0; }
    try {
        Bytes yuv=yuv420(rgb,width,height,stride), ts;
        std::vector<Frame> frames;
        for (int qp=24; qp<=48; qp+=4) {
            int largest=0;
            if (!encode(yuv,width,height,s,qp,frames,largest)) {
                std::snprintf(error,256,"OpenH264 kon het beeld niet coderen."); return 0;
            }
            if (!multiplex(frames,s,call,ts)) continue;
            if (std::fwrite(ts.data(),1,ts.size(),output)!=ts.size() || std::fflush(output)!=0) {
                std::snprintf(error,256,"TS schrijven is mislukt. Controleer vrije ruimte en schrijfrechten."); return 0;
            }
            if (result) *result={qp,static_cast<int>(frames.size()),largest};
            return 1;
        }
        std::snprintf(error,256,"Beeld past niet binnen deze TS-bitrate en een seconde buffer. Kies een lagere resolutie, lagere beeldfrequentie of langere GOP.");
    } catch (const std::bad_alloc &) { std::snprintf(error,256,"Onvoldoende geheugen voor TS-export."); }
    return 0;
}

struct DatvStream {
    std::atomic_bool stop{false};
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    DatvUdpStatus status{};
    std::chrono::steady_clock::time_point started;
};

namespace {
class UdpSocket {
    sockaddr_in address{};
#ifdef _WIN32
    SOCKET fd=INVALID_SOCKET;
    bool initialized=false, timer=false;
    static int last_error() { return WSAGetLastError(); }
#else
    int fd=-1;
    static int last_error() { return errno; }
#endif
    static void failed(int error=last_error()) { throw std::runtime_error("UDP-netwerkfout (code "+std::to_string(error)+"). Controleer het adres en het netwerk."); }
    void create_socket() {
        fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
#ifdef _WIN32
        if (fd==INVALID_SOCKET) failed();
        u_long nonblocking=1;
        if (ioctlsocket(fd,FIONBIO,&nonblocking)!=0) failed();
#else
        if (fd<0) failed();
        int flags=fcntl(fd,F_GETFL,0);
        if (flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0) failed();
#endif
        if (connect(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))!=0) failed();
    }
public:
    ~UdpSocket() {
#ifdef _WIN32
        if (fd!=INVALID_SOCKET) closesocket(fd);
        if (timer) timeEndPeriod(1);
        if (initialized) WSACleanup();
#else
        if (fd>=0) close(fd);
#endif
    }
    void open(const DatvUdpSettings &s) {
#ifdef _WIN32
        WSADATA data;
        int err=WSAStartup(MAKEWORD(2,2),&data);
        if (err) throw std::runtime_error("Kan Windows-netwerk niet starten (code "+std::to_string(err)+").");
        initialized=true;
        timer=timeBeginPeriod(1)==TIMERR_NOERROR;
#endif
        address.sin_family=AF_INET; address.sin_port=htons(s.port);
        if (inet_pton(AF_INET,s.ip,&address.sin_addr)!=1) throw std::runtime_error("Ongeldig IPv4-adres.");
        create_socket();
    }
    bool packet(const unsigned char *data) {
        int sent=send(fd,reinterpret_cast<const char *>(data),1316,0);
        if (sent==1316) return true;
        if (sent>=0) throw std::runtime_error("UDP-datagram onvolledig verzonden.");
        int error=last_error();
#ifdef _WIN32
        if (error!=WSAECONNREFUSED && error!=WSAECONNRESET) failed(error);
        // Winsock documents the socket as unusable after WSAECONNRESET.
        closesocket(fd); fd=INVALID_SOCKET;
        create_socket();
#else
        if (error!=ECONNREFUSED) failed(error);
#endif
        // An ICMP port refusal may refer to a previous datagram. Keep the live
        // timeline advancing: no retransmission or backlog when RX starts later.
        return false;
    }
};

void stream_worker(DatvStream *stream, std::vector<uint32_t> rgb, int w, int h,
                   std::string call, DatvUdpSettings settings) {
    try {
        UdpSocket socket;
        socket.open(settings);
        DatvSettings s=settings.video;
        Bytes yuv=yuv420(rgb.data(),w,h,w*4);
        std::vector<Frame> frames;
        int selected=0;
        for (int qp=24; qp<=48 && !stream->stop; qp+=4) {
            int largest=0;
            // Consecutive IDR access units must have different idr_pic_id
            // (H.264 7.4.3). Keep two encoder-produced IDRs for GOP=1 so
            // the IDs alternate across template boundaries as well.
            if (!encode(yuv,w,h,s,qp,frames,largest,&stream->stop,std::max(2,s.gop))) {
                if (stream->stop) break;
                throw std::runtime_error("OpenH264 kon het beeld niet coderen.");
            }
            // Exercise a continuous, repeated closed GOP, including PSI phases.
            // A finite file's trailing second must never hide a bitrate deficit.
            Mux probe(frames,s,call.c_str(),true);
            uint64_t duration=std::max(30,(3*s.gop+s.fps-1)/s.fps);
            uint64_t count=duration*s.bitrate/1504;
            bool fits=true; unsigned char packet[188];
            for (uint64_t i=0;i<count;++i) {
                if (stream->stop) { fits=false; break; }
                if (!probe.next(packet)) { fits=false; break; }
            }
            if (fits) { selected=qp; break; }
        }
        if (!stream->stop && !selected)
            throw std::runtime_error("Beeld past niet in de doorlopende TS. Kies een lagere resolutie, lagere beeldfrequentie of langere GOP.");
        if (!stream->stop) {
            Mux mux(frames,s,call.c_str(),true);
            const auto origin=std::chrono::steady_clock::now();
            const auto interval=std::chrono::nanoseconds(byte_time(1316,1000000000,s.bitrate));
            {
                std::lock_guard<std::mutex> lock(stream->mutex);
                stream->started=origin; stream->status.qp=selected;
            }
            for (uint64_t n=0; !stream->stop; ++n) {
                unsigned char datagram[1316];
                for (int p=0;p<7;++p) if (!mux.next(datagram+p*188))
                    throw std::runtime_error("UDP gestopt: videodata overschrijdt de beschikbare bitrate.");
                auto due=origin+std::chrono::nanoseconds(byte_time(n*1316,1000000000,s.bitrate));
                {
                    std::unique_lock<std::mutex> lock(stream->mutex);
                    stream->wake.wait_until(lock,due,[&] { return stream->stop.load(); });
                }
                if (stream->stop) break;
                // Never flush a backlog of datagrams after suspend or overload.
                if (std::chrono::steady_clock::now()-due>interval)
                    throw std::runtime_error("UDP gestopt: verzending liep te ver achter (slaapstand of systeembelasting). Start opnieuw.");
                bool sent=socket.packet(datagram);
                std::lock_guard<std::mutex> lock(stream->mutex);
                stream->status.state=DATV_RUNNING;
                if (sent) ++stream->status.packets;
                else ++stream->status.refusals;
                stream->status.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-origin).count();
            }
        }
        std::lock_guard<std::mutex> lock(stream->mutex);
        stream->status.state=DATV_STOPPED;
    } catch (const std::exception &e) {
        std::lock_guard<std::mutex> lock(stream->mutex);
        stream->status.state=stream->stop?DATV_STOPPED:DATV_FAILED;
        std::snprintf(stream->status.error,sizeof(stream->status.error),"%s",e.what());
    }
}
}

DatvStream *datv_udp_start(const uint32_t *rgb, int width, int height, int stride,
                          const char *call, DatvUdpSettings settings, char error[256]) {
    error[0]=0;
    const char *invalid=datv_udp_validate(settings,width,height);
    if (invalid) { std::snprintf(error,256,"%s",invalid); return nullptr; }
    if (!rgb || !call || stride<width*4 || stride%4 || std::strlen(call)>24) {
        std::snprintf(error,256,"Ongeldig beeld of roepnaam."); return nullptr;
    }
    wchar_t wide[25]={0};
    for (size_t i=0;call[i];++i) wide[i]=static_cast<unsigned char>(call[i]);
    if (!valid_call(wide)) { std::snprintf(error,256,"Ongeldige roepnaam."); return nullptr; }
    try {
        std::vector<uint32_t> copy(width*height);
        for (int y=0;y<height;++y)
            std::memcpy(copy.data()+y*width,reinterpret_cast<const unsigned char *>(rgb)+y*stride,width*4);
        auto stream=std::make_unique<DatvStream>();
        stream->worker=std::thread(stream_worker,stream.get(),std::move(copy),width,height,std::string(call),settings);
        return stream.release();
    } catch (const std::exception &e) { std::snprintf(error,256,"UDP starten mislukt: %s",e.what()); return nullptr; }
}
void datv_udp_status(DatvStream *stream, DatvUdpStatus *status) {
    std::lock_guard<std::mutex> lock(stream->mutex); *status=stream->status;
}
void datv_udp_stop(DatvStream *stream) {
    if (!stream) return;
    { std::lock_guard<std::mutex> lock(stream->mutex); stream->stop=true; }
    stream->wake.notify_all();
}
void datv_udp_destroy(DatvStream *stream) {
    if (!stream) return;
    datv_udp_stop(stream);
    if (stream->worker.joinable()) stream->worker.join();
    delete stream;
}
