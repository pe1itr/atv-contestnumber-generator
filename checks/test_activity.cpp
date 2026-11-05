#include "core.h"
extern "C" {
#include "pm5544.h"
}
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc,char **argv) {
    if (argc!=2) return 1;
    // Smooth text must have white interiors and intermediate edge coverage.
    std::vector<uint32_t> smooth(320*240);
    assert(activity_render(smooth.data(),320,240,L"PE1ITR",L"JO21QK",3,0,0,0,0));
    int edges=0,white=0;
    for (auto p:smooth) { edges+=p>0 && p<0xffffff; white+=p==0xffffff; }
    assert(edges>100 && white>100);
    assert(std::strstr(activity_font_license,"Copyright (c) 2003 by Bitstream"));
    pm5544_cleanup();
    std::vector<uint32_t> reloaded(320*240);
    assert(activity_render(reloaded.data(),320,240,L"PE1ITR",L"JO21QK",3,0,0,0,0));
    assert(smooth==reloaded);
    // Guarded buffers: all offered sizes, portable calls and twelve-character locators.
    for (int aspect=0;aspect<2;++aspect) for (int i=0;i<RESOLUTION_COUNT;++i) {
        Resolution r=(aspect?resolutions169:resolutions43)[i];
        std::vector<uint32_t> pixels(r.width*r.height+2,0x12345678);
        assert(activity_render(pixels.data()+1,r.width,r.height,L"LX/PE1ITR/P",L"JO21QK86DV12",6,1,0,0,1));
        assert(pixels.front()==0x12345678 && pixels.back()==0x12345678);
        assert(activity_render(pixels.data()+1,r.width,r.height,L"WWWWWWWWWWWWWWWWWWWWWWW1",L"JO21QK86DV12",5,0,0,0,1));
        assert(pixels.front()==0x12345678 && pixels.back()==0x12345678);
    }
    struct Case { int w,h,bitrate,fps,gop,seconds,ebu,strip,motion; };
    const Case cases[]={
        {160,120,30080,2,4,8,0,0,1}, {160,120,30160,4,8,8,0,1,1},
        {160,120,60000,4,4,8,1,0,1}, {160,90,60000,4,4,8,1,0,1},
        {120,68,60000,4,4,8,0,1,1}, {320,180,120000,4,4,8,1,0,1},
        {160,120,120000,4,1,8,1,0,1}, {160,120,60000,2,20,1,0,0,1},
        {160,120,30080,1,1,8,0,0,0}, {160,120,60000,4,4,8,0,1,0},
    };
    int i=0;
    for (auto c:cases) {
        std::vector<uint32_t> pixels(c.w*c.h);
        assert(activity_render(pixels.data(),c.w,c.h,L"LX/PE1ITR/P",L"JO21QK",3,c.ebu,c.strip,c.strip,c.motion));
        char path[1024]; std::snprintf(path,sizeof(path),"%s/activity-%d.ppm",argv[1],i);
        FILE *file=std::fopen(path,"wb"); assert(file);
        std::fprintf(file,"P6\n%d %d\n255\n",c.w,c.h);
        for (auto p:pixels) { std::fputc(p>>16,file); std::fputc(p>>8,file); std::fputc(p,file); }
        assert(!std::fclose(file));
        DatvSettings s=datv_defaults();
        s.bitrate=c.bitrate; s.fps=c.fps; s.gop=c.gop; s.seconds=c.seconds; s.activity_motion=c.motion;
        assert(!datv_validate(s,c.w,c.h));
        if (c.motion) { auto invalid=s; invalid.fps=1; assert(datv_validate(invalid,c.w,c.h)); }
        std::snprintf(path,sizeof(path),"%s/activity-%d.ts",argv[1],i++);
        file=std::fopen(path,"wb"); assert(file);
        DatvResult result{}; char error[256];
        if (!datv_write(file,pixels.data(),c.w,c.h,c.w*4,"PE1ITR",s,&result,error)) {
            std::fprintf(stderr,"%s: %s\n",path,error); std::fclose(file); return 1;
        }
        assert(!std::fclose(file));
        std::printf("%s %d %d %d %d %d %d QP=%d\n",path,c.bitrate,c.fps,c.gop,c.w,c.h,c.seconds,result.qp);
    }
    return 0;
}
