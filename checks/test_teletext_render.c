#include "core.h"
#include <assert.h>
#include <string.h>
#include "teletext_level1_fixture.h"
#include "teletext_import_cases.h"
static unsigned pixels[480*240];
static void pixel_rect(void *ctx,int x,int y,int w,int h,unsigned rgb) {
    (void)ctx;
    assert(x>=0 && y>=0 && x+w<=480 && y+h<=240);
    for (int yy=y;yy<y+h;++yy) for (int xx=x;xx<x+w;++xx) pixels[yy*480+xx]=rgb;
}
static int ink(int row,int col,unsigned rgb) {
    int n=0;
    for (int y=(row+1)*10;y<(row+2)*10;++y)
        for (int x=col*12;x<(col+1)*12;++x) n+=pixels[y*480+x]==rgb;
    return n;
}
static void render(TeletextEditor *e) { teletext_page_plot(e,"LEVEL 1 CHECK",100,480,240,pixel_rect,NULL); }
int main(int argc,char **argv) {
    assert(argc==2);
    test_teletext_import();
    TeletextSettings s={0}; TeletextEditor e;
    teletext_editor_load(&e,&s); e.grid=0; teletext_level1_fixture(&e);
    teletext_editor_save(&e,&s); assert(!teletext_validate(&s));
    render(&e);
    /* T26: hold is set-at; release is set-after; held shape retains separation. */
    assert(ink(6,2,0x00ffff)==120 && ink(6,3,0x00ffff)==120 && ink(6,4,0x00ffff)==120);
    assert(ink(6,5,0x00ffff)==70 && ink(6,6,0x00ffff)==0 && ink(6,7,0x00ffff)==120);
    assert(ink(7,3,0x00ffff)==70 && ink(7,4,0x00ffff)==70 && ink(7,5,0x00ffff)==70);
    assert(ink(7,6,0x00ffff)==120);
    /* Conceal is set-at and colour restores visibility. */
    assert(ink(8,2,0)==120 && ink(8,3,0)==120 && ink(8,5,0xff0000)>0);
    /* DH ignores the whole lower row; normal-size columns keep only background. */
    assert(ink(10,3,0x0000ff)==120 && ink(10,5,0xffffff)>0);
    assert(ink(9,7,0x00ffff)==120 && ink(10,7,0x00ffff)==120);
    assert(ink(10,9,0x0000ff)==120);
    /* Normative held-mosaic resets, including changes of size and mode. */
    assert(teletext_editor_cell(&e,11*40+5).blocks==0);
    assert(teletext_editor_cell(&e,11*40+8).blocks==63); /* reinforcing normal size */
    assert(teletext_editor_cell(&e,11*40+10).blocks==0);
    assert(teletext_editor_cell(&e,11*40+12).blocks==0);
    e.flash_off=1; render(&e);
    assert(ink(8,7,0)==120 && ink(8,9,0xff0000)>0);
    assert(ink(13,3,0)==120 && ink(13,4,0x00ffff)==120); /* steady set-at */
    e.reveal=1; e.flash_off=0; render(&e); assert(ink(8,2,0xffffff)>0);
    FILE *f=fopen(argv[1],"wb"); assert(f); fprintf(f,"P6\n480 240\n255\n");
    for (int i=0;i<480*240;++i) {
        unsigned char rgb[]={(unsigned char)(pixels[i]>>16),(unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
    /* Insertions fail transactionally at forbidden/occupied rows. */
    e.cursor=22*40; TeletextEditor before=e;
    assert(teletext_editor_control(&e,13)); assert(!memcmp(&before,&e,sizeof(e)));
    e.cursor=5*40; before=e;
    assert(teletext_editor_control(&e,13)); assert(!memcmp(&before,&e,sizeof(e)));
    e.cursor=10*40; before=e;
    assert(teletext_editor_type(&e,'X')); assert(!memcmp(&before,&e,sizeof(e)));
    e.raw[22*40]=13; teletext_editor_save(&e,&s); assert(teletext_validate(&s));
    /* DH mosaic lower-half click edits the originating upper cell. */
    e.raw[22*40]=' '; e.cursor=0;
    teletext_page_click(&e,7*12+1,11*10+9,480,240,1);
    assert(e.cursor==9*40+7 && e.raw[e.cursor]==111);
    /* Non-integral scaling: first visible middle/bottom pixel hits that band. */
    teletext_editor_load(&e,&s); memset(e.raw,' ',sizeof(e.raw)); e.raw[0]=22;
    teletext_editor_click(&e,13,6,480,23*21,0);
    assert(e.cursor==1 && e.raw[1]==36);
    teletext_editor_click(&e,13,14,480,23*21,0);
    assert(e.raw[1]==52);
    e.raw[1]=' '; e.raw[2]=13;
    teletext_editor_click(&e,3*12+1,12,480,23*21,0);
    assert(e.cursor==3 && e.raw[3]==36);
    TeletextEditor undo=e; e.reveal=1; e.flash_off=1; e.grid=0; e.raw[3]=' ';
    teletext_editor_undo(&e,&undo);
    assert(e.raw[3]==36 && e.reveal && e.flash_off && !e.grid);
    memset(e.raw,' ',sizeof(e.raw));
    const unsigned char concealed_hold[]={22,127,30,24,28,17,127};
    memcpy(e.raw,concealed_hold,sizeof(concealed_hold));
    e.reveal=0; e.flash_off=0; render(&e);
    assert(ink(0,2,0x00ffff)==120 && ink(0,3,0)==120 && ink(0,5,0)==120);
    assert(ink(0,6,0xff0000)==120); /* Colour cancels conceal set-after. */
    memset(e.raw,' ',sizeof(e.raw)); e.raw[0]=22; e.cursor=0;
    teletext_view_click(&e,97,21,800,480,0); assert(e.cursor==1 && e.raw[1]==33);
    teletext_view_click(&e,0,21,800,480,0); assert(e.cursor==1 && e.raw[1]==33);
    teletext_view_click(&e,13,236,480,800,0); assert(e.cursor==1 && e.raw[1]==' ');
    teletext_view_click(&e,13,0,480,800,0); assert(e.cursor==1);
    puts("Level 1: spacing, hold, separated, conceal, flash, double height, header and boundaries OK.");
}
