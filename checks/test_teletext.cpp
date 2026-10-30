#include "../src/datv.cpp"
#include <cassert>
int main(int argc,char **argv) {
    assert(argc==2);
    TeletextSettings t{}; t.enabled=1;
    assert(!teletext_from_text(&t,"ATV CONTEST\r\nPagina 100\n\n73 de PE1ITR"));
    char text[TELETEXT_INPUT_SIZE]; teletext_to_text(&t,text,0);
    assert(!std::strcmp(text,"ATV CONTEST\nPagina 100\n\n73 de PE1ITR"));
    TeletextSettings before=t;
    assert(teletext_from_text(&t,"12345678901234567890123456789012345678901"));
    assert(!std::memcmp(&t,&before,sizeof(t)));
    assert(teletext_from_text(&t,"caf\xc3\xa9"));
    assert(teletext_from_text(&t,"a\tb"));
    std::vector<uint32_t> pixels(160*120,0x202060);
    for (int rate:{60000,120000,2000000}) for (int enabled:{0,1}) {
        DatvSettings s=datv_defaults(); s.bitrate=rate; s.seconds=6; s.fps=2; s.gop=2;
        s.teletext=t; s.eit_enabled=enabled;
        std::strcpy(s.station.city,"Eindhoven");
        std::string path=std::string(argv[1])+"-"+std::to_string(rate)+"-"+std::to_string(enabled)+".ts";
        FILE *file=std::fopen(path.c_str(),"wb"); assert(file); char error[256];
        if (!datv_write(file,pixels.data(),160,120,640,"PE1ITR",s,nullptr,error)) {
            std::fprintf(stderr,"%s: %s\n",path.c_str(),error); assert(false);
        }
        assert(!std::fclose(file));
    }
    for (int interval:{2,5,10}) {
        TeletextSettings book{}; book.enabled=1; book.interval=interval;
        for (int index=0;index<4;++index) {
            if (index) assert(teletext_page_add(&book));
            TeletextSettings page{}; TeletextEditor editor;
            teletext_editor_load(&editor,&page);
            std::string label="CAROUSEL PAGE "+std::to_string(100+index);
            std::memcpy(editor.raw,label.data(),label.size());
            const unsigned char codes[]={1,'R',17,63,127,7,'T',3,29,2,'G',28,'K'};
            std::memcpy(editor.raw+80,codes,sizeof(codes));
            // Each G1 sextant bit separately, then all six together.
            editor.raw[120]=22;
            for (int bit=0;bit<6;++bit) editor.raw[121+bit]=(unsigned char)(32|(bit==5?64:1<<bit));
            editor.raw[127]=127;
            editor.cursor=160; assert(!teletext_editor_background(&editor,4));
            assert(!teletext_editor_paste(&editor,"WHITE ON BLUE"));
            editor.cursor=200; teletext_editor_control(&editor,22);
            assert(!teletext_editor_background(&editor,4)); editor.raw[editor.cursor]=127;
            std::strcpy(page.header,label.c_str()); teletext_editor_save(&editor,&page);
            teletext_page_set(&book,index,&page);
        }
        assert(!teletext_validate(&book));
        DatvSettings settings=datv_defaults(); settings.bitrate=60000; settings.seconds=interval*4+2;
        settings.fps=2; settings.gop=2; settings.teletext=book; settings.eit_enabled=1;
        std::string path=std::string(argv[1])+"-grid-60000-"+std::to_string(interval)+".ts";
        FILE *file=std::fopen(path.c_str(),"wb"); assert(file); char error[256];
        if (!datv_write(file,pixels.data(),160,120,640,"PE1ITR",settings,nullptr,error)) {
            std::fprintf(stderr,"%s: %s\n",path.c_str(),error); assert(false);
        }
        assert(!std::fclose(file));
    }
    DatvSettings s=datv_defaults(); s.bitrate=32000; s.teletext=t;
    assert(datv_validate(s,160,120));
    // Full page, including row 23, survives packing.
    std::string full;
    for (int row=0;row<23;++row) { if (row) full+='\n'; full+=std::string(40,'A'+row); }
    assert(!teletext_from_text(&t,full.c_str())); teletext_to_text(&t,text,0);
    assert(full==text);
    // Check stream-removal signalling and version retention.
    unsigned v=si_version(2,"TTX"); assert(si_version(2,"TTX")==v);
    assert(si_version(2,"")==((v+1)&31));
    assert(si_version(2,"TTX")==((v+2)&31));
    std::puts("Teletext: input boundaries, bitrate matrix with/without EIT and PMT version transitions OK.");
}
