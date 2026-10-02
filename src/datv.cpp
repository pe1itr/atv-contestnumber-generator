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
#include <deque>
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
                 uint64_t(datv_buffer_ms(s))*90+uint64_t(i)*CLOCK/s.fps, bs.eFrameType==videoFrameTypeIDR};
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
struct DecoderDelete {
    void operator()(ISVCDecoder *p) const { if (p) { p->Uninitialize(); WelsDestroyDecoder(p); } }
};
std::vector<uint32_t> decode_preview(const Frame &frame, int width, int height) {
    ISVCDecoder *raw=nullptr;
    if (WelsCreateDecoder(&raw)!=0) throw std::runtime_error("Cannot start the preview decoder.");
    std::unique_ptr<ISVCDecoder,DecoderDelete> decoder(raw);
    int log=WELS_LOG_QUIET; decoder->SetOption(DECODER_OPTION_TRACE_LEVEL,&log);
    SDecodingParam params{};
    params.sVideoProperty.size=sizeof(params.sVideoProperty);
    params.sVideoProperty.eVideoBsType=VIDEO_BITSTREAM_AVC;
    params.eEcActiveIdc=ERROR_CON_DISABLE;
    if (decoder->Initialize(&params)!=0) throw std::runtime_error("Cannot initialise the preview decoder.");
    unsigned char *planes[3]={}; SBufferInfo info{};
    // Our encoder prepends a fixed 14-byte PES header to each Annex B access unit.
    if (frame.pes.size()<=14 || decoder->DecodeFrameNoDelay(frame.pes.data()+14,
            static_cast<int>(frame.pes.size()-14),planes,&info)!=dsErrorFree ||
            info.iBufferStatus!=1 || info.UsrData.sSystemBuffer.iWidth!=width ||
            info.UsrData.sSystemBuffer.iHeight!=height)
        throw std::runtime_error("Cannot decode the compressed preview.");
    const auto &format=info.UsrData.sSystemBuffer;
    std::vector<uint32_t> rgb(width*height);
    // Match the limited-range BT.601 signal produced by yuv420().
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        int c=planes[0][y*format.iStride[0]+x]-16;
        int d=planes[1][(y/2)*format.iStride[1]+x/2]-128;
        int e=planes[2][(y/2)*format.iStride[1]+x/2]-128;
        unsigned r=clip((298*c+409*e+128)>>8);
        unsigned g=clip((298*c-100*d-208*e+128)>>8);
        unsigned b=clip((298*c+516*d+128)>>8);
        rgb[y*width+x]=(r<<16)|(g<<8)|b;
    }
    return rgb;
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
Bytes pmt(bool teletext=false, unsigned version=0) {
    Bytes b={2,0xb0,18,0,1,0xc1,0,0,0xe1,0,0xf0,0,0x1b,0xe1,0,0xf0,0};
    if (teletext) b.insert(b.end(),{0x06,0xe1,0x01,0xf0,7,0x56,5,'n','l','d',0x09,0x00});
    b[5]|=(version&31)<<1; b[2]=b.size()+1; crc(b); return b;
}
Bytes sdt(const char *call, bool eit=false, unsigned version=0) {
    size_t n=std::strlen(call);
    Bytes b={0x42,0xf0,0,0,1,0xc1,0,0,0,1,0xff,0,1,0xfc,0x80,
             static_cast<unsigned char>(5+2*n),0x48,static_cast<unsigned char>(3+2*n),0x16,
             static_cast<unsigned char>(n)};
    b.insert(b.end(), call, call+n); b.push_back(n); b.insert(b.end(),call,call+n);
    b[5]|=(version&31)<<1; b[13]|=eit?1:0; b[2]=b.size()+1; crc(b); return b;
}
// EN 300 468: DVB UTF-8 selector, short and extended event descriptors.
Bytes dvb_text(const std::string &text) {
    Bytes b;
    if (!text.empty()) { b.push_back(0x15); b.insert(b.end(),text.begin(),text.end()); }
    return b;
}
void sized(Bytes &out, const Bytes &value) {
    out.push_back(static_cast<unsigned char>(value.size()));
    out.insert(out.end(),value.begin(),value.end());
}
unsigned char bcd(unsigned value) { return ((value/10)<<4)|(value%10); }
void utc_time(Bytes &out, int64_t seconds) {
    int64_t mjd=seconds/86400+40587;
    if (seconds<0 || mjd>65535) throw std::runtime_error("System date is outside the DVB date range.");
    out.push_back(mjd>>8); out.push_back(mjd);
    out.push_back(bcd(seconds/3600%24)); out.push_back(bcd(seconds/60%60)); out.push_back(bcd(seconds%60));
}
Bytes eit(const char *call, const DatvSettings &s, int section, int64_t utc, unsigned version) {
    int64_t day=utc/86400;
    Bytes b={0x4e,0xf0,0,0,1,static_cast<unsigned char>(0xc1|((version&31)<<1)),
             static_cast<unsigned char>(section),1,0,1,0,1,1,0x4e};
    if (!section) {
        // One daily station-information window. No following event is scheduled.
        b.push_back(day>>8); b.push_back(day);
        utc_time(b,day*86400); b.insert(b.end(),{0x24,0,0});
        Bytes descriptors, short_event={'n','l','d'};
        std::string title=call;
        if (s.locator[0]) title+=" - "+std::string(s.locator);
        sized(short_event,dvb_text(title)); sized(short_event,dvb_text(s.station.city));
        descriptors.push_back(0x4d); sized(descriptors,short_event);
        // Each extended descriptor holds at most 248 UTF-8 bytes plus selector.
        std::string description;
        if (s.station.operator_name[0]) description="Operator: "+std::string(s.station.operator_name);
        if (s.station.description[0]) {
            if (!description.empty()) description+=" | ";
            description+=s.station.description;
        }
        std::vector<Bytes> chunks;
        for (size_t offset=0;offset<description.size();) {
            size_t end=std::min(offset+248,description.size());
            while (end<description.size() && (static_cast<unsigned char>(description[end])&0xc0)==0x80) --end;
            chunks.push_back(dvb_text(description.substr(offset,end-offset))); offset=end;
        }
        for (size_t i=0;i<chunks.size();++i) {
            Bytes ext={static_cast<unsigned char>((i<<4)|(chunks.size()-1)),'n','l','d',0};
            sized(ext,chunks[i]); descriptors.push_back(0x4e); sized(descriptors,ext);
        }
        b.push_back(0x80|(descriptors.size()>>8)); b.push_back(descriptors.size());
        b.insert(b.end(),descriptors.begin(),descriptors.end());
    }
    size_t length=b.size()+1; b[1]|=length>>8; b[2]=length; crc(b); return b;
}
Bytes tdt(int64_t utc) {
    Bytes b={0x70,0x70,5}; utc_time(b,utc); return b; // TDT has no CRC.
}
int64_t utc_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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
// A receiver can retain SI across Stop/Start. A new content snapshot needs a
// new version even within the same UTC day. Serialise only this small registry;
// each in-flight section remains immutable until its last CRC byte is sent.
unsigned si_version(int kind, const std::string &key, int64_t day=0) {
    struct State { bool seen=false; std::string key; int64_t day=0; unsigned version=0; };
    static State states[3]; static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    State &state=states[kind];
    if (state.seen && (state.key!=key || state.day!=day))
        state.version=(state.version+1)&31;
    state.seen=true; state.key=key; state.day=day;
    return state.version;
}
std::string eit_key(const char *call, const DatvSettings &s) {
    std::string key=call;
    for (const char *field:{s.locator,s.station.city,s.station.operator_name,s.station.description}) {
        key.push_back('\0'); key+=field;
    }
    return key;
}
// EN 300 472: one 184-byte PES per TS; three 46-byte data units.
// Header alone allows page erasure before the body (EN 300 706 Annex B).
unsigned char reverse_bits(unsigned char v) {
    v=((v&0x55)<<1)|((v>>1)&0x55);
    v=((v&0x33)<<2)|((v>>2)&0x33);
    return (v<<4)|(v>>4);
}
unsigned char hamming(int v) {
    static const unsigned char code[]={0x15,0x02,0x49,0x5e,0x64,0x73,0x38,0x2f,
                                      0xd0,0xc7,0x8c,0x9b,0xa1,0xb6,0xfd,0xea};
    return reverse_bits(code[v&15]);
}
unsigned char odd_parity(unsigned char v) {
    unsigned char p=v; p^=p>>4; p^=p>>2; p^=p>>1;
    return reverse_bits(v|((!(p&1))<<7));
}
Bytes teletext_pes(const TeletextSettings &s, int part, uint64_t pts) {
    Bytes b={0,0,1,0xbd,0,178,0x84,0x80,0x24}; timestamp(b,pts);
    b.resize(45,0xff); b.push_back(0x10);
    for (int unit=0;unit<3;++unit) {
        int row=(part && part!=9)?1+(part-1)*3+unit:unit?24:0;
        if (row>23) { b.push_back(0xff); b.push_back(44); b.insert(b.end(),44,0xff); continue; }
        b.insert(b.end(),{0x02,44,0xe0,0xe4}); // Undefined VBI line; first field.
        int address=1+(row<<3);
        b.push_back(hamming(address)); b.push_back(hamming(address>>4));
        if (!row) {
            // Page 100, subcode 0000, C4 erase, C11 serial, English G0.
            for (int nibble:{part==9?15:0,part==9?15:0,0,part==9?0:8,0,0,0,1}) b.push_back(hamming(nibble));
            const char title[]="ATV TELETEXT 100";
            for (int col=0;col<32;++col)
                b.push_back(odd_parity(col<int(sizeof(title)-1)?title[col]:' '));
        } else {
            for (int col=0;col<40;++col)
                b.push_back(odd_parity(s.text[0]?s.text[(row-1)*40+col]:' '));
        }
    }
    return b;
}
// Conservative ES occupancy guard for the extended transport buffer.
// H.264 (08/2024) Annex A, table A-1; H.222.0 (05/2006) 2.14.3.1:
// without NAL HRD, CPB capacity = 1200 * MaxCPB bits.
size_t avc_cpb_capacity(const Frame &frame) {
    const auto &p=frame.pes;
    for (size_t i=14;i+6<p.size();++i) if (!p[i] && !p[i+1] && p[i+2]==1 && (p[i+3]&31)==7) {
        int level=p[i+6];
        if (level==11 && (p[i+5]&0x10)) return 350*150;
        const int levels[]={10,11,12,13,20,21,22,30,31,32,40,41,42,50,51,52,60,61,62};
        const int cpb[]={175,500,1000,2000,2000,4000,4000,10000,14000,20000,25000,62500,62500,135000,240000,240000,240000,480000,800000};
        for (size_t n=0;n<sizeof(levels)/sizeof(*levels);++n) if (level==levels[n]) return size_t(cpb[n])*150;
        break;
    }
    throw std::runtime_error("Cannot determine the AVC decoder buffer for this image.");
}
class Mux {
    const std::vector<Frame> &frames;
    DatvSettings s;
    Bytes tables[5], pes;
    const int pids[5]={0,PMT,0x11,0x12,0x14};
    uint64_t due[5]={}, last_pcr_packet=0, index=0;
    uint64_t completed[6]={}; // PAT, PMT, SDT, EIT present, TDT, EIT following
    bool first_table[5]={true,true,true,true,true};
    size_t table_offset[5]={};
    int cc[5]={}, video_cc=0, eit_section=0;
    int teletext_cc=0, teletext_part=0;
    bool teletext_first=true;
    uint64_t teletext_due=0, teletext_cycle=0, teletext_last_end=0;
    std::string call;
    int64_t epoch;
    size_t offset=0;
    bool repeat;
    size_t cpb_capacity=0, cpb_used=0;
    std::deque<std::pair<uint64_t,size_t>> buffered;
public:
    uint64_t frame=0;
    Mux(const std::vector<Frame> &f, DatvSettings settings, const char *name, bool loop=false, int64_t utc=utc_now())
        : frames(f),s(settings),tables{pat(),pmt(settings.teletext.enabled,si_version(2,settings.teletext.enabled?"TTX":"")),sdt(name,settings.eit_enabled,si_version(false,std::string(name)+(settings.eit_enabled?"+EIT":"")))},call(name),epoch(utc),repeat(loop) {
            if (datv_buffer_ms(s)>1000) cpb_capacity=avc_cpb_capacity(frames.front());
        }
    bool next(unsigned char packet[188]) {
        uint64_t now=byte_time(index*188,CLOCK,s.bitrate);
        uint64_t end=byte_time((index+1)*188,CLOCK,s.bitrate);
        uint64_t release=(frame/s.fps)*CLOCK+(frame%s.fps)*CLOCK/s.fps;
        while (!buffered.empty() && buffered.front().first<=now) {
            cpb_used-=buffered.front().second; buffered.pop_front();
        }
        bool ready=(repeat || frame<frames.size()) && release<=now;
        if (ready && end>release+uint64_t(datv_buffer_ms(s))*90) return false;
        const Frame *f=ready?&frames[frame%frames.size()]:nullptr;
        const uint64_t pcr_packets=std::max(2,s.bitrate*(s.teletext.enabled?80:40)/(1504*1000));
        bool pcr=(index==0 || index-last_pcr_packet>=pcr_packets);
        int table=-1;
        for (int t=0; t<(s.eit_enabled?5:3); ++t)
            if (now>=due[t] && (table<0 || due[t]<due[table])) {
                table=t; // Oldest deadline first, also without EIT: avoid SDT starvation.
            }
        if (s.eit_enabled) {
            const uint64_t limits[]={CLOCK/2,CLOCK/2,2*CLOCK,2*CLOCK,30*CLOCK,2*CLOCK};
            for (int t=0;t<6;++t) if (end-completed[t]>limits[t])
                throw std::runtime_error("EIT and image do not fit the TS schedule. Increase the bitrate or shorten the station description.");
        }
        bool ttx=s.teletext.enabled && now>=teletext_due;
        if (!pcr && ttx && teletext_first) {
            header(packet,0x101,false,15); packet[3]=0x2f; packet[4]=183; packet[5]=0x80;
            teletext_first=false;
        } else if (!pcr && ttx) {
            if (teletext_part && end-teletext_last_end>CLOCK/10)
                throw std::runtime_error("Teletext does not fit the TS schedule. Increase the bitrate.");
            header(packet,0x101,true,teletext_cc); teletext_cc=(teletext_cc+1)&15;
            Bytes data=teletext_pes(s.teletext,teletext_part,end+CLOCK/200);
            std::memcpy(packet+4,data.data(),184);
            if (!teletext_part) teletext_cycle=now;
            teletext_last_end=end;
            if (++teletext_part==10) { teletext_part=0; teletext_due=teletext_cycle+CLOCK*2; }
            else teletext_due=end+CLOCK/50; // >=20 ms erase time; <=100 ms packet gap.
        } else if (pcr || (ready && table<0)) {
            bool start=ready && offset==0;
            bool random=start && f->idr;
            if (start) {
                pes=f->pes;
                Bytes pts; timestamp(pts,release+uint64_t(datv_buffer_ms(s))*90);
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
                if (cpb_capacity) {
                    // Count PES bytes too: slightly conservative vs. ES-only capacity.
                    if (cpb_used+payload>cpb_capacity) return false;
                    if (start) buffered.emplace_back(release+uint64_t(datv_buffer_ms(s))*90,0);
                    buffered.back().second+=payload; cpb_used+=payload;
                }
                std::memcpy(packet+188-payload,pes.data()+offset,payload);
                video_cc=(video_cc+1)&15; offset+=payload;
                if (offset==pes.size()) { ++frame; offset=0; }
            }
        } else if (table>=0) {
            bool start=table_offset[table]==0;
            if (start && table==3) {
                int64_t utc=epoch+now/CLOCK;
                tables[3]=eit(call.c_str(),s,eit_section,utc,si_version(true,eit_key(call.c_str(),s),utc/86400));
            }
            if (start && table==4) tables[4]=tdt(epoch+now/CLOCK);
            header(packet,pids[table],start,cc[table]); cc[table]=(cc[table]+1)&15;
            int payload=4;
            if (first_table[table]) {
                packet[3]|=0x20; packet[4]=1; packet[5]=0x80;
                payload=6; first_table[table]=false;
            }
            if (start) packet[payload++]=0; // pointer_field only on PUSI
            size_t count=std::min<size_t>(188-payload,tables[table].size()-table_offset[table]);
            std::memcpy(packet+payload,tables[table].data()+table_offset[table],count);
            table_offset[table]+=count;
            due[table]=now; // Allow other overdue PIDs between section fragments.
            if (table_offset[table]==tables[table].size()) {
                table_offset[table]=0;
                completed[table==3 && eit_section?5:table]=end;
                if (table==3) eit_section^=1;
                // EIT section gap is >=500 ms from packet end (norm: >=25 ms).
                due[table]=end+(table==4?20*CLOCK:table==3?CLOCK/2:table==2?CLOCK*4/5:CLOCK/5);
                // Compact no-EIT profile with extra scheduling margin at low rates.
                if (!s.eit_enabled) {
                    // Leave room for PCR and SDT while retaining <=300 ms PAT/PMT gaps.
                    uint64_t psi_period=s.bitrate<40000?CLOCK*3/20:CLOCK/5;
                    due[table]=now+(table==2?CLOCK*4/5:psi_period);
                }
            }
        } else header(packet,0x1fff,false,0);
        ++index;
        return true;
    }
};

