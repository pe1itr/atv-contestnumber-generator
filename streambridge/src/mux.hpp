#pragma once
#include <array>
#include <vector>
#include <deque>
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <ctime>
#include <cstdint>
namespace transport {
using Bytes=std::vector<uint8_t>;
struct Unit { Bytes bytes; int64_t pts; bool key=false; size_t slice=0; };
inline void crc(Bytes &b) {
    uint32_t c=0xffffffff;
    for (auto v:b) { c^=uint32_t(v)<<24; for(int i=0;i<8;++i)c=(c<<1)^((c&0x80000000)?0x04c11db7:0); }
    for(int i=24;i>=0;i-=8)b.push_back(c>>i);
}
inline Bytes section(Bytes b) {
    size_t n=b.size()+1; b[1]|=n>>8; b[2]=n; crc(b); return b;
}
inline void stamp(Bytes &b,uint64_t t) {
    t&=(UINT64_C(1)<<33)-1;
    b.insert(b.end(),{uint8_t(0x21|((t>>29)&14)),uint8_t(t>>22),uint8_t((t>>14)|1),uint8_t(t>>7),uint8_t((t<<1)|1)});
}
inline Bytes pes(Unit u,int stream,uint64_t pts) {
    size_t len=stream==0xe0?0:u.bytes.size()+8;
    if(len>65535)throw std::runtime_error("Audio-access-unit te groot.");
    Bytes b={0,0,1,uint8_t(stream),uint8_t(len>>8),uint8_t(len),0x80,0x80,5};
    stamp(b,pts); b.insert(b.end(),u.bytes.begin(),u.bytes.end());return b;
}
inline uint64_t ticks(uint64_t bytes,uint64_t clock,int bitrate) {
    return bytes/bitrate*8*clock+(bytes%bitrate)*8*clock/bitrate;
}
class Mux {
    int bitrate;
    int64_t audio_release_delay;
    std::array<int,7> pids={0,4096,17,16,18,18,20};
    std::array<Bytes,7> tables;
    std::array<uint64_t,7> due{};
    std::array<int,8192> cc{};
    std::array<bool,8192> seen{};
    Bytes active[2]; size_t offset[2]={}; uint64_t deadline[2]={};size_t slice[2]={}; bool key[2]={};
    uint64_t index=0,last_pcr=0;
    int64_t base;
    time_t utc;
    int version;
    bool camera_profile;
    uint64_t lead;
    std::array<uint64_t,2> last_psi{};
    static uint8_t bcd(int n){return (n/10)*16+n%10;}
public:
    Mux(int rate,int audio_kbps,const std::string &name,int64_t ptsbase,int ver,bool hd=false,bool camera=false)
        :bitrate(rate),audio_release_delay(90000-std::min(9000,1152000/std::max(1,audio_kbps))),base(ptsbase),utc(time(nullptr)),version(ver&31),camera_profile(camera),lead(camera?180000:90000) {
        if(camera && audio_kbps)throw std::runtime_error("Het cameraprofiel ondersteunt alleen video.");
        uint8_t v=0xc1|(version<<1);
        tables[0]=section({0,0xb0,0,0,1,v,0,0,0,0,0xe0,0x10,0,1,0xf0,0});
        Bytes p={2,0xb0,0,0,1,v,0,0,0xe1,0,0xf0,0,0x1b,0xe1,0,0xf0,0};
        if(audio_kbps)p.insert(p.end(),{0x0f,0xe1,1,0xf0,0});
        tables[1]=section(p);
        // ASCII names are enforced in settings. Keep SDT/NIT service types consistent.
        const uint8_t service_type=hd?0x19:0x16;
        size_t n=name.size();
        Bytes s={0x42,0xf0,0,0,1,v,0,0,0,1,0xff,0,1,0xfd,0x80,uint8_t(5+2*n),0x48,uint8_t(3+2*n),service_type,uint8_t(n)};
        s.insert(s.end(),name.begin(),name.end());s.push_back(n);s.insert(s.end(),name.begin(),name.end());tables[2]=section(s);
        // Actual network, service list. RF delivery descriptors belong to modulator.
        tables[3]=section({0x40,0xf0,0,0,1,v,0,0,0xf0,6,0x40,4,'D','A','T','V',0xf0,11,0,1,0,1,0xf0,5,0x41,3,0,1,service_type});
        // Empty present/following sections: no EPG is supplied by this bridge.
        for(int i=0;i<2;++i)tables[4+i]=section({0x4e,0xf0,0,0,1,v,uint8_t(i),1,0,1,0,1,1,0x4e});
    }
    bool pending()const{return offset[0]<active[0].size() || offset[1]<active[1].size();}
    uint64_t packets()const{return index;}
    uint64_t time90()const{return ticks(index*188,90000,bitrate);}
    void next(uint8_t *p,std::deque<Unit> (&queues)[2]) {
        const uint64_t now=time90(),end=ticks((index+1)*188,90000,bitrate);
        std::memset(p,255,188);p[0]=0x47;
        auto head=[&](int pid,bool start,bool payload){
            p[1]=(pid>>8)|(start?64:0);p[2]=pid;
            p[3]=(payload?0x10:0x20)|(payload?cc[pid]:((cc[pid]+15)&15));
            if(payload)cc[pid]=(cc[pid]+1)&15;
        };
        for(int a=0;a<2;++a) {
            if(offset[a]==active[a].size() && !queues[a].empty() && queues[a].front().pts-base+(a?audio_release_delay:0)<=int64_t(now)) {
                Unit u=std::move(queues[a].front());queues[a].pop_front();
                if(u.pts<base)throw std::runtime_error("Invoertijdstempel springt achteruit; herstart de stream.");
                deadline[a]=uint64_t(u.pts-base)+lead;key[a]=u.key;slice[a]=u.slice+14;
                active[a]=pes(std::move(u),a?0xc0:0xe0,deadline[a]);offset[a]=0;
            }
            if(offset[a]<active[a].size() && end>deadline[a])
                throw std::runtime_error("Encoder overschrijdt TS-budget/decodeerdeadline. Verlaag video-/audiobitrate of resolutie.");
        }
        bool pcr=index==0 || index-last_pcr>=uint64_t(std::max(2,bitrate*40/1504000));
        int table=-1;
        for(int i=0;i<7;++i)if(now>=due[i] && (table<0||due[i]<due[table]))table=i;
        // The low-rate camera profile has only ~10 non-PCR packet slots/s.
        // Bound PAT/PMT starvation while keeping the legacy bridge unchanged.
        if(camera_profile)for(int i=0;i<2;++i)
            if(now>=last_psi[i]+31500 && (table>=2 || table<0 || last_psi[i]<last_psi[table]))table=i;
        int a=-1;
        for(int i=0;i<2;++i)if(offset[i]<active[i].size() && (a<0||deadline[i]<deadline[a]))a=i;
        if(pcr) a=offset[0]<active[0].size()?0:-1;
        if(pcr || (a>=0 && table<0)) {
            int pid=a<0?256:256+a;bool start=a>=0 && !offset[a];
            head(pid,start,a>=0);
            bool disc=!seen[pid];seen[pid]=true;
            bool rai=a>=0 && start && key[a==1?1:0];
            bool priority=a==0 && key[0] && offset[0]<=slice[0] && slice[0]<offset[0]+(pcr?176:182);
            size_t size=a<0?0:std::min<size_t>(pcr?176:((disc||rai||priority)?182:184),active[a].size()-offset[a]);
            if(size<184){
                p[3]=(p[3]&15)|(size?0x30:0x20);p[4]=183-size;
                if(p[4])p[5]=(pcr?16:0)|(rai?64:0)|(disc?128:0)|(priority?32:0);
                if(pcr){
                    uint64_t t=ticks(index*188+11,27000000,bitrate),b=t/300;unsigned e=t%300;
                    p[6]=b>>25;p[7]=b>>17;p[8]=b>>9;p[9]=b>>1;p[10]=((b&1)<<7)|0x7e|(e>>8);p[11]=e;
                    last_pcr=index;
                }
            }
            if(size){std::memcpy(p+188-size,active[a].data()+offset[a],size);offset[a]+=size;}
        } else if(table>=0) {
            if(table==6){
                time_t t=utc+now/90000;uint64_t mjd=t/86400+40587;
                if(mjd>65535)throw std::runtime_error("Systeemdatum buiten DVB-bereik.");
                tables[6]={0x70,0x70,5,uint8_t(mjd>>8),uint8_t(mjd),bcd(t/3600%24),bcd(t/60%60),bcd(t%60)};
            }
            int pid=pids[table];head(pid,true,true);int pos=4;
            if(!seen[pid]){p[3]|=0x20;p[4]=1;p[5]=0x80;pos=6;seen[pid]=true;}
            p[pos++]=0;
            if(tables[table].size()>size_t(188-pos))throw std::runtime_error("SI-sectie te groot.");
            std::memcpy(p+pos,tables[table].data(),tables[table].size());
            const int periods[]={18000,18000,72000,360000,90000,90000,1800000};
            const int camera_periods[]={27000,27000,90000,450000,108000,108000,1800000};
            due[table]=now+(camera_profile?camera_periods[table]:periods[table]);
            if(table<2)last_psi[table]=now;
            // EIT sections on same PID separated by at least 25 ms.
            if(table==4)due[5]=now+4500;
        } else {head(8191,false,true);}
        ++index;
    }
};
}
