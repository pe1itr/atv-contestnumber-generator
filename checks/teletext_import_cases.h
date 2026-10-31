static void import_fails(TeletextSettings *page,const char *text,int tti) {
    TeletextSettings before=*page; char error[256];
    assert(!teletext_import_data(page,(const unsigned char *)text,strlen(text),tti,error,sizeof(error)));
    assert(error[0] && !memcmp(page,&before,sizeof(before)));
}
static void test_teletext_import(void) {
    char error[256]; TeletextSettings page={0}; page.enabled=1; page.interval=5;
    strcpy(page.header,"KEEP HEADER"); assert(teletext_page_add(&page));
    TeletextSettings extra={0}; assert(!teletext_from_text(&extra,"KEEP EXTRA")); teletext_page_set(&page,1,&extra);
    const unsigned char text[]="\xef\xbb\xbfHELLO\r\nWORLD\r\n";
    assert(teletext_import_data(&page,text,sizeof(text)-1,0,error,sizeof(error)));
    assert(teletext_byte(&page,0)=='H' && teletext_byte(&page,40)=='W');
    assert(page.enabled && page.interval==5 && page.extra_count==1 && !strcmp(page.header,"KEEP HEADER"));
    TeletextSettings fetched; teletext_page_get(&page,1,&fetched);
    for (int i=0;i<TELETEXT_CELLS;++i) assert(teletext_byte(&extra,i)==teletext_byte(&fetched,i));
    const unsigned char tti[]="\xef\xbb\xbf\r\nDE,Description,with commas\nPN,70000\nPS,C040\nSC,0000\nRE,0\nPF,0,0\nCT,10,T\n\nOL,2,\x1b" "MDOUBLE\nOL,1,\x1b" "R\x7f\x1b" "^\x1b" "Z\x1b" "_\nOL,4,\x87" "TEXT\n";
    assert(teletext_import_data(&page,tti,sizeof(tti)-1,1,error,sizeof(error)));
    assert(teletext_byte(&page,0)==18 && teletext_byte(&page,1)==127 && teletext_byte(&page,2)==30);
    assert(teletext_byte(&page,40)==13 && teletext_byte(&page,120)==7);
    assert(page.interval==5 && !strcmp(page.header,"KEEP HEADER") && page.extra_count==1);
    import_fails(&page,"PN,10000\nPN,10000\nOL,1,X",1);
    import_fails(&page,"PN,10001\nOL,1,X",1);
    import_fails(&page,"OL,1,X\nOL,1,Y",1);
    import_fails(&page,"OL,0,HEADER",1);
    import_fails(&page,"OL,24,NAVIGATION",1);
    import_fails(&page,"OL,26,ENHANCEMENT",1);
    import_fails(&page,"OL,1,\x1b",1);
    import_fails(&page,"OL,1,\x1b" "?",1);
    import_fails(&page,"OL,1,\x1b" "@",1); /* Black foreground is not Level 1. */
    import_fails(&page,"OL,1,\x1b" "N",1); /* Double width. */
    import_fails(&page,"OL,1,\x1b" "[",1); /* Alternate charset. */
    import_fails(&page,"OL,23,\x1b" "MABC",1);
    import_fails(&page,"OL,1,\x7f",1); /* G1-only code in G0. */
    import_fails(&page,"OL,1,caf\xc3\xa9",1);
    import_fails(&page,"OL,1,12345678901234567890123456789012345678901",1);
    import_fails(&page,"OL,1,\x1b" "G1234567890123456789012345678901234567890",1);
    import_fails(&page,"OL,1,X\nPS,C041",1);
    import_fails(&page,"OL,1,X\nPS,C0C0",1);
    import_fails(&page,"OL,1,X\nRE,1",1);
    import_fails(&page,"OL,1,X\nPF,1,0",1);
    import_fails(&page,"OL,1,X\nFL,100,100,100,100,100,100",1);
    import_fails(&page,"PN,10000\nDE,No rows",1);
    import_fails(&page,"tab\ttext",0);
    import_fails(&page,"12345678901234567890123456789012345678901",0);
    const unsigned char nul[]={'A',0,'B'}; TeletextSettings before=page;
    assert(!teletext_import_data(&page,nul,sizeof(nul),0,error,sizeof(error)));
    assert(!memcmp(&before,&page,sizeof(page)));
    unsigned char large[TELETEXT_IMPORT_LIMIT+1]; memset(large,' ',sizeof(large));
    assert(!teletext_import_data(&page,large,sizeof(large),0,error,sizeof(error)));
    assert(!memcmp(&before,&page,sizeof(page)));
    assert(!teletext_import_file(&page,"build/nonexistent-page.tti",error,sizeof(error)));
    assert(!teletext_import_file(&page,"build/page.png",error,sizeof(error)));
    assert(!memcmp(&before,&page,sizeof(page)));
    assert(teletext_import_file(&page,"examples/teletext/atv-station.tti",error,sizeof(error)));
    assert(teletext_byte(&page,120)==22 && teletext_byte(&page,121)==127);
    /* UTF-8 file names and case-insensitive extensions on both platforms. */
#ifdef _WIN32
    FILE *file=_wfopen(L"build/teletekst-\u00e9.TtI",L"wb");
#else
    FILE *file=fopen("build/teletekst-\xc3\xa9.TtI","wb");
#endif
    assert(file); assert(fputs("OL,1,FILE IMPORT\n",file)>=0); assert(!fclose(file));
    assert(teletext_import_file(&page,"build/teletekst-\xc3\xa9.TtI",error,sizeof(error)));
    assert(teletext_byte(&page,0)=='F');
#ifdef _WIN32
    assert(!_wremove(L"build/teletekst-\u00e9.TtI"));
#else
    assert(!remove("build/teletekst-\xc3\xa9.TtI"));
#endif
    puts("Import: TXT/TTI, BOM, escaped/8-bit controls, profile checks, metadata retention and atomic failures OK.");
}
