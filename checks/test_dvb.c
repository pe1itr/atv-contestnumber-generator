#include "core.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    DvbSettings s=dvb_defaults();
    assert(dvb_bitrate(s)==115196); /* Portsdown: S, 125k, 1/2. */
    s.system=DVB_S2;
    assert(dvb_bitrate(s)==123607); /* Normal frame, no pilots. */
    s.pilots=1; assert(dvb_bitrate(s)==120665);
    s.fec=1; assert(dvb_bitrate(s)==161348);
    s.fec=2; assert(dvb_bitrate(s)==181509);
    s=dvb_defaults(); s.symbol_rate=0;
    const int low_s[]={32254,43006,48382};
    const int low_s2[]={34610,46278,52061};
    for (int fec=0;fec<DVB_FEC_COUNT;++fec) {
        s.fec=fec; s.system=DVB_S; assert(dvb_bitrate(s)==low_s[fec]);
        s.system=DVB_S2; assert(dvb_bitrate(s)==low_s2[fec]);
    }
    s=dvb_defaults(); s.system=DVB_T; s.bandwidth_khz=150;
    const int low_t[]={103676,109775,113101};
    for (int guard=0;guard<DVB_GUARD_COUNT;++guard) {
        s.guard=guard; assert(dvb_bitrate(s)==low_t[guard]);
    }
    s.bandwidth_khz=500; s.fec=2; s.guard=2;
    assert(dvb_bitrate(s)==565508);
    /* Every requested combination fits the advertised transport input range;
     * inactive controls must never alter its calculated capacity. */
    for (int system=0;system<DVB_SYSTEM_COUNT;++system)
    for (int sr=0;sr<DVB_SYMBOL_RATE_COUNT;++sr)
    for (int bw=0;bw<DVB_BANDWIDTH_COUNT;++bw)
    for (int fec=0;fec<DVB_FEC_COUNT;++fec)
    for (int guard=0;guard<DVB_GUARD_COUNT;++guard) {
        s=(DvbSettings){system,sr,fec,0,guard,dvb_bandwidths[bw]};
        int rate=dvb_bitrate(s);
        DatvSettings video={.bitrate=rate,.seconds=1,.fps=1,.gop=1}; assert(!datv_validate(video,160,120));
        s.pilots=1;
        if (system==DVB_S2) assert(dvb_bitrate(s)<rate);
        else assert(dvb_bitrate(s)==rate);
        s.pilots=0;
        if (system==DVB_T) { s.symbol_rate=(sr+1)%DVB_SYMBOL_RATE_COUNT; assert(dvb_bitrate(s)==rate); }
        else { s.bandwidth_khz=dvb_bandwidths[(bw+1)%DVB_BANDWIDTH_COUNT]; s.guard=(guard+1)%DVB_GUARD_COUNT; assert(dvb_bitrate(s)==rate); }
    }
    s=dvb_defaults(); s.system=DVB_SYSTEM_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.fec=-1; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.symbol_rate=DVB_SYMBOL_RATE_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.guard=DVB_GUARD_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.pilots=2; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.bandwidth_khz=151; assert(!dvb_bitrate(s));
    puts("DVB calculator: reference rates, all requested combinations, inactive options and invalid inputs OK.");
    return 0;
}
