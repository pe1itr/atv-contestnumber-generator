#include "core.h"
extern "C" {
#include "pm5544.h"
}
#include <cstdio>
#include <cstdlib>
#include <cassert>

/* Exercise the same shared renderer/settings/encoder that both GUI dialogs
 * use after selecting DVB-S or DVB-S2, SR 125 and FEC 1/2. */
int main(int argc,char **argv) {
    if (argc!=2) return 1;
    uint32_t pixels[160*120];
    assert(motion_render(pixels,L"PE1ITR",L"1957",L"JO21QK",0));
    const int expected[]={115196,120665,123607};
    const char *names[]={"s125","s2-125-pilots","s2-125-no-pilots"};
    for (int i=0;i<3;++i) {
        DvbSettings radio=dvb_defaults();
        radio.system=i?DVB_S2:DVB_S; radio.symbol_rate=2;
        radio.fec=0; radio.pilots=i==1;
        DatvSettings settings=motion_defaults();
        settings.bitrate=dvb_bitrate(radio); settings.gop=5; settings.seconds=12;
        assert(settings.bitrate==expected[i] && !datv_validate(settings,160,120));
        char path[1024]; std::snprintf(path,sizeof(path),"%s/%s.ts",argv[1],names[i]);
        FILE *file=std::fopen(path,"wb"); assert(file);
        char error[256]; DatvResult result{};
        if (!datv_write(file,pixels,160,120,640,"PE1ITR",settings,&result,error)) {
            std::fprintf(stderr,"%s: %s\n",names[i],error); std::fclose(file); return 1;
        }
        assert(std::fclose(file)==0);
        std::printf("%s: %d bit/s, 10 fps, GOP 5, QP %d, IDR %d bytes, %d frames\n",
                    names[i],settings.bitrate,result.qp,result.largest_idr,result.frames);
    }
    return 0;
}