bool multiplex(const std::vector<Frame> &frames, DatvSettings s, const char *call, Bytes &out) {
    Mux mux(frames,s,call);
    uint64_t count=((uint64_t(s.seconds)*1000+datv_buffer_ms(s))*s.bitrate+1504000*7-1)/(1504000*7)*7;
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
        std::snprintf(error,256,"Invalid image, callsign or output file."); return 0;
    }
    wchar_t wide_call[25]={0};
    for (size_t i=0; call[i]; ++i) wide_call[i]=static_cast<unsigned char>(call[i]);
    if (!valid_call(wide_call)) { std::snprintf(error,256,"Invalid callsign."); return 0; }
    try {
        Bytes yuv=yuv420(rgb,width,height,stride), ts;
        std::vector<Frame> frames;
        for (int qp=24; qp<=48; qp+=4) {
            int largest=0;
            if (!encode(yuv,width,height,s,qp,frames,largest)) {
                std::snprintf(error,256,"OpenH264 could not encode the image."); return 0;
            }
            if (!multiplex(frames,s,call,ts)) continue;
            if (std::fwrite(ts.data(),1,ts.size(),output)!=ts.size() || std::fflush(output)!=0) {
                std::snprintf(error,256,"Failed to write TS. Check free space and write permissions."); return 0;
            }
            if (result) *result={qp,static_cast<int>(frames.size()),largest};
            return 1;
        }
        std::snprintf(error,256,"The image does not fit this TS bitrate and transport buffer. Increase the buffer or use a lower resolution, lower frame rate or longer GOP.");
    } catch (const std::bad_alloc &) { std::snprintf(error,256,"Not enough memory for TS export."); }
    catch (const std::runtime_error &e) { std::snprintf(error,256,"%s",e.what()); }
    return 0;
}

