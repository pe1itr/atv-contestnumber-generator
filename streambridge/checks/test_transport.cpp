#include "avc.hpp"
#include "network.hpp"
#include <cassert>
#include <iostream>
using namespace transport;
int main(){
    Bytes input={0,0,0,1,9,0xf0,0,0,1,6,5,4,1,2,3,4,1,1,0x40,0x80,0,0,1,0x65,0x88};
    Bytes filtered=avc_for_transport(input);
    Bytes expected={0,0,0,1,9,0xf0,0,0,0,1,6,1,1,0x40,0x80,0,0,1,0x65,0x88};
    assert(filtered==expected&&first_slice(filtered)==18);
    bool rejected=false;try{avc_for_transport({0,0,1,6,1,100,0x80});}catch(...){rejected=true;}assert(rejected);
    Bytes stamp_bytes;stamp(stamp_bytes,(UINT64_C(1)<<33)+90000);
    Bytes plain;stamp(plain,90000);assert(stamp_bytes==plain);
    std::vector<LocalAddress> addresses={{"wifi0","192.168.1.10"},{"eth0","10.1.1.2"}};
    auto phone=phone_addresses("srt://:4001?mode=listener",addresses);
    assert(phone.find("srt://192.168.1.10:4001?mode=caller")!=std::string::npos);
    auto bound=phone_addresses("srt://10.1.1.2:5000?mode=listener",addresses);
    assert(bound.find("srt://10.1.1.2:5000?mode=caller")!=std::string::npos);
    assert(bound.find("srt://192.168.1.10")==std::string::npos);
    std::deque<Unit> q[2];Unit u;u.bytes=filtered;u.pts=0;u.key=true;u.slice=first_slice(filtered);q[0].push_back(u);
    Mux mux(440310,0,"DATV",0,0);uint8_t p[188];mux.next(p,q);
    assert((p[5]&0x70)==0x70); // PCR, random_access and elementary_stream_priority
    assert(!mux.pending());
    std::cout<<"SEI preservation, IDR priority, timestamp wrap and telephone-address formatting OK\n";
}
