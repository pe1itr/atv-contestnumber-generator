#include "core.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    int previous=0;
    for (int row=0;row<DVB_SYMBOL_RATE_CHOICE_COUNT;++row) {
        int id=dvb_symbol_rate_order[row], rate=dvb_symbol_rates[id];
        assert(rate>previous && rate!=31 && rate!=32);
        assert(dvb_symbol_rate_row(id)==row);
        previous=rate;
    }
    assert(dvb_symbol_rate_row(9)==-1 && dvb_symbol_rate_row(10)==-1);
    DvbSettings s=dvb_defaults();
    assert(dvb_bitrate(s)==115196); /* Portsdown: S, 125k, 1/2. */
    s.system=DVB_S2;
    assert(dvb_bitrate(s)==123607); /* Normal frame, no pilots. */
    s.pilots=1; assert(dvb_bitrate(s)==120665);
    s.fec=1; assert(dvb_bitrate(s)==161348);
    s.fec=2; assert(dvb_bitrate(s)==181509);
    s=dvb_defaults(); s.symbol_rate=11;
    assert(dvb_symbol_rates[11]==250 && dvb_symbol_rate_row(11)==7);
    assert(dvb_bitrate(s)==230392);
    s.system=DVB_S2; assert(dvb_bitrate(s)==247214);
    s.pilots=1; assert(dvb_bitrate(s)==241331);
    s=dvb_defaults(); s.symbol_rate=0;
    const int low_s[]={32254,43006,48382};
    const int low_s2[]={34610,46278,52061};
    for (int fec=0;fec<3;++fec) {
        s.fec=fec; s.system=DVB_S; assert(dvb_bitrate(s)==low_s[fec]);
        s.system=DVB_S2; assert(dvb_bitrate(s)==low_s2[fec]);
    }
    const int added_rates[]={25,30,33};
    const int added_s[][3]={{23039,30718,34558},{27647,36862,41470},{30411,40549,45617}};
    for (int i=0;i<3;++i) for (int fec=0;fec<3;++fec) {
        s=dvb_defaults(); s.symbol_rate=6+i; s.fec=fec;
        assert(dvb_symbol_rates[s.symbol_rate]==added_rates[i]);
        assert(dvb_bitrate(s)==added_s[i][fec]);
    }
    s=dvb_defaults(); s.system=DVB_T; s.bandwidth_khz=150;
    const int low_t[]={103676,109775,113101};
    for (int guard=0;guard<DVB_GUARD_COUNT;++guard) {
        s.guard=guard; assert(dvb_bitrate(s)==low_t[guard]);
    }
    s.bandwidth_khz=500; s.fec=2; s.guard=2;
    assert(dvb_bitrate(s)==565508);
    /* Combinations below the PCR transport floor must be rejected;
     * inactive controls must never alter its calculated capacity. */
    for (int system=0;system<DVB_SYSTEM_COUNT;++system)
    for (int row=0;row<DVB_SYMBOL_RATE_CHOICE_COUNT;++row)
    for (int bw=0;bw<DVB_BANDWIDTH_COUNT;++bw)
    for (int fec=0;fec<DVB_FEC_COUNT;++fec)
    for (int guard=0;guard<DVB_GUARD_COUNT;++guard) {
        int sr=dvb_symbol_rate_order[row];
        s=(DvbSettings){system,sr,fec,0,guard,dvb_bandwidths[bw]};
        int rate=dvb_bitrate(s);
        DatvSettings video={.bitrate=rate,.seconds=1,.fps=1,.gop=1}; assert((datv_validate(video,160,120)!=NULL)==(rate<30080));
        s.pilots=1;
        if (system==DVB_S2 && rate) assert(dvb_bitrate(s)<rate);
        else assert(dvb_bitrate(s)==rate);
        video.bitrate=dvb_bitrate(s);
        assert((datv_validate(video,160,120)!=NULL)==(video.bitrate<30080));
        s.pilots=0;
        if (system==DVB_T) { s.symbol_rate=dvb_symbol_rate_order[(row+1)%DVB_SYMBOL_RATE_CHOICE_COUNT]; assert(dvb_bitrate(s)==rate); }
        else { s.bandwidth_khz=dvb_bandwidths[(bw+1)%DVB_BANDWIDTH_COUNT]; s.guard=(guard+1)%DVB_GUARD_COUNT; assert(dvb_bitrate(s)==rate); }
    }
    /* Portsdown-specific FEC availability and pilot-sensitive boundaries. */
    const int order[]={5,6,0,7,1,2,3,4,8,9};
    for (int system=0;system<DVB_SYSTEM_COUNT;++system)
    for (int row=0;row<DVB_SYMBOL_RATE_CHOICE_COUNT;++row)
    for (int pilots=0;pilots<2;++pilots) {
        s=dvb_defaults(); s.system=system; s.symbol_rate=dvb_symbol_rate_order[row]; s.pilots=pilots;
        int ids[DVB_FEC_COUNT], count=dvb_fec_choices(s,ids), found=0;
        assert(count>0);
        for (int i=0;i<DVB_FEC_COUNT;++i) {
            s.fec=order[i]; int rate=dvb_bitrate(s);
            if (rate>=DATV_MIN_BITRATE) { assert(found<count && ids[found]==s.fec); ++found; }
        }
        assert(found==count);
    }
    s=dvb_defaults(); s.system=DVB_S2; s.symbol_rate=7; s.fec=7;
    assert(dvb_bitrate(s)==35649); s.pilots=1; assert(dvb_bitrate(s)==34800);
    s.symbol_rate=6; assert(dvb_bitrate(s)<DATV_MIN_BITRATE);
    s.symbol_rate=9; assert(dvb_bitrate(s)==0);
    s.symbol_rate=10; assert(dvb_bitrate(s)==0);
    s.symbol_rate=1; s.fec=5; assert(dvb_bitrate(s)==31586);
    s.system=DVB_S; assert(dvb_bitrate(s)==0);
    s.fec=4; assert(dvb_bitrate(s)>0); s.system=DVB_S2; assert(dvb_bitrate(s)==0);
    s=dvb_defaults(); s.system=DVB_SYSTEM_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.fec=-1; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.symbol_rate=DVB_SYMBOL_RATE_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.guard=DVB_GUARD_COUNT; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.pilots=2; assert(!dvb_bitrate(s));
    s=dvb_defaults(); s.bandwidth_khz=151; assert(!dvb_bitrate(s));
    puts("DVB calculator: reference rates, all requested combinations, inactive options and invalid inputs OK.");
    return 0;
}