struct DatvStream {
    std::atomic_bool stop{false};
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    DatvUdpStatus status{};
    bool preview_only=false;
    int width=0, height=0;
    std::vector<uint32_t> preview;
    DatvTimeline timeline{};
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
    static void failed(int error=last_error()) { throw std::runtime_error("UDP network error (code "+std::to_string(error)+"). Check the address and network."); }
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
        if (err) throw std::runtime_error("Cannot start Windows networking (code "+std::to_string(err)+").");
        initialized=true;
        timer=timeBeginPeriod(1)==TIMERR_NOERROR;
#endif
        address.sin_family=AF_INET; address.sin_port=htons(s.port);
        if (inet_pton(AF_INET,s.ip,&address.sin_addr)!=1) throw std::runtime_error("Invalid IPv4 address.");
        create_socket();
    }
    bool packet(const unsigned char *data) {
        int sent=send(fd,reinterpret_cast<const char *>(data),1316,0);
        if (sent==1316) return true;
        if (sent>=0) throw std::runtime_error("UDP datagram was not sent in full.");
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
        if (!stream->preview_only) socket.open(settings);
        DatvSettings s=settings.video;
        Bytes yuv=yuv420(rgb.data(),w,h,w*4);
        std::vector<Frame> frames;
        int selected=0;
        uint64_t first_image_packets=0;
        DatvTimeline timeline{};
        std::string planning_error;
        for (int qp=24; qp<=48 && !stream->stop; qp+=4) {
            int largest=0;
            // Consecutive IDR access units must have different idr_pic_id
            // (H.264 7.4.3). Keep two encoder-produced IDRs for GOP=1 so
            // the IDs alternate across template boundaries as well.
            if (!encode(yuv,w,h,s,qp,frames,largest,&stream->stop,std::max(2,s.gop))) {
                if (stream->stop) break;
                throw std::runtime_error("OpenH264 could not encode the image.");
            }
            // Exercise a continuous, repeated closed GOP, including PSI phases.
            // A finite file's trailing second must never hide a bitrate deficit.
            Mux probe(frames,s,call.c_str(),true);
            uint64_t duration=std::max(30,(3*s.gop+s.fps-1)/s.fps)+4*((datv_buffer_ms(s)+999)/1000);
            uint64_t count=duration*s.bitrate/1504;
            bool fits=true; unsigned char packet[188];
            first_image_packets=0;
            uint64_t idr_start=0;
            timeline={}; timeline.qp=qp;
            timeline.idr_bytes=static_cast<int>(frames.front().pes.size())-14;
            timeline.span_ms[0]=1000;
            timeline.span_ms[1]=1000.0*s.gop/s.fps+datv_buffer_ms(s);
            for (uint64_t i=0;i<count;++i) {
                if (stream->stop) { fits=false; break; }
                uint64_t frame=probe.frame;
                try {
                    if (!probe.next(packet)) { fits=false; timeline.failed_frame=frame; break; }
                } catch (const std::runtime_error &e) {
                    if (!stream->preview_only) throw;
                    planning_error=e.what(); fits=false; timeline.failed_frame=frame; break;
                }
                if (!first_image_packets && probe.frame) first_image_packets=i+1;
                if (stream->preview_only) {
                    int pid=((packet[1]&31)<<8)|packet[2];
                    bool payload=(packet[3]&0x10)!=0;
                    int kind=pid==0x1fff?3:(pid==VIDEO && payload?(frames[frame%frames.size()].idr?0:1):2);
                    if (kind==0 && (packet[1]&0x40)) idr_start=i;
                    if (kind==0 && probe.frame!=frame) {
                        double duration_ms=(i+1-idr_start)*1504000.0/s.bitrate;
                        if (!timeline.idr_count || duration_ms<timeline.idr_min_ms) timeline.idr_min_ms=duration_ms;
                        timeline.idr_max_ms=std::max(timeline.idr_max_ms,duration_ms);
                        ++timeline.idr_count;
                    }
                    double ms=i*1504000.0/s.bitrate;
                    for (int view=0;view<2;++view) {
                        double scale=DATV_TIMELINE_BINS/timeline.span_ms[view];
                        double a=ms*scale, b=(ms+1504000.0/s.bitrate)*scale;
                        for (int bin=static_cast<int>(a);bin<DATV_TIMELINE_BINS && bin<b;++bin)
                            timeline.bins[view][bin][kind]+=std::min(b,bin+1.0)-std::max(a,double(bin));
                    }
                    timeline.packets=i+1;
                }
            }
            if (fits) { selected=qp; break; }
            if (!planning_error.empty()) break;
        }
        if (!stream->stop && stream->preview_only) {
            timeline.ready=1; timeline.fits=selected!=0;
            timeline.first_image_ms=first_image_packets*1504000.0/s.bitrate;
            timeline.end_ms=timeline.packets*1504000.0/s.bitrate;
            std::lock_guard<std::mutex> lock(stream->mutex);
            stream->timeline=timeline;
        }
        if (!stream->stop && !planning_error.empty()) throw std::runtime_error(planning_error);
        if (!stream->stop && !selected)
            throw std::runtime_error("The image does not fit the continuous TS. Increase the transport buffer or use a lower resolution, lower frame rate or longer GOP.");
        if (!stream->stop && stream->preview_only) {
            auto pixels=decode_preview(frames.front(),w,h);
            std::lock_guard<std::mutex> lock(stream->mutex);
            if (!stream->stop) { stream->preview=std::move(pixels); stream->status.qp=selected;
                stream->status.first_image_ms=first_image_packets*1504000.0/s.bitrate; }
            stream->status.state=DATV_STOPPED;
            return;
        }
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
                    throw std::runtime_error("UDP stopped: video data exceeds the available bitrate.");
                auto due=origin+std::chrono::nanoseconds(byte_time(n*1316,1000000000,s.bitrate));
                {
                    std::unique_lock<std::mutex> lock(stream->mutex);
                    stream->wake.wait_until(lock,due,[&] { return stream->stop.load(); });
                }
                if (stream->stop) break;
                // Never flush a backlog of datagrams after suspend or overload.
                if (std::chrono::steady_clock::now()-due>interval)
                    throw std::runtime_error("UDP stopped: transmission fell too far behind (sleep or system load). Please restart.");
                bool sent=socket.packet(datagram);
                std::lock_guard<std::mutex> lock(stream->mutex);
                stream->status.state=DATV_RUNNING;
                stream->status.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-origin).count();
                if (sent) ++stream->status.packets;
                else {
                    ++stream->status.refusals;
                    stream->status.last_refusal_seconds=stream->status.seconds;
                }
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

