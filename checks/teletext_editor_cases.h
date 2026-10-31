/* Shared behavior and persistence tests, also compiled for Windows. */
static void test_teletext_background(void) {
    TeletextEditor e;
    for (int mosaic=0;mosaic<2;++mosaic) for (int color=1;color<=7;++color) {
        TeletextSettings blank={0}; teletext_editor_load(&e,&blank);
        e.raw[0]=(unsigned char)(6+16*mosaic); e.cursor=1;
        e.raw[4]=mosaic?127:'A';
        assert(!teletext_editor_background(&e,color));
        assert(e.cursor==4 && e.raw[1]==color+16*mosaic && e.raw[2]==29 && e.raw[3]==6+16*mosaic);
        TeletextCell cell=teletext_editor_cell(&e,4);
        assert(cell.foreground==6 && cell.background==color && cell.mosaic==mosaic);
        assert(e.raw[4]==(mosaic?127:'A'));
        assert(teletext_editor_cell(&e,1).background==0);
        assert(teletext_editor_cell(&e,2).background==color);
        assert(teletext_editor_cell(&e,40).background==0);
        TeletextEditor before=e; e.cursor=1; before.cursor=1;
        assert(teletext_editor_background(&e,4)); assert(!memcmp(&e,&before,sizeof(e)));
    }
    TeletextSettings blank={0}; teletext_editor_load(&e,&blank);
    for (int p=0;p<TELETEXT_CELLS;++p) if (p%40>=37) {
        e.cursor=p; TeletextEditor before=e;
        assert(teletext_editor_background(&e,4)); assert(!memcmp(&e,&before,sizeof(e)));
    }
    for (int i=0;i<3;++i) {
        e.cursor=0; e.raw[i]='X'; TeletextEditor before=e;
        assert(teletext_editor_background(&e,4)); assert(!memcmp(&e,&before,sizeof(e))); e.raw[i]=' ';
    }
    e.cursor=916; assert(!teletext_editor_background(&e,4)); assert(e.cursor==919);
    assert(!teletext_editor_type(&e,'Z'));
    assert(teletext_editor_cell(&e,919).background==4 && teletext_editor_cell(&e,919).foreground==7);
    teletext_editor_save(&e,&blank); assert(!teletext_validate(&blank));
}
static void test_teletext_editor(void) {
    test_teletext_background();
    TeletextSettings s={0}; TeletextEditor e;
    assert(!teletext_from_text(&s,"OLD PAGE\nKeep this"));
    teletext_editor_load(&e,&s); assert(e.raw[0]=='O');
    e.cursor=40; teletext_editor_control(&e,17); e.raw[41]=32;
    /* Toggle each of the six bits, including bit 7 of the transmitted code. */
    for (int bit=0;bit<6;++bit) {
        teletext_editor_click(&e,16+(bit%2)*8+3,18+(bit/2)*6+2,640,414,0);
        assert(e.cursor==41);
    }
    assert(e.raw[41]==127 && teletext_editor_cell(&e,41).blocks==63);
    assert(teletext_editor_cell(&e,41).foreground==1);
    assert(!teletext_editor_cell(&e,80).mosaic && teletext_editor_cell(&e,80).foreground==7);
    /* Exact non-integral cell/subcell boundaries must hit the visible cell. */
    e.raw[40]=17; e.raw[41]=' ';
    teletext_editor_click(&e,641/40,415/23,641,415,0);
    assert(e.cursor==41 && e.raw[41]==33);
    e.cursor=42; teletext_editor_control(&e,7); assert(!teletext_editor_type(&e,'A'));
    e.cursor=44; teletext_editor_control(&e,3); teletext_editor_control(&e,29);
    assert(teletext_editor_cell(&e,45).background==3); /* set-at */
    teletext_editor_control(&e,2); assert(!teletext_editor_type(&e,'G'));
    assert(teletext_editor_cell(&e,47).foreground==2 && teletext_editor_cell(&e,47).background==3);
    teletext_editor_control(&e,28); assert(teletext_editor_cell(&e,48).background==0);
    e.cursor=80; assert(!teletext_editor_paste(&e,"HELLO\nWORLD"));
    assert(!memcmp(e.raw+80,"HELLO",5) && !memcmp(e.raw+120,"WORLD",5));
    TeletextEditor before=e; assert(teletext_editor_paste(&e,"bad\tinput")); assert(!memcmp(&before,&e,sizeof(e)));
    e.cursor=919; assert(teletext_editor_paste(&e,"AB")); assert(!teletext_editor_type(&e,'Z')); assert(e.cursor==919);
    teletext_editor_save(&e,&s); strcpy(s.header,"TEST HEADER"); assert(!teletext_validate(&s));
    teletext_editor_load(&before,&s); assert(!memcmp(before.raw,e.raw,sizeof(e.raw)));
    s.interval=5;
    for (int i=1;i<4;++i) {
        assert(teletext_page_add(&s)); TeletextSettings page={0};
        assert(!teletext_from_text(&page,i==1?"PAGE 101":i==2?"PAGE 102":"PAGE 103"));
        strcpy(page.header,"HEADER"); teletext_page_set(&s,i,&page);
    }
    assert(!teletext_page_add(&s)); assert(!teletext_validate(&s));
    AppConfig config=config_defaults(),loaded;
    config.teletext=s; assert(config_save("build/teletext-editor.conf",&config));
    assert(config_load("build/teletext-editor.conf",&loaded)==1);
    assert(!memcmp(&config,&loaded,sizeof(config)));
    teletext_page_delete(&s,2); TeletextSettings page;
    teletext_page_get(&s,2,&page); assert(teletext_byte(&page,7)=='3'); assert(!teletext_validate(&s));
    teletext_page_delete(&s,0); assert(teletext_page_count(&s)==3);
    s.extra[0]='X'; assert(teletext_validate(&s));
    s=config.teletext; s.cells[2]='0'; s.cells[3]='0'; assert(teletext_validate(&s));
    s=config.teletext; s.interval=3; assert(teletext_validate(&s));
    s=(TeletextSettings){0}; char import_error[256];
    assert(teletext_import_file(&s,"examples/teletext/atv-station.tti",import_error,sizeof(import_error)));
    teletext_editor_load(&e,&s); assert(!teletext_validate(&s));
    assert(!memcmp(e.raw+14*40+5,"Welcome to our ATV station",26));
    assert(!remove("build/teletext-editor.conf"));
    puts("Teletext editor: mosaic bits, colour boundaries, paste, carousel and config round-trip OK.");
}
