#include "mux.hpp"
#include "core.h"
#include <cassert>
#include <cstdio>
#include <set>
// Empty video queues force separate PCR packets: worst case for available SI slots.
int main(){
    std::set<int> rates={DATV_MIN_BITRATE};
    for(int system=0;system<DVB_SYSTEM_COUNT;++system)
        for(int sr=0;sr<DVB_SYMBOL_RATE_COUNT;++sr)for(int fec=0;fec<DVB_FEC_COUNT;++fec)
            for(int pilots=0;pilots<2;++pilots)for(int guard=0;guard<DVB_GUARD_COUNT;++guard)
                for(int bw:dvb_bandwidths){int r=dvb_bitrate({system,sr,fec,pilots,guard,bw});if(r>=DATV_MIN_BITRATE)rates.insert(r);}
    const double limits[]={.5,.5,2,10,2,2,30};double global[7]={};
    for(int rate:rates){
        transport::Mux mux(rate,0,"PE1ITR",0,0,false,true);std::deque<transport::Unit> queues[2];uint8_t packet[188];
        int64_t last[7]={-1,-1,-1,-1,-1,-1,-1};
        uint64_t last_pcr=0;bool have_pcr=false;
        for(int64_t i=0;i<int64_t(rate)*600/1504;++i){
            mux.next(packet,queues);int pid=((packet[1]&31)<<8)|packet[2];
            if((packet[3]&32)&&packet[4]>=7&&(packet[5]&16)){
                uint64_t base=(uint64_t(packet[6])<<25)|(uint64_t(packet[7])<<17)|(uint64_t(packet[8])<<9)|(uint64_t(packet[9])<<1)|(packet[10]>>7);
                uint64_t pcr=base*300+((packet[10]&1)<<8)+packet[11];
                assert(pcr==transport::ticks(i*188+11,27000000,rate));
                assert(!have_pcr||pcr-last_pcr<=2700000);last_pcr=pcr;have_pcr=true;
            }
            int table=pid==0?0:pid==4096?1:pid==17?2:pid==16?3:pid==18?4:pid==20?6:-1;
            if(table<0)continue;
            assert(packet[1]&64);assert(packet[3]&16);
            int pos=packet[3]&32?5+packet[4]:4;const uint8_t *section=packet+pos+1+packet[pos];
            if(table==4){assert(section[6]<2);table+=section[6];}
            // Also bound first appearance, not only steady-state repetitions.
            double gap=(i-(last[table]<0?0:last[table]))*1504.0/rate;
            if(gap>limits[table]+1e-10){std::fprintf(stderr,"rate %d table %d gap %.9f\n",rate,table,gap);return 1;}
            global[table]=std::max(global[table],gap);last[table]=i;
        }
        for(auto index:last)assert(index>=0);
    }
    std::printf("%zu DVB rates x 600 seconds: PCR and individual SI sections OK. Max gaps:",rates.size());
    for(auto gap:global)std::printf(" %.6f",gap);
    std::puts("");
}