static DatvStream *start_job(const uint32_t *rgb, int width, int height, int stride,
                          const char *call, DatvUdpSettings settings, char error[256], bool preview_only) {
    error[0]=0;
    const char *invalid=preview_only ? datv_validate(settings.video,width,height) : datv_udp_validate(settings,width,height);
    if (invalid) { std::snprintf(error,256,"%s",invalid); return nullptr; }
    if (!rgb || !call || stride<width*4 || stride%4 || std::strlen(call)>24) {
        std::snprintf(error,256,"Invalid image or callsign."); return nullptr;
    }
    wchar_t wide[25]={0};
    for (size_t i=0;call[i];++i) wide[i]=static_cast<unsigned char>(call[i]);
    if (!valid_call(wide)) { std::snprintf(error,256,"Invalid callsign."); return nullptr; }
    try {
        std::vector<uint32_t> copy(width*height);
        for (int y=0;y<height;++y)
            std::memcpy(copy.data()+y*width,reinterpret_cast<const unsigned char *>(rgb)+y*stride,width*4);
        auto stream=std::make_unique<DatvStream>();
        stream->preview_only=preview_only; stream->width=width; stream->height=height;
        stream->worker=std::thread(stream_worker,stream.get(),std::move(copy),width,height,std::string(call),settings);
        return stream.release();
    } catch (const std::exception &e) { std::snprintf(error,256,"DATV preparation failed: %s",e.what()); return nullptr; }
}
DatvStream *datv_udp_start(const uint32_t *rgb, int width, int height, int stride,
                          const char *call, DatvUdpSettings settings, char error[256]) {
    return start_job(rgb,width,height,stride,call,settings,error,false);
}
DatvStream *datv_preview_start(const uint32_t *rgb, int width, int height, int stride,
                              const char *call, DatvSettings settings, char error[256]) {
    DatvUdpSettings udp{}; udp.video=settings;
    return start_job(rgb,width,height,stride,call,udp,error,true);
}
int datv_preview_timeline(DatvStream *job, DatvTimeline *out) {
    if (!job || !out) return 0;
    std::lock_guard<std::mutex> lock(job->mutex);
    if (!job->timeline.ready) return 0;
    *out=job->timeline;
    return 1;
}
int datv_preview_image(DatvStream *job, uint32_t *rgb, int width, int height, int stride) {
    if (!job || !rgb || width!=job->width || height!=job->height || stride<width*4 || stride%4) return 0;
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->preview.empty() || job->status.state!=DATV_STOPPED) return 0;
    for (int y=0;y<height;++y)
        std::memcpy(reinterpret_cast<unsigned char *>(rgb)+y*stride,job->preview.data()+y*width,width*4);
    return 1;
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
