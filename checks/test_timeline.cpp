#include "../src/datv.cpp"
#include <cassert>
#include <cmath>

static void check(int bitrate,int fps,int gop,int width,int height,bool noisy,bool expected,int buffer_ms=0) {
    std::vector<uint32_t> rgb(width*height);
    uint32_t seed=123;
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        seed=seed*1664525+1013904223;
        rgb[y*width+x]=noisy?seed&0xffffff:((x/16+y/16)%2?0xffffff:0);
    }
    DatvSettings s=datv_defaults(); s.bitrate=bitrate; s.fps=fps; s.gop=gop; s.buffer_ms=buffer_ms;
    char error[256];
    DatvStream *job=datv_preview_start(rgb.data(),width,height,width*4,"PE1ITR",s,error);
    assert(job);
    DatvUdpStatus status{};
    for (int i=0;i<6000;++i) {
        datv_udp_status(job,&status);
        if (status.state!=DATV_PREPARING) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    DatvTimeline t{}; assert(datv_preview_timeline(job,&t));
    assert(t.ready && bool(t.fits)==expected && !status.packets);
    assert(status.state==(expected?DATV_STOPPED:DATV_FAILED));
    if (expected) { assert(t.first_image_ms==status.first_image_ms);
        if (noisy && buffer_ms) assert(t.first_image_ms>1000 && t.first_image_ms<=buffer_ms); }
    // Independently count actual video-PES payload and AU completion boundaries.
    Bytes yuv=yuv420(rgb.data(),width,height,width*4); std::vector<Frame> frames;
    int largest=0; assert(encode(yuv,width,height,s,t.qp,frames,largest,nullptr,std::max(2,gop)));
    Mux mux(frames,s,"PE1ITR",true);
    uint64_t first_end=0; size_t first_bytes=0;
    double totals[2][4]={};
    for (uint64_t i=0;i<t.packets;++i) {
        unsigned char p[188]; uint64_t frame=mux.frame; assert(mux.next(p));
        int pid=((p[1]&31)<<8)|p[2], payload=(p[3]&16)!=0;
        int kind=pid==0x1fff?3:(pid==VIDEO && payload?(frame%gop==0?0:1):2);
        double a=i*1504000.0/bitrate,b=(i+1)*1504000.0/bitrate;
        for (int v=0;v<2;++v) totals[v][kind]+=std::max(0.0,std::min(b,t.span_ms[v])-std::min(a,t.span_ms[v]));
        if (pid==VIDEO && payload && !first_end) {
            first_bytes+=188-((p[3]&32)?5+p[4]:4);
            if (first_bytes==frames.front().pes.size()) first_end=i+1;
        }
    }
    assert(t.first_image_ms==first_end*1504000.0/bitrate);
    assert(t.idr_bytes==int(frames.front().pes.size())-14);
    assert((t.idr_count>0)==(first_end>0));
    if (first_end) assert(t.idr_min_ms<=t.first_image_ms && t.idr_max_ms>=t.first_image_ms);
    if (buffer_ms && expected) {
        auto oversized=frames;
        size_t cap=avc_cpb_capacity(oversized.front());
        oversized.front().pes.resize(cap+1024,0xab);
        DatvSettings fast=s; fast.bitrate=2000000;
        Mux guarded(oversized,fast,"PE1ITR",true); bool rejected=false;
        unsigned char packet[188];
        for (int i=0;i<20000 && !rejected;++i) rejected=!guarded.next(packet);
        assert(rejected && guarded.frame==0);
    }
    for (int v=0;v<2;++v) for (int k=0;k<4;++k) {
        double sum=0;
        for (int bin=0;bin<DATV_TIMELINE_BINS;++bin) {
            assert(t.bins[v][bin][k]>=0 && t.bins[v][bin][k]<=1.000001);
            sum+=t.bins[v][bin][k]*t.span_ms[v]/DATV_TIMELINE_BINS;
        }
        assert(std::abs(sum-totals[v][k])<0.001);
    }
    if (!expected) { unsigned char packet[188]; assert(!mux.next(packet)); }
    char text[1500]; datv_timeline_text(&t,s,text,sizeof(text));
    assert(strstr(text,expected?"Past in":"Past niet"));
    printf("Timeline %d bit/s %d fps GOP%d %dx%d: fits=%d, QP%d, first=%.3f ms, raw minimum=%.3f ms\n",bitrate,fps,gop,width,height,t.fits,t.qp,t.first_image_ms,8000.0*t.idr_bytes/bitrate);
    datv_udp_destroy(job);
}
int main() {
    Frame sps{}; sps.pes.resize(14);
    sps.pes.insert(sps.pes.end(),{0,0,1,0x67,66,0x10,11});
    assert(avc_cpb_capacity(sps)==350*150); // Baseline level 1b.
    sps.pes[19]=0; assert(avc_cpb_capacity(sps)==500*150); // Level 1.1.

    check(60000,5,5,320,240,false,true);
    check(120000,2,1,320,240,false,true);
    check(30080,25,1,640,480,true,false);
    check(30080,1,25,640,480,true,true,10000);
    check(30080,25,1,640,480,true,false,10000);
    check(2000000,1,250,160,120,false,true);
    puts("Timeline: mux payload, timing, bin occupancy, long GOP, failure and no network output OK.");
}
