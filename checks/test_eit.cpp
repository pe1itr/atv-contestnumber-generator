// Exercise the production encoder/muxer, including an accelerated UTC midnight.
#include "../src/datv.cpp"
#include <cassert>
#include <cstdlib>

static void write_bytes(const std::string &path, const Bytes &bytes) {
    FILE *file=std::fopen(path.c_str(),"wb"); assert(file);
    assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());
    assert(!std::fclose(file));
}
static unsigned first_version(const std::vector<Frame> &frames, DatvSettings s, int pid) {
    Mux mux(frames,s,"PE1ITR/P",true,1789689590);
    for (int i=0;i<s.bitrate*2/1504;++i) {
        unsigned char p[188]; assert(mux.next(p));
        if ((((p[1]&31)<<8)|p[2])!=pid || !(p[1]&64)) continue;
        int pos=(p[3]&32)?5+p[4]:4;
        pos+=1+p[pos]; return (p[pos+5]>>1)&31;
    }
    assert(false); return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);
    std::string prefix=argv[1];
    std::vector<uint32_t> pixels(160*120,0x202060);
    for (int rate:{32000,60000,120000,2000000}) {
        DatvSettings s=datv_defaults(); s.bitrate=rate; s.seconds=4; s.fps=2; s.gop=2;
        s.eit_enabled=1; std::strcpy(s.locator,"JO21QK");
        std::strcpy(s.station.city,"Eindhoven");
        std::strcpy(s.station.operator_name,"René");
        std::strcpy(s.station.description,"ATV-contest; 70 cm; antenne richting zuid.");
        std::string path=prefix+"-"+std::to_string(rate)+".ts";
        FILE *file=std::fopen(path.c_str(),"wb"); assert(file);
        char error[256]; DatvResult result{};
        if (!datv_write(file,pixels.data(),160,120,640,"PE1ITR",s,&result,error)) {
            std::fprintf(stderr,"%d: %s\n",rate,error); assert(false);
        }
        assert(!std::fclose(file));
    }
    // Maximum-length BMP text forces UTF-8-safe descriptor splitting and
    // multi-packet sections. Both bytes and timestamps are checked in Python.
    DatvSettings s=datv_defaults(); s.bitrate=120000; s.fps=2; s.gop=2;
    s.eit_enabled=1; std::strcpy(s.locator,"JO21QK86DV12");
    for (int i=0;i<EIT_CITY_LENGTH;++i) std::strcat(s.station.city,"漢");
    for (int i=0;i<EIT_OPERATOR_LENGTH;++i) std::strcat(s.station.operator_name,"é");
    for (int i=0;i<EIT_DESCRIPTION_LENGTH;++i) std::strcat(s.station.description,"語");
    assert(!datv_validate(s,160,120));
    Bytes yuv=yuv420(pixels.data(),160,120,640);
    std::vector<Frame> frames; int largest;
    assert(encode(yuv,160,120,s,32,frames,largest,nullptr,2));
    // 2026-09-17 23:59:50 UTC: cross midnight while running for 45 seconds.
    Mux mux(frames,s,"PE1ITR/P",true,1789689590);
    Bytes out; uint64_t count=(uint64_t(45)*s.bitrate/10528)*7;
    for (uint64_t i=0;i<count;++i) {
        unsigned char packet[188]; assert(mux.next(packet));
        out.insert(out.end(),packet,packet+188);
    }
    write_bytes(prefix+"-midnight.ts",out);
    unsigned version=first_version(frames,s,0x12);
    assert(first_version(frames,s,0x12)==version);
    std::strcpy(s.station.operator_name,"André");
    assert(first_version(frames,s,0x12)==((version+1)&31));
    version=first_version(frames,s,0x11);
    s.eit_enabled=0;
    assert(first_version(frames,s,0x11)==((version+1)&31));
    s.eit_enabled=1;
    assert(first_version(frames,s,0x11)==((version+2)&31));
    // A large EIT at the minimum bitrate cannot meet the SI deadlines.
    s.bitrate=32000;
    FILE *file=std::tmpfile(); assert(file); char error[256];
    assert(!datv_write(file,pixels.data(),160,120,640,"PE1ITR",s,nullptr,error));
    assert(std::ftell(file)==0 && std::strstr(error,"EIT")); std::fclose(file);
    // Invalid text must fail before any output is published.
    std::strcpy(s.station.operator_name,"\xf0\x9f\x98\x80");
    assert(datv_validate(s,160,120));
    std::puts("EIT: bitrate matrix, long UTF-8, UTC midnight, overhead rejection and invalid text OK.");
}
