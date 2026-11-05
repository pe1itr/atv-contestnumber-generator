#include "core.h"
#include "datv.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

void datv_timeline_text(const DatvTimeline *t, DatvSettings s, char *out, size_t size) {
    char first[180], result[220], contest[220], idr[220];
    if (t->idr_count) snprintf(idr,sizeof(idr),"Self-contained image (IDR): %.1f - %.1f ms including TS overhead (%d images measured).",
        t->idr_min_ms,t->idr_max_ms,t->idr_count);
    else snprintf(idr,sizeof(idr),"Self-contained image (IDR): transfer incomplete.");
    snprintf(contest,sizeof(contest),"Contest target < 1000 ms: %s. Operator's choice.",
        !t->idr_count?"no complete measurement":t->idr_max_ms<1000?"measured IDRs below target":"measured IDR at/above target");
    if (t->first_image_ms>0)
        snprintf(first,sizeof(first),"First complete image: %.1f ms from TS start.",t->first_image_ms);
    else snprintf(first,sizeof(first),"First complete image: not completed before scheduling stopped.");
    if (t->fits) snprintf(result,sizeof(result),"Fits the continuous stream schedule; QP %d. First presentation time: %d ms.",t->qp,datv_buffer_ms(s));
    else snprintf(result,sizeof(result),"Does not fit: scheduling stops at frame %llu, after %.1f ms; diagnostic QP %d.",
        (unsigned long long)t->failed_frame+1,t->end_ms,t->qp);
    snprintf(out,size,"%s\n%s\n%s\n%s\nFrame period: %.1f ms; IDR interval: %.1f ms (GOP %d). Waiting for an IDR is additional.\nFirst IDR: %d bytes H.264; at least %.1f ms without TS overhead.\nTS time; network and decoder latency excluded.",
        idr,contest,first,result,1000.0/s.fps,1000.0*s.gop/s.fps,s.gop,t->idr_bytes,8000.0*t->idr_bytes/s.bitrate);
}

void datv_timeline_plot(const DatvTimeline *t, DatvSettings s, int width, int height,
                        DatvPlotRect rect, DatvPlotText text, void *ctx) {
    const unsigned colors[]={0x2676c8,0x32a476,0xda9b32,0xc4cad1};
    const char *labels[]={"IDR: complete image","P: predicted frame","TS-overhead*","Null: unused capacity"};
    rect(ctx,0,0,width,height,0xffffff);
    if (!t->ready || width<200 || height<130) { text(ctx,8,8,"Calculating transport schedule..."); return; }
    int left=8, plot=width-16;
    for (int k=0;k<4;++k) {
        int x=left+k*plot/4;
        rect(ctx,x,4,10,10,colors[k]); text(ctx,x+14,1,labels[k]);
    }
    for (int view=0;view<2;++view) {
        int y=24+view*65;
        char label[180];
        snprintf(label,sizeof(label),"%s: 0 - %.1f ms",view?"One GOP + buffer":"First second",t->span_ms[view]);
        text(ctx,left,y,label);
        rect(ctx,left,y+19,plot,24,0xeceff2);
        for (int i=0;i<DATV_TIMELINE_BINS;++i) {
            int x=left+i*plot/DATV_TIMELINE_BINS;
            int w=left+(i+1)*plot/DATV_TIMELINE_BINS-x;
            double sum=0;
            for (int k=0;k<4;++k) {
                int bottom=(int)(sum*24+0.5);
                sum+=t->bins[view][i][k];
                int top=(int)(sum*24+0.5);
                if (top>24) top=24;
                if (top>bottom) rect(ctx,x,y+43-top,w,top-bottom,colors[k]);
            }
        }
        /* Frame boundaries refer to encoder release times, not presentation. */
        int frames=(int)(t->span_ms[view]*s.fps/1000);
        int step=frames>40?(frames+39)/40:1;
        for (int f=0;f<=frames;f+=step) {
            int x=left+(int)(1000.0*f/s.fps/t->span_ms[view]*(plot-1));
            rect(ctx,x,y+44,1,4,0x555555);
        }
        for (int tick=0;tick<=4;++tick) {
            char value[40]; snprintf(value,sizeof(value),"%.0f",t->span_ms[view]*tick/4);
            int x=left+tick*(plot-1)/4;
            if (tick==4) x-=(int)strlen(value)*8;
            text(ctx,x,y+48,value);
        }
        if (t->first_image_ms>0 && t->first_image_ms<=t->span_ms[view]) {
            int x=left+(int)(t->first_image_ms/t->span_ms[view]*(plot-1));
            rect(ctx,x,y+18,2,26,0x111111);
        }
        int marker=left+(int)(1000.0/t->span_ms[view]*(plot-1));
        for (int dy=0;dy<26;dy+=6) rect(ctx,marker,y+18+dy,2,3,0x8a3db5);
        if (!t->fits && t->end_ms<=t->span_ms[view]) {
            int x=left+(int)(t->end_ms/t->span_ms[view]*(plot-1));
            rect(ctx,x,y+18,2,26,0xc02020);
        }
    }
    text(ctx,left,155,"Purple: contest 1000 ms; black: image complete; red: scheduling stops. Ticks: frame periods.");
    text(ctx,left,173,"* Other: SI, teletext and reserved audio. Video includes TS headers; colours show time allocation.");
}

const wchar_t *const image_modes[IMAGE_MODE_COUNT] = {L"Contest", L"PM5544", L"FUBK", L"PM5644", L"Activity"};
/* Display order is independent of the persistent configuration IDs. */
const int image_mode_order[IMAGE_MODE_COUNT] = {IMAGE_CONTEST, IMAGE_PM5544, IMAGE_PM5644, IMAGE_FUBK, IMAGE_ACTIVITY};
int image_mode_row(int mode) {
    for (int row=0; row<IMAGE_MODE_COUNT; ++row)
        if (image_mode_order[row]==mode) return row;
    return 0;
}
int valid_fubk_locator(const wchar_t *s) { return valid_locator(s) && wcslen(s)>=6; }

const wchar_t *const dvb_system_names[DVB_SYSTEM_COUNT]={L"DVB-S",L"DVB-S2",L"DVB-T"};
/* Indices are stored in configuration files: append new rates.
 * IDs 9/10 belonged to retired 31/32 ksym/s; never reuse them. */
const int dvb_symbol_rates[DVB_SYMBOL_RATE_COUNT]={35,66,125,150,333,500,25,30,33,0,0,250};
/* Sorted display order keeps the persistent IDs of remaining rates intact. */
const int dvb_symbol_rate_order[DVB_SYMBOL_RATE_CHOICE_COUNT]={6,7,8,0,1,2,3,11,4,5};
int dvb_symbol_rate_row(int id) {
    for (int row=0;row<DVB_SYMBOL_RATE_CHOICE_COUNT;++row)
        if (dvb_symbol_rate_order[row]==id) return row;
    return -1;
}
const int dvb_bandwidths[DVB_BANDWIDTH_COUNT]={35,40,150,250,333,500};
/* Preserve existing FEC IDs 0..2. The added choices match Portsdown menus. */
const wchar_t *const dvb_fec_names[DVB_FEC_COUNT]={L"1/2",L"2/3",L"3/4",L"5/6",L"7/8",L"1/4",L"1/3",L"3/5",L"8/9",L"9/10"};
static const int fec_num[DVB_FEC_COUNT]={1,2,3,5,7,1,1,3,8,9};
static const int fec_den[DVB_FEC_COUNT]={2,3,4,6,8,4,3,5,9,10};
static const int fec_kbch[DVB_FEC_COUNT]={32208,43040,48408,53840,0,16008,21408,38688,57472,58192};
const wchar_t *const dvb_guard_names[DVB_GUARD_COUNT]={L"1/8",L"1/16",L"1/32"};
DvbSettings dvb_defaults(void) { return (DvbSettings){DVB_S,2,0,0,2,500}; }
DvbSettings motion_dvb_defaults(void) { return (DvbSettings){DVB_T,2,0,0,2,40}; }
DatvSettings motion_defaults(void) {
    DatvSettings s=datv_defaults();
    s.bitrate=dvb_bitrate(motion_dvb_defaults()); s.seconds=4;
    s.fps=10; s.gop=20; s.motion=1;
    return s;
}
void activity_marker(int width, int height, unsigned frame, unsigned cycle,
                     int *left, int *top, int *size) {
    *size=height*4/100;
    if (*size<2) *size=2;
    *top=height*84/100;
    unsigned half=cycle/2;
    unsigned phase=half ? frame%cycle : 0;
    if (phase>half) phase=cycle-phase;
    *left=width/20+(half ? (int)((width-width/10-*size)*phase/half) : 0);
}
void activity_luma(unsigned char *yuv, int width, int height, unsigned frame, unsigned cycle) {
    int left,top,size;
    activity_marker(width,height,frame,cycle,&left,&top,&size);
    int y0=(height*82/100)&~1, y1=((height*90/100)+1)&~1;
    for (int y=y0;y<y1;++y) for (int x=0;x<width;++x)
        yuv[y*width+x]=(y>=top && y<top+size && x>=left && x<left+size)?235:16;
    for (int plane=0;plane<2;++plane)
        for (int y=y0/2;y<y1/2;++y)
            memset(yuv+width*height+plane*width*height/4+y*width/2,128,width/2);
}
void motion_luma(unsigned char *y, unsigned frame) {
    unsigned phase=frame%40;
    /* A large block travels out and back on two lanes. Halfway through it
     * reveals a smiling face and winks. Every phase has a distinct position. */
    int x=8+6*(phase<20?(int)phase:39-(int)phase);
    int top=phase<20?82:94;
    memset(y+160*80,16,160*40);
    for (int dy=0;dy<24;++dy) for (int dx=0;dx<24;++dx) {
        int white=1;
        if (phase>=8 && phase<32) {
            int eye=(dx>=5 && dx<9 && dy>=6 && dy<10) ||
                (dx>=15 && dx<19 && dy>=6 && dy<(phase>=14 && phase<18?8:10));
            int smile=(dy>=17 && dy<19 && dx>=7 && dx<17) ||
                (dy>=14 && dy<17 && ((dx>=5 && dx<7)||(dx>=17 && dx<19)));
            white=!(eye||smile);
        }
        y[(top+dy)*160+x+dx]=white?235:16;
    }
}
int motion_options(int count, const char *const *values, DatvSettings *s,
                   wchar_t call[25], wchar_t code[5], wchar_t locator[13]) {
    if (count<3 || count>5) return 0;
    wchar_t *out[]={call,code,locator};
    const size_t max[]={24,4,12};
    for (int f=0;f<3;++f) {
        size_t n=strlen(values[f]); if (n>max[f]) return 0;
        for (size_t i=0;i<=n;++i) {
            unsigned char c=(unsigned char)values[f][i];
            out[f][i]=c>='a' && c<='z'?c-'a'+'A':c;
        }
    }
    if (!valid_call(call) || !valid_code(code) || !valid_locator(locator)) return 0;
    *s=motion_defaults();
    for (int i=3;i<count;++i) {
        char *end; errno=0; long value=strtol(values[i],&end,10);
        if (errno || !*values[i] || *end || value<1 || value>60) return 0;
        if (i==3) s->seconds=(int)value; else s->gop=(int)value;
    }
    return datv_validate(*s,160,120)==NULL;
}
int dvb_fec_choices(DvbSettings s, int ids[DVB_FEC_COUNT]) {
    const int order[]={5,6,0,7,1,2,3,4,8,9};
    int count=0;
    for (int i=0;i<DVB_FEC_COUNT;++i) {
        s.fec=order[i]; int rate=dvb_bitrate(s);
        if (rate>=DATV_MIN_BITRATE && rate<=2000000) ids[count++]=s.fec;
    }
    return count;
}
int dvb_bitrate(DvbSettings s) {
    if (s.system<0 || s.system>=DVB_SYSTEM_COUNT || s.symbol_rate<0 ||
        s.symbol_rate>=DVB_SYMBOL_RATE_COUNT || !dvb_symbol_rates[s.symbol_rate] || s.fec<0 || s.fec>=DVB_FEC_COUNT ||
        s.pilots<0 || s.pilots>1 || s.guard<0 || s.guard>=DVB_GUARD_COUNT ||
        s.bandwidth_khz<1 || s.bandwidth_khz>8000) return 0;
    int bandwidth_ok=0;
    for (int i=0;i<DVB_BANDWIDTH_COUNT;++i) bandwidth_ok|=s.bandwidth_khz==dvb_bandwidths[i];
    if (!bandwidth_ok) return 0;
    if (s.system==DVB_S2 ? fec_kbch[s.fec]==0 : s.fec>4) return 0;
    int64_t rate=(int64_t)dvb_symbol_rates[s.symbol_rate]*1000;
    int64_t top, bottom;
    if (s.system==DVB_S) {
        /* EN 300 421: QPSK, convolutional code and RS(204,188). */
        top=rate*2*fec_num[s.fec]*188; bottom=fec_den[s.fec]*204;
    } else if (s.system==DVB_S2) {
        /* EN 302 307-1: normal 64800-bit FECFRAME, 80-bit BBHEADER,
         * 90-symbol PLHEADER and 22 pilot blocks of 36 symbols for QPSK.
         * Full DATAFIELD, CCM, no ISSY or null-packet deletion. */
        top=rate*(fec_kbch[s.fec]-80); bottom=32400+90+s.pilots*22*36;
    } else {
        /* EN 300 744: 1512 data carriers, Tu=2048*7/(8*B), RS188/204.
         * 423/544 includes carriers, useful-symbol time and outer FEC. */
        const int guard[]={8,16,32};
        top=(int64_t)s.bandwidth_khz*1000*423*2*fec_num[s.fec]*guard[s.guard];
        bottom=(int64_t)544*fec_den[s.fec]*(guard[s.guard]+1);
    }
    return (int)(top/bottom);
}

int dvb_audio_allowed(DvbSettings s) {
    if (s.system==DVB_T) return s.bandwidth_khz==333 || s.bandwidth_khz==500;
    return (s.system==DVB_S || s.system==DVB_S2) && s.symbol_rate>=0 &&
        s.symbol_rate<DVB_SYMBOL_RATE_COUNT &&
        (dvb_symbol_rates[s.symbol_rate]==333 || dvb_symbol_rates[s.symbol_rate]==500);
}
int datv_audio_reservation(int bitrate) { return bitrate==48000?141000:bitrate==96000?211500:0; }
int datv_buffer_ms(DatvSettings s) { return s.buffer_ms?s.buffer_ms:1000; }
DatvSettings datv_defaults(void) { return (DatvSettings){.bitrate=120000, .seconds=10, .fps=5, .gop=5}; }
DatvUdpSettings datv_udp_defaults(void) {
    DatvUdpSettings s={.port=10000,.video={.bitrate=120000,.seconds=10,.fps=10,.gop=4}};
    return s;
}
void datv_udp_status_text(DatvUdpSettings s, DatvUdpStatus status, char *text, size_t size) {
    if (status.state==DATV_FAILED) snprintf(text,size,"%s",status.error);
    else if (status.state==DATV_PREPARING) snprintf(text,size,"Preparing image and checking bitrate...");
    else if (status.state==DATV_STOPPED) snprintf(text,size,"UDP stopped (%llu packets).",(unsigned long long)status.packets);
    else snprintf(text,size,"UDP output active to %s:%d\n%d bit/s, %llu packets of 1316 bytes, QP %d.",
        s.ip,s.port,s.video.bitrate,(unsigned long long)status.packets,status.qp);
    if (size && status.state==DATV_RUNNING && status.refusals &&
        status.seconds-status.last_refusal_seconds<3.0) {
        size_t used=strlen(text);
        if (used<size) snprintf(text+used,size-used,
            "\nThe receiver just reported a closed UDP port.\nTransmission continues; check that IPTS is active.");
    }
}
const char *datv_udp_validate(DatvUdpSettings s, int width, int height) {
    if (s.port<1 || s.port>65535) return "Port must be between 1 and 65535.";
    /* Numeric unicast IPv4 only; no DNS lookup, ambiguous octal or broadcast. */
    int octets[4]={0}; size_t pos=0;
    for (int part=0; part<4; ++part) {
        size_t start=pos;
        while (pos<sizeof(s.ip) && s.ip[pos]>='0' && s.ip[pos]<='9') {
            if (pos-start>=3) return "Enter a valid unicast IPv4 address.";
            octets[part]=octets[part]*10+s.ip[pos++]-'0';
        }
        if (pos==start || octets[part]>255 || (pos-start>1 && s.ip[start]=='0') ||
            pos>=sizeof(s.ip) || s.ip[pos]!=(part==3?'\0':'.'))
            return "Enter a valid unicast IPv4 address.";
        ++pos;
    }
    if (octets[0]==0 || octets[0]>=224) return "Use a unicast IPv4 address (not multicast or broadcast).";
    s.video.seconds=10; /* A live stream has no preset duration. */
    return datv_validate(s.video,width,height);
}
int datv_test_options(int count, const char *const *values, DatvSettings *s, int *w, int *h) {
    *s=datv_defaults(); *w=320; *h=240;
    if (count<0 || count>7) return 0;
    int *fields[]={&s->bitrate,&s->seconds,&s->fps,&s->gop,w,h,&s->buffer_ms};
    for (int i=0; i<count; ++i) {
        char *end; errno=0;
        long n=strtol(values[i],&end,10);
        if (errno || !*values[i] || *end || n<1 || n>INT_MAX) return 0;
        *fields[i]=(int)n;
    }
    return datv_validate(*s,*w,*h)==NULL;
}
/* Supported Latin G0 characters with English national option (C12-C14=000).
 * Ambiguous ASCII/national-option positions are not accepted as typed text. */
static int teletext_character(unsigned char c) {
    return c>=32 && c<=126 && !strchr("#@[\\]^_`{|}~",c);
}
static int teletext_hex(unsigned char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
unsigned char teletext_byte(const TeletextSettings *s, int index) {
    if (index<0 || index>=TELETEXT_CELLS) return ' ';
    if (s->cells[0]) {
        int hi=teletext_hex(s->cells[index*2]),lo=teletext_hex(s->cells[index*2+1]);
        return hi<0 || lo<0?' ':(unsigned char)(hi*16+lo);
    }
    return s->text[0]?(unsigned char)s->text[index]:' ';
}
int teletext_interval(const TeletextSettings *s) { return s->interval?s->interval:2; }
int teletext_page_count(const TeletextSettings *s) { return 1+s->extra_count; }
void teletext_page_get(const TeletextSettings *s,int index,TeletextSettings *page) {
    memset(page,0,sizeof(*page)); page->enabled=s->enabled;
    if (!index) {
        memcpy(page->text,s->text,sizeof(page->text));
        memcpy(page->cells,s->cells,sizeof(page->cells));
        memcpy(page->header,s->header,sizeof(page->header));
    } else if (index>0 && index<=s->extra_count && index<TELETEXT_MAX_PAGES) {
        const char *p=s->extra+(index-1)*TELETEXT_EXTRA_SIZE;
        for (int i=0;i<32;++i) page->header[i]=(char)(teletext_hex(p[i*2])*16+teletext_hex(p[i*2+1]));
        memcpy(page->cells,p+64,TELETEXT_CELLS*2);
    }
}
void teletext_page_set(TeletextSettings *s,int index,const TeletextSettings *page) {
    if (!index) {
        memcpy(s->text,page->text,sizeof(s->text));
        memcpy(s->cells,page->cells,sizeof(s->cells));
        memcpy(s->header,page->header,sizeof(s->header));
    } else if (index>0 && index<=s->extra_count && index<TELETEXT_MAX_PAGES) {
        const char hex[]="0123456789ABCDEF";
        char *p=s->extra+(index-1)*TELETEXT_EXTRA_SIZE;
        size_t len=strlen(page->header);
        for (int i=0;i<32+TELETEXT_CELLS;++i) {
            unsigned c=i<32?(i<(int)len?(unsigned char)page->header[i]:0):teletext_byte(page,i-32);
            p[i*2]=hex[c>>4]; p[i*2+1]=hex[c&15];
        }
    }
}
int teletext_page_add(TeletextSettings *s) {
    if (s->extra_count>=TELETEXT_MAX_PAGES-1) return 0;
    TeletextSettings blank={0}; ++s->extra_count;
    teletext_page_set(s,s->extra_count,&blank);
    s->extra[s->extra_count*TELETEXT_EXTRA_SIZE]=0; return 1;
}
void teletext_page_delete(TeletextSettings *s,int index) {
    if (index<=0 || index>s->extra_count) return;
    memmove(s->extra+(index-1)*TELETEXT_EXTRA_SIZE,s->extra+index*TELETEXT_EXTRA_SIZE,
            (size_t)(s->extra_count-index)*TELETEXT_EXTRA_SIZE);
    --s->extra_count;
    memset(s->extra+s->extra_count*TELETEXT_EXTRA_SIZE,0,sizeof(s->extra)-(size_t)s->extra_count*TELETEXT_EXTRA_SIZE);
}
static int teletext_control(unsigned c) {
    return (c>=1 && c<=9) || c==12 || c==13 || (c>=17 && c<=26) || (c>=28 && c<=31);
}
const char *teletext_validate(const TeletextSettings *s) {
    if (s->interval && s->interval!=2 && s->interval!=5 && s->interval!=10) return "Invalid teletext repeat interval.";
    if (s->enabled!=0 && s->enabled!=1) return "Invalid teletext option.";
    size_t n=0;
    while (n<sizeof(s->text) && s->text[n]) {
        if (!teletext_character((unsigned char)s->text[n++]))
            return "Use unaccented letters, digits and basic punctuation for teletext.";
    }
    if (n!=0 && n!=TELETEXT_CELLS) return "Invalid teletext page.";
    n=0;
    while (n<sizeof(s->header) && s->header[n]) {
        if (!teletext_character((unsigned char)s->header[n++])) return "Use basic unaccented text in the header.";
    }
    if (n==sizeof(s->header)) return "Use up to 32 header characters.";
    if (s->cells[0]) {
        for (n=0;n<TELETEXT_CELLS*2;++n)
            if (teletext_hex((unsigned char)s->cells[n])<0) return "Invalid teletext grid encoding.";
        if (s->cells[n]) return "Invalid teletext grid length.";
        int mosaic=0;
        for (int i=0;i<TELETEXT_CELLS;++i) {
            if (i%40==0) mosaic=0;
            unsigned c=teletext_byte(s,i);
            if (c==13 && i/40==22) return "Double height is not allowed on the last body row.";
            if (teletext_control(c)) {
                if (c<8) mosaic=0;
                else if (c>=17 && c<=23) mosaic=1;
            } else if (!(mosaic && ((c>=32 && c<=63) || (c>=96 && c<=127))) && !teletext_character(c))
                return "Unsupported character in text mode. Restore Mosaic colour or erase that cell.";
        }
    }
    if (s->extra_count<0 || s->extra_count>=TELETEXT_MAX_PAGES) return "Invalid teletext page count.";
    size_t extra_length=(size_t)s->extra_count*TELETEXT_EXTRA_SIZE;
    for (n=0;n<extra_length;++n) if (teletext_hex((unsigned char)s->extra[n])<0)
        return "Invalid teletext carousel encoding.";
    if (s->extra[extra_length]) return "Invalid teletext carousel length.";
    for (int i=1;i<=s->extra_count;++i) {
        TeletextSettings page;
        teletext_page_get(s,i,&page);
        const char *error=teletext_validate(&page); if (error) return error;
    }
    return NULL;
}

const char *const teletext_color_names[7]={"Red","Green","Yellow","Blue","Magenta","Cyan","White"};
const unsigned teletext_colors[8]={0x000000,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff};
const TeletextAttribute teletext_attributes[TELETEXT_ATTRIBUTE_COUNT]={
    {"Normal height",12},{"Double height",13},{"Flash",8},{"Steady",9},
    {"Conceal",24},{"Contiguous mosaics",25},{"Separated mosaics",26},
    {"Hold mosaics",30},{"Release mosaics",31}
};
void teletext_editor_load(TeletextEditor *e, const TeletextSettings *s) {
    memset(e,0,sizeof(*e)); e->grid=1;
    for (int i=0;i<TELETEXT_CELLS;++i) e->raw[i]=teletext_byte(s,i);
}
void teletext_editor_save(const TeletextEditor *e, TeletextSettings *s) {
    const char hex[]="0123456789ABCDEF";
    memset(s->text,0,sizeof(s->text));
    for (int i=0;i<TELETEXT_CELLS;++i) {
        s->cells[i*2]=hex[e->raw[i]>>4]; s->cells[i*2+1]=hex[e->raw[i]&15];
    }
    s->cells[TELETEXT_CELLS*2]=0;
}
void teletext_editor_undo(TeletextEditor *e,TeletextEditor *undo) {
    TeletextEditor previous=*e;
    memcpy(e->raw,undo->raw,sizeof(e->raw)); e->cursor=undo->cursor;
    *undo=previous;
}
/* EN 300 706 table 26: apply set-at attributes before producing the cell,
 * and set-after attributes afterwards. Held mosaics retain their own shape. */
static int teletext_decode_row(const TeletextEditor *e,int row,TeletextCell cells[40]) {
    TeletextCell state={.foreground=7};
    unsigned held=32; int hold=0,held_separated=0,has_double=0;
    for (int col=0;col<40;++col) {
        unsigned v=e->raw[row*40+col];
        if (v==9) state.flash=0;
        if (v==12) { if (state.double_height) held=32; state.double_height=0; }
        if (v==24) state.conceal=1;
        if (v==25 || v==26) state.separated=v==26;
        if (v==28) state.background=0;
        if (v==29) state.background=state.foreground;
        if (v==30) hold=1;
        TeletextCell c=state; c.code=(unsigned char)v; c.display=(unsigned char)(v<32?32:v);
        if (v<32 && hold && state.mosaic) { c.display=(unsigned char)held; c.separated=held_separated; }
        if (c.mosaic && (c.display&32)) c.blocks=(c.display&31)|((c.display&64)>>1);
        cells[col]=c;
        if (v>=32 && state.mosaic && (v&32)) { held=v; held_separated=state.separated; }
        if ((v>=1 && v<=7) || (v>=17 && v<=23)) {
            int mosaic=v>=17;
            if (state.mosaic!=mosaic) held=32;
            state.foreground=v&7; state.mosaic=mosaic; state.conceal=0;
        }
        if (v==8) state.flash=1;
        if (v==13) { if (!state.double_height) held=32; state.double_height=1; has_double=1; }
        if (v==31) hold=0;
    }
    return has_double;
}
static int teletext_lower_row(const TeletextEditor *e,int row) {
    int lower=0;
    for (int r=0;r<row;++r) {
        if (lower) { lower=0; continue; }
        lower=memchr(e->raw+r*40,13,40)!=NULL;
    }
    return lower;
}
TeletextCell teletext_editor_cell(const TeletextEditor *e, int index) {
    TeletextCell cells[40],empty={.foreground=7};
    if (index<0 || index>=TELETEXT_CELLS) return empty;
    int lower=teletext_lower_row(e,index/40);
    teletext_decode_row(e,index/40-lower,cells);
    TeletextCell c=cells[index%40]; c.lower=lower;
    if (lower && !c.double_height) { c.display=32; c.blocks=0; }
    return c;
}
const char *teletext_editor_type(TeletextEditor *e, unsigned c) {
    if (teletext_lower_row(e,e->cursor/40)) return "This row is occupied by double height text from the row above.";
    if (e->cursor<0 || e->cursor>=TELETEXT_CELLS) return "Select a cell inside the page.";
    if (c>127 || !teletext_character(c)) return "Use unaccented letters, digits and basic punctuation.";
    if (teletext_editor_cell(e,e->cursor).mosaic)
        return "Insert Text colour before typing here (it uses one blank cell).";
    e->raw[e->cursor]=(unsigned char)c;
    if (e->cursor<TELETEXT_CELLS-1) ++e->cursor;
    return NULL;
}
const char *teletext_editor_paste(TeletextEditor *e,const char *text) {
    TeletextEditor copy=*e; int row=e->cursor/40,col=e->cursor%40;
    for (const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if (*p=='\r' && p[1]=='\n') continue;
        if (*p=='\n') { ++row; col=0; continue; }
        if (row>=23 || col>=40) return "Pasted text exceeds the page. Use up to 40 characters per line.";
        copy.cursor=row*40+col;
        const char *error=teletext_editor_type(&copy,*p); if (error) return error;
        ++col;
    }
    if (row>=23 && !(row==23 && col==0)) return "Pasted text exceeds the page.";
    *e=copy; return NULL;
}
const char *teletext_editor_background(TeletextEditor *e,int color) {
    int p=e->cursor;
    if (color<1 || color>7) return "Select a background colour first.";
    if (p<0 || p>=TELETEXT_CELLS || p%40>36)
        return "Background needs 3 empty cells and room to type after them on this row.";
    for (int i=0;i<3;++i) if (e->raw[p+i]!=' ')
        return "Background needs 3 empty cells. Existing text or graphics will not be overwritten.";
    TeletextCell previous=teletext_editor_cell(e,p);
    if (previous.lower) return "This row is occupied by double height text from the row above.";
    if (previous.conceal) return "Background colour codes cancel Conceal. Set the background before Conceal.";
    unsigned mode=previous.mosaic?16:0;
    /* EN 300 706 table 26: set-after colour, set-at background, restore ink. */
    e->raw[p]=(unsigned char)(mode+color);
    e->raw[p+1]=29;
    e->raw[p+2]=(unsigned char)(mode+previous.foreground);
    e->cursor=p+3;
    return NULL;
}
const char *teletext_editor_control(TeletextEditor *e, unsigned c) {
    if (!teletext_control(c)) return "Unsupported Level 1 attribute.";
    if (teletext_lower_row(e,e->cursor/40)) return "This row is occupied by double height text from the row above.";
    if (c==13) {
        if (e->cursor/40==22) return "Double height is not allowed on the last body row.";
        for (int i=(e->cursor/40+1)*40;i<(e->cursor/40+2)*40;++i)
            if (e->raw[i]!=' ') return "Double height needs an empty following row.";
    }
    e->raw[e->cursor]=(unsigned char)c;
    if (e->cursor%40<39) ++e->cursor;
    return NULL;
}
void teletext_editor_key(TeletextEditor *e, int key) {
    int p=e->cursor;
    switch (key) {
    case TT_LEFT: if (p) --p; break;
    case TT_RIGHT: if (p<TELETEXT_CELLS-1) ++p; break;
    case TT_UP: if (p>=40) p-=40; break;
    case TT_DOWN: if (p<TELETEXT_CELLS-40) p+=40; break;
    case TT_HOME: p=p/40*40; break;
    case TT_END: p=p/40*40+39; break;
    case TT_ENTER: if (p<TELETEXT_CELLS-40) p=(p/40+1)*40; break;
    case TT_BACKSPACE: if (p) --p; if (!teletext_lower_row(e,p/40)) e->raw[p]=' '; break;
    case TT_DELETE: if (!teletext_lower_row(e,p/40)) e->raw[p]=' '; break;
    }
    e->cursor=p;
}
void teletext_editor_click(TeletextEditor *e, int x, int y, int width, int height, int erase) {
    if (x<0 || y<0 || x>=width || y>=height || width<40 || height<23) return;
    int col=((x+1)*40-1)/width,row=((y+1)*23-1)/height;
    e->cursor=row*40+col;
    TeletextCell c=teletext_editor_cell(e,e->cursor);
    if (!c.mosaic || c.code<32 || (c.lower && !c.double_height)) return;
    if (c.lower) e->cursor-=40;
    int left=col*width/40,top=row*height/23;
    int cw=(col+1)*width/40-left,ch=(row+1)*height/23-top;
    int sy=((y-top+1)*(c.double_height?5:10)-1)/ch+(c.lower?5:0);
    int bit=(sy<3?0:sy<7?1:2)*2+((x-left+1)*2-1)/cw;
    if (bit>5) bit=5;
    unsigned blocks=erase?c.blocks&~(1u<<bit):c.blocks^(1u<<bit);
    e->raw[e->cursor]=(unsigned char)(32|(blocks&31)|((blocks&32)<<1));
}
#include "teletext_font.h"
static void teletext_plot_cell(TeletextCell c,int x,int y,int w,int h,
                               TeletextRect rect,void *ctx) {
    unsigned fg=teletext_colors[c.foreground];
    int scans=c.double_height?5:10,start=c.lower&&c.double_height?5:0;
    for (int sy=0;sy<scans;++sy) {
        int py=sy+start;
        for (int sx=0;sx<12;) {
            int first=sx;
            for (;sx<12;++sx) {
                int on=0;
                if (c.mosaic && (c.display&32)) {
                    int band=py<3?0:py<7?1:2;
                    on=(c.blocks>>(band*2+sx/6))&1;
                    if (c.separated && (sx%6==5 || py==2 || py==6 || py==9)) on=0;
                } else if (c.display>=32 && c.display<=126 && sx<10) {
                    on=(teletext_font[c.display-32][py]>>(4-sx/2))&1;
                }
                if (!on) break;
            }
            if (sx>first) rect(ctx,x+first*w/12,y+sy*h/scans,
                sx*w/12-first*w/12,(sy+1)*h/scans-sy*h/scans,fg);
            ++sx;
        }
    }
}
void teletext_editor_plot(const TeletextEditor *e,int width,int height,
                          TeletextRect rect,void *ctx) {
    if (width<40 || height<23) return;
    TeletextCell cells[40]; int lower=0;
    for (int row=0;row<23;++row) {
        int next_lower=0;
        if (!lower) next_lower=teletext_decode_row(e,row,cells);
        for (int col=0;col<40;++col) {
            int i=row*40+col,x=col*width/40,y=row*height/23;
            int w=(col+1)*width/40-x,h=(row+1)*height/23-y;
            TeletextCell c=cells[col]; c.lower=lower;
            rect(ctx,x,y,w,h,teletext_colors[c.background]);
            if ((!lower || c.double_height) && !(c.conceal && !e->reveal) && !(c.flash && e->flash_off))
                teletext_plot_cell(c,x,y,w,h,rect,ctx);
            if (e->grid && !lower && c.code<32) {
                /* Editing annotations only; the clean preview shows spacing/held mosaics. */
                const char *marks=" TTTTTTTFS  ND   MMMMMMMCHS BCKR";
                TeletextCell mark={.foreground=7,.display=(unsigned char)marks[c.code]};
                teletext_plot_cell(mark,x,y,w,h,rect,ctx);
            }
            if (e->grid) {
                rect(ctx,x,y,w,1,0x353535); rect(ctx,x,y,1,h,0x353535);
                if (c.mosaic && (!lower || c.double_height)) {
                    rect(ctx,x+w/2,y,1,h,0x353535);
                    if (c.double_height) rect(ctx,x,y+(lower?2:3)*h/5,w,1,0x353535);
                    else {
                        rect(ctx,x,y+3*h/10,w,1,0x353535);
                        rect(ctx,x,y+7*h/10,w,1,0x353535);
                    }
                }
                if (i==e->cursor) {
                    rect(ctx,x,y,w,2,0xffa000); rect(ctx,x,y+h-2,w,2,0xffa000);
                    rect(ctx,x,y,2,h,0xffa000); rect(ctx,x+w-2,y,2,h,0xffa000);
                }
            }
        }
        lower=next_lower;
    }
}
typedef struct { TeletextRect rect; void *ctx; int top, left; } TeletextOffset;
static void teletext_offset_rect(void *ctx,int x,int y,int w,int h,unsigned rgb) {
    TeletextOffset *offset=ctx; offset->rect(offset->ctx,x+offset->left,y+offset->top,w,h,rgb);
}
void teletext_page_plot(const TeletextEditor *e,const char *header,int page,
                        int width,int height,TeletextRect rect,void *ctx) {
    if (width<40 || height<24) return;
    int top=height/24;
    char title[33],number[9];
    if (header && *header) snprintf(title,sizeof(title),"%.32s",header);
    else snprintf(title,sizeof(title),"ATV TELETEXT %d",page);
    /* Columns 0-7 are receiver UI, not transmitted header text. */
    snprintf(number,sizeof(number),"P%03d",page);
    rect(ctx,0,0,width,top,0);
    for (int col=0;col<40;++col) {
        unsigned char ch=' ';
        if (col<(int)strlen(number)) ch=(unsigned char)number[col];
        if (col>=8 && col-8<(int)strlen(title)) ch=(unsigned char)title[col-8];
        TeletextCell c={.foreground=7,.display=ch};
        int x=col*width/40;
        teletext_plot_cell(c,x,0,(col+1)*width/40-x,top,rect,ctx);
    }
    TeletextOffset offset={rect,ctx,top,0};
    teletext_editor_plot(e,width,height-top,teletext_offset_rect,&offset);
}
void teletext_page_click(TeletextEditor *e,int x,int y,int width,int height,int erase) {
    if (height<24 || y<height/24) return;
    teletext_editor_click(e,x,y-height/24,width,height-height/24,erase);
}
void teletext_viewport(int width,int height,int *left,int *top,int *pw,int *ph) {
    *pw=width; *ph=width*3/4;
    if (*ph>height) { *ph=height; *pw=height*4/3; }
    *left=(width-*pw)/2; *top=(height-*ph)/2;
}
void teletext_view_plot(const TeletextEditor *e,const char *header,int page,
                        int width,int height,TeletextRect rect,void *ctx) {
    int x,y,w,h; teletext_viewport(width,height,&x,&y,&w,&h);
    rect(ctx,0,0,width,height,0x181818);
    TeletextOffset offset={rect,ctx,y,x};
    teletext_page_plot(e,header,page,w,h,teletext_offset_rect,&offset);
}
void teletext_view_click(TeletextEditor *e,int x,int y,int width,int height,int erase) {
    int left,top,w,h; teletext_viewport(width,height,&left,&top,&w,&h);
    teletext_page_click(e,x-left,y-top,w,h,erase);
}

const char *teletext_from_text(TeletextSettings *s, const char *text) {
    TeletextSettings page=*s; memset(page.text,0,sizeof(page.text)); memset(page.cells,0,sizeof(page.cells));
    memcpy(page.header,s->header,sizeof(page.header));
    memset(page.text,' ',TELETEXT_CELLS);
    int row=0,col=0;
    for (const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if (*p=='\r' && p[1]=='\n') continue;
        if (*p=='\n') { ++row; col=0; continue; }
        if (row>=TELETEXT_ROWS || col>=TELETEXT_COLUMNS)
            return "Use up to 23 lines of 40 characters each; press Enter for a new line.";
        if (!teletext_character(*p))
            return "Use unaccented letters, digits and basic punctuation for teletext.";
        page.text[row*TELETEXT_COLUMNS+col++]=(char)*p;
    }
    if (row>=TELETEXT_ROWS && !(row==TELETEXT_ROWS && !col))
        return "Use up to 23 lines of 40 characters each.";
    *s=page; return NULL;
}
void teletext_to_text(const TeletextSettings *s, char out[TELETEXT_INPUT_SIZE], int crlf) {
    size_t pos=0;
    if (!s->text[0]) { out[0]=0; return; }
    int last=TELETEXT_ROWS-1;
    while (last>0) {
        int i=0; while (i<TELETEXT_COLUMNS && s->text[last*TELETEXT_COLUMNS+i]==' ') ++i;
        if (i<TELETEXT_COLUMNS) break;
        --last;
    }
    for (int row=0;row<=last;++row) {
        int len=TELETEXT_COLUMNS;
        while (len && s->text[row*TELETEXT_COLUMNS+len-1]==' ') --len;
        memcpy(out+pos,s->text+row*TELETEXT_COLUMNS,(size_t)len); pos+=(size_t)len;
        if (row<last) { if (crlf) out[pos++]='\r'; out[pos++]='\n'; }
    }
    out[pos]=0;
}
static const char *teletext_tti_line(TeletextEditor *e,char *line,unsigned *seen,int *page_seen) {
    if (strlen(line)<3 || line[2]!=',') return "Expected a TTI record such as PN,10000 or OL,1,text.";
    char *value=line+3;
    if (!strncmp(line,"OL,",3)) {
        char *end; long row=strtol(value,&end,10);
        if (end==value || *end!=',' || row<1 || row>23)
            return "Only body rows OL,1 through OL,23 are supported. Edit the header in the editor.";
        if (*seen&(1u<<row)) return "Duplicate output row.";
        *seen|=1u<<row; int col=0;
        for (const unsigned char *p=(const unsigned char *)end+1;*p;++p) {
            unsigned c=*p;
            if (c==27) {
                if (p[1]<64 || p[1]>95) return "Incomplete or invalid TTI ESC sequence.";
                c=*++p-64;
            } else if (c>=128 && c<=159) c-=128;
            else if (c<32 || c>127) return "TTI needs 7-bit text and ESC or 8-bit translated control codes.";
            if (c<32 && !teletext_control(c)) return "This TTI attribute is not supported by our Level 1 profile.";
            if (col==40) return "An output row exceeds 40 cells, including attribute cells.";
            e->raw[(row-1)*40+col++]=(unsigned char)c;
        }
        return NULL;
    }
    if (!strncmp(line,"PN,",3)) {
        if ((*page_seen)++) return "Load one page at a time; multiple PN records are not supported.";
        if (strlen(value)!=5 || value[0]<'1' || value[0]>'8' ||
            teletext_hex(value[1])<0 || teletext_hex(value[2])<0 || value[3]!='0' || value[4]!='0')
            return "Use one PN,mpp00 page without subpages. The selected editor page is the destination.";
        return NULL;
    }
    if (!strncmp(line,"DE,",3)) return NULL; /* Description, not displayed header. */
    if (!strncmp(line,"CT,",3)) {
        char *end; long n=strtol(value,&end,10);
        if (end==value || n<1 || n>86400 || (*end!=',') ||
            (strcmp(end,",T") && strcmp(end,",C"))) return "Invalid TTI cycle time.";
        return NULL; /* Editor's existing carousel interval is authoritative. */
    }
    if (!strncmp(line,"PS,",3)) {
        unsigned flags=0;
        if (strlen(value)!=4) return "Invalid TTI page status.";
        for (int i=0;i<4;++i) { int v=teletext_hex(value[i]); if (v<0) return "Invalid TTI page status."; flags=flags*16+(unsigned)v; }
        if (flags&0x03bf) return "TTI page flags or national character set do not match this Level 1 profile.";
        return NULL;
    }
    if (!strncmp(line,"SC,",3)) return strcmp(value,"0000")?"TTI subpages are not supported.":NULL;
    if (!strncmp(line,"RE,",3)) return strcmp(value,"0")?"Only the Latin region is supported.":NULL;
    if (!strncmp(line,"PF,",3)) return strcmp(value,"0,0")?"Only basic Level 1, 7-bit page coding is supported.":NULL;
    return "Unsupported TTI record (for example Fastext or enhancement packets).";
}
int teletext_import_data(TeletextSettings *page,const unsigned char *data,size_t length,
                         int tti,char *error,size_t error_size) {
    const char *problem=NULL; unsigned line_number=0;
    if (length>TELETEXT_IMPORT_LIMIT) problem="Page files must be at most 16 KiB.";
    else if (memchr(data,0,length)) problem="The file contains a NUL byte; use a TXT or TTI page file.";
    if (problem) { snprintf(error,error_size,"%s",problem); return 0; }
    if (length>=3 && !memcmp(data,"\xef\xbb\xbf",3)) { data+=3; length-=3; }
    char buffer[TELETEXT_IMPORT_LIMIT+1]; memcpy(buffer,data,length); buffer[length]=0;
    TeletextSettings candidate={0}; candidate.enabled=page->enabled;
    memcpy(candidate.header,page->header,sizeof(candidate.header));
    if (!tti) problem=teletext_from_text(&candidate,buffer);
    else {
        TeletextEditor e; teletext_editor_load(&e,&candidate);
        unsigned seen=0; int page_seen=0; char *line=buffer;
        while (line && !problem) {
            ++line_number;
            char *end=strchr(line,'\n'); if (end) *end=0;
            size_t n=strlen(line); if (n && line[n-1]=='\r') line[n-1]=0;
            if (*line) problem=teletext_tti_line(&e,line,&seen,&page_seen);
            if (!end) break;
            line=end+1;
        }
        if (!problem && !seen) problem="No output rows found in this TTI file.";
        if (!problem) teletext_editor_save(&e,&candidate);
    }
    if (!problem) { line_number=0; problem=teletext_validate(&candidate); }
    if (problem) {
        if (tti && line_number) snprintf(error,error_size,"TTI line %u: %s",line_number,problem);
        else snprintf(error,error_size,"%s",problem);
        return 0;
    }
    /* Import only the body; settings and the other carousel pages are untouched. */
    memcpy(page->text,candidate.text,sizeof(page->text));
    memcpy(page->cells,candidate.cells,sizeof(page->cells));
    if (error_size) error[0]=0;
    return 1;
}
const char *datv_validate(DatvSettings s, int width, int height) {
    if (s.activity_motion<0 || s.activity_motion>1 || (s.activity_motion && s.motion))
        return "Invalid activity motion option.";
    if (s.activity_motion && s.fps<2) return "Activity motion requires at least 2 frames per second.";
    if (s.motion && (s.motion!=1 || width!=160 || height!=120 || s.fps!=10 ||
        (s.gop!=5 && s.gop!=10 && s.gop!=20) || s.seconds<4 ||
        s.audio.bitrate || s.eit_enabled || s.teletext.enabled))
        return "Motion challenge: 160 x 120, 10 fps, GOP 5/10/20, at least 4 seconds, no audio/EIT/teletext.";
    if (s.audio.bitrate && s.audio.bitrate!=48000 && s.audio.bitrate!=96000)
        return "Audio bitrate must be 48000 or 96000 bit/s.";
    if (s.audio.bitrate) {
        if (!s.audio.source[0] || !memchr(s.audio.source,0,sizeof(s.audio.source)))
            return "Enter a PulseAudio source, for example ffmix.monitor.";
        if (s.bitrate<datv_audio_reservation(s.audio.bitrate)+40000)
            return "Insufficient TS capacity for audio and video. Choose a higher FEC rate or 48 kbit/s audio.";
    }
    if (s.bitrate < DATV_MIN_BITRATE || s.bitrate > 2000000)
        return "TS bitrate must be between 30080 and 2000000 bit/s. Choose an available combination of system, symbol rate, FEC and pilots.";
    if (s.seconds < 1 || s.seconds > 60) return "Duration must be between 1 and 60 seconds.";
    if (s.fps < 1 || s.fps > 25) return "Frame rate must be between 1 and 25 frames/s.";
    if (s.buffer_ms && (s.buffer_ms<1000 || s.buffer_ms>10000)) return "Transport buffer must be between 1000 and 10000 ms.";
    if (s.gop < 1 || s.gop > 250) return "GOP must be between 1 and 250 frames (1 = IDR only).";
    if (width < 16 || height < 16 || width > 1920 || height > 1440 || width % 2 || height % 2)
        return "For TS, use even dimensions between 16 x 16 and 1920 x 1440.";
    const char *tt_error=teletext_validate(&s.teletext);
    if (tt_error) return tt_error;
    if (s.teletext.enabled && s.bitrate<60000)
        return "Teletext requires a TS bitrate of at least 60000 bit/s.";
    if (s.eit_enabled!=0 && s.eit_enabled!=1) return "Invalid EIT option.";
    if (s.eit_enabled) {
        const char *error=station_validate(&s.station);
        if (error) return error;
        wchar_t locator[13]={0}; size_t n=0;
        while (n<sizeof(s.locator) && s.locator[n]) ++n;
        if (n>12) return "Invalid EIT locator.";
        for (size_t i=0;i<n;++i) locator[i]=(unsigned char)s.locator[i];
        if (n && !valid_locator(locator)) return "Invalid EIT locator.";
    }
    return NULL;
}
const Resolution resolutions43[RESOLUTION_COUNT] = {
    {120,90},{128,96},{144,108},{160,120},{240,180},
    {320,240},{640,480},{800,600},{1024,768},
    {1080,810},{1280,960},{1600,1200},{1920,1440}
};
const Resolution resolutions169[RESOLUTION_COUNT] = {
    {120,68},{128,72},{144,82},{160,90},{240,136}, /* Even heights for H.264. */
    {320,180},{640,360},{800,450},{960,540},
    {1024,576},{1280,720},{1600,900},{1920,1080}
};
const wchar_t *const bands[12] = {
    L"50 MHz",L"70 MHz",L"144 MHz",L"436 MHz",L"1152 MHz",L"2330 MHz",
    L"3.4 GHz",L"5.7 GHz",L"10 GHz",L"24 GHz",L"47 GHz",NULL
};
const wchar_t *const band_files[12] = {
    L"50MHz",L"70MHz",L"144MHz",L"436MHz",L"1152MHz",L"2330MHz",
    L"3.4GHz",L"5.7GHz",L"10GHz",L"24GHz",L"47GHz",NULL
};
static int digit(wchar_t c) { return c >= L'0' && c <= L'9'; }
int valid_call(const wchar_t *s) {
    size_t n = wcslen(s);
    int letter = 0, number = 0;
    if (n < 3 || n > 24 || s[0] == L'/' || s[n-1] == L'/') return 0;
    for (size_t i=0; i<n; ++i) {
        if (s[i] >= L'A' && s[i] <= L'Z') letter = 1;
        else if (digit(s[i])) number = 1;
        else if (s[i] != L'/' || (i && s[i-1] == L'/')) return 0;
    }
    return letter && number;
}
int valid_locator(const wchar_t *s) {
    size_t n = wcslen(s);
    if (n < 4 || n > LOCATOR_MAX_LENGTH || n % 2) return 0;
    for (size_t i=0; i<n; ++i) {
        if (i < 2) { if (s[i] < L'A' || s[i] > L'R') return 0; }
        else if ((i/2)%2) { if (!digit(s[i])) return 0; }
        else if (s[i] < L'A' || s[i] > L'X') return 0;
    }
    return 1;
}
static int four_digits(const wchar_t *s) {
    return wcslen(s) == 4 && digit(s[0]) && digit(s[1]) && digit(s[2]) && digit(s[3]);
}
int valid_code(const wchar_t *s) {
    if (!four_digits(s)) return 0;
    int same = 1, ascending = 1, descending = 1;
    for (int i=1; i<4; ++i) {
        same &= s[i] == s[i-1];
        ascending &= s[i] == s[i-1]+1;
        descending &= s[i]+1 == s[i-1];
    }
    return !same && !ascending && !descending;
}
int code_digit_sum(const wchar_t *s) {
    if (!four_digits(s)) return -1;
    return (s[0]-L'0') + (s[1]-L'0') + (s[2]-L'0') + (s[3]-L'0');
}
int generated_code_valid(unsigned code) {
    int d[4];
    if (code < 1000 || code > 9999) return 0;
    for (int i=3; i>=0; --i) { d[i] = code % 10; code /= 10; }
    for (int i=0; i<4; ++i) {
        for (int j=0; j<i; ++j) if (d[i] == d[j]) return 0;
        if (i && abs(d[i]-d[i-1]) == 1) return 0;
    }
    return 1;
}
void filename_call(wchar_t *out, const wchar_t *in) {
    do { *out++ = *in == L'/' ? L'_' : *in; } while (*in++);
}

const wchar_t contest_color_advice[] = L"DATV contest: white on black recommended.";

ContestPalette contest_palette(int blue_yellow, int inverse) {
    ContestPalette palette = blue_yellow
        ? (ContestPalette){{0, 0, 128}, {255, 255, 0}}
        : (ContestPalette){{0, 0, 0}, {255, 255, 255}};
    if (inverse) {
        ContestColor swap = palette.background;
        palette.background = palette.foreground;
        palette.foreground = swap;
    }
    return palette;
}

const wchar_t *contest_color_suffix(int blue_yellow) {
    return blue_yellow ? L"-blauw-geel" : L"";
}

void filename_locator(wchar_t out[7], const wchar_t *locator) {
    out[0] = 0;
    if (!valid_locator(locator)) return;
    size_t n = wcslen(locator);
    if (n > 6) n = 6;
    wmemcpy(out, locator, n);
    out[n] = 0;
}

int locator_square_changed(wchar_t previous[7], const wchar_t *locator) {
    if (!valid_locator(locator) || wcslen(locator) < 6) return 0;
    wchar_t square[7];
    filename_locator(square, locator);
    int changed = previous[0] && wcscmp(previous, square);
    wcscpy(previous, square);
    return changed != 0;
}


/* Shared, versioned configuration. All known fields are required, so a partial
 * or damaged file is never applied to the GUI. Unknown keys are rejected. */
#ifdef _WIN32
#include <windows.h>
#endif

/* EBU 75% RGB bars, left to right: white, yellow, cyan, green,
 * magenta, red, blue, black. Independent of the contest palette. */
const ContestColor ebu_colors[EBU_BAR_COUNT] = {
    {191,191,191}, {191,191,0}, {0,191,191}, {0,191,0},
    {191,0,191}, {191,0,0}, {0,0,191}, {0,0,0}
};

typedef struct { const char *key; size_t offset, size; int min, max; } ConfigField;
#define CONFIG_INT_FIELD(field, low, high) {#field, offsetof(AppConfig, field), 0, low, high}
#define CONFIG_TEXT_FIELD(field) {#field, offsetof(AppConfig, field), sizeof(((AppConfig *)0)->field), 0, 0}
static const ConfigField config_fields[]={
    CONFIG_TEXT_FIELD(call), CONFIG_TEXT_FIELD(locator), CONFIG_TEXT_FIELD(code),
    CONFIG_INT_FIELD(mode,0,IMAGE_MODE_COUNT-1), CONFIG_INT_FIELD(automatic,0,1), CONFIG_INT_FIELD(aspect,0,1),
    CONFIG_INT_FIELD(resolution,0,RESOLUTION_COUNT-1), CONFIG_INT_FIELD(band,0,10),
    CONFIG_INT_FIELD(show,0,1), CONFIG_INT_FIELD(inverse,0,1), CONFIG_INT_FIELD(blue_yellow,0,1),
    CONFIG_INT_FIELD(activity_ebu,0,1), CONFIG_INT_FIELD(activity_motion,0,1),
    CONFIG_INT_FIELD(ebu_top,0,1), CONFIG_INT_FIELD(ebu_bottom,0,1),
    CONFIG_INT_FIELD(show_sum,0,1), CONFIG_INT_FIELD(top_code,0,1), CONFIG_INT_FIELD(genius,1,4),
    CONFIG_INT_FIELD(ts.bitrate,DATV_MIN_BITRATE,2000000), CONFIG_INT_FIELD(ts.seconds,1,60),
    CONFIG_INT_FIELD(ts.buffer_ms,0,10000),
    CONFIG_INT_FIELD(ts.fps,1,25), CONFIG_INT_FIELD(ts.gop,1,250),
    CONFIG_INT_FIELD(ts.eit_enabled,0,1), CONFIG_INT_FIELD(udp.video.eit_enabled,0,1),
    CONFIG_INT_FIELD(teletext.enabled,0,1), CONFIG_TEXT_FIELD(teletext.text),
    CONFIG_TEXT_FIELD(teletext.cells), CONFIG_TEXT_FIELD(teletext.header),
    CONFIG_INT_FIELD(teletext.interval,0,10), CONFIG_INT_FIELD(teletext.extra_count,0,TELETEXT_MAX_PAGES-1), CONFIG_TEXT_FIELD(teletext.extra),
    CONFIG_TEXT_FIELD(station.city), CONFIG_TEXT_FIELD(station.description), CONFIG_TEXT_FIELD(station.operator_name),
    CONFIG_TEXT_FIELD(udp.ip), CONFIG_INT_FIELD(udp.port,1,65535),
    CONFIG_INT_FIELD(udp.video.bitrate,DATV_MIN_BITRATE,2000000), CONFIG_INT_FIELD(udp.video.fps,1,25),
    CONFIG_INT_FIELD(udp.video.gop,1,250),
    CONFIG_INT_FIELD(udp.video.buffer_ms,0,10000),
    CONFIG_INT_FIELD(udp.video.audio.bitrate,0,96000), CONFIG_TEXT_FIELD(udp.video.audio.source),
    CONFIG_INT_FIELD(ts_dvb.system,0,DVB_SYSTEM_COUNT-1),
    CONFIG_INT_FIELD(ts_dvb.symbol_rate,0,DVB_SYMBOL_RATE_COUNT-1),
    CONFIG_INT_FIELD(ts_dvb.fec,0,DVB_FEC_COUNT-1),
    CONFIG_INT_FIELD(ts_dvb.pilots,0,1),
    CONFIG_INT_FIELD(ts_dvb.guard,0,DVB_GUARD_COUNT-1),
    CONFIG_INT_FIELD(ts_dvb.bandwidth_khz,1,8000),
    CONFIG_INT_FIELD(udp_dvb.system,0,DVB_SYSTEM_COUNT-1),
    CONFIG_INT_FIELD(udp_dvb.symbol_rate,0,DVB_SYMBOL_RATE_COUNT-1),
    CONFIG_INT_FIELD(udp_dvb.fec,0,DVB_FEC_COUNT-1),
    CONFIG_INT_FIELD(udp_dvb.pilots,0,1),
    CONFIG_INT_FIELD(udp_dvb.guard,0,DVB_GUARD_COUNT-1),
    CONFIG_INT_FIELD(udp_dvb.bandwidth_khz,1,8000)
};
#define CONFIG_FIELDS (sizeof(config_fields)/sizeof(config_fields[0]))
_Static_assert(CONFIG_FIELDS<64,"configuration field mask overflow");
AppConfig config_defaults(void) {
    AppConfig s={0}; s.automatic=1; s.resolution=DEFAULT_RESOLUTION_INDEX;
    s.band=3; s.show=1; s.genius=1; s.ts=datv_defaults(); s.udp=datv_udp_defaults();
    s.ts_dvb=dvb_defaults(); s.udp_dvb=dvb_defaults();
    s.ts.bitrate=dvb_bitrate(s.ts_dvb); s.udp.video.bitrate=dvb_bitrate(s.udp_dvb);
    return s;
}
static int config_text_valid(const char *s, size_t capacity) {
    size_t i=0;
    while (i<capacity && s[i]) {
        unsigned c=(unsigned char)s[i++];
        if (c<32 || c==127) return 0;
        if (c<128) continue;
        unsigned n, value, minimum;
        if (c>=0xc2 && c<=0xdf) { n=1; value=c&31; minimum=128; }
        else if (c>=0xe0 && c<=0xef) { n=2; value=c&15; minimum=2048; }
        else if (c>=0xf0 && c<=0xf4) { n=3; value=c&7; minimum=65536; }
        else return 0;
        while (n--) {
            if (i>=capacity || ((unsigned char)s[i]&0xc0)!=0x80) return 0;
            value=(value<<6)|((unsigned char)s[i++]&63);
        }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return 0;
    }
    return i<capacity;
}
/* DVB UTF-8 (EN 300 468 annex A) is restricted to the BMP. Single-line
 * station fields also avoid DVB control characters and config line injection. */
const char *station_validate(const StationInfo *station) {
    const char *fields[]={station->city,station->description,station->operator_name};
    const size_t capacities[]={sizeof(station->city),sizeof(station->description),sizeof(station->operator_name)};
    const size_t limits[]={EIT_CITY_LENGTH,EIT_DESCRIPTION_LENGTH,EIT_OPERATOR_LENGTH};
    for (int i=0;i<3;++i) {
        if (!config_text_valid(fields[i],capacities[i]))
            return "Enter valid single-line text for EIT station information.";
        size_t count=0;
        const unsigned char *p=(const unsigned char *)fields[i];
        while (*p) {
            unsigned c=*p++;
            if (c>=0xf0 || (c==0xc2 && *p>=0x80 && *p<=0x9f))
                return "EIT does not support these characters. Use text without emoji or control characters.";
            if ((c&0xc0)!=0x80) ++count;
        }
        if (count>limits[i]) return "Use up to 40 characters for city/operator name and 240 for station description.";
    }
    return NULL;
}
static int config_valid(const AppConfig *s) {
    for (size_t i=0;i<CONFIG_FIELDS;++i) {
        const ConfigField *f=&config_fields[i]; const char *value=(const char *)s+f->offset;
        if (f->size) {
            if (!config_text_valid(value,f->size)) return 0;
            size_t characters=0;
            for (const unsigned char *p=(const unsigned char *)value; *p; ++p)
                if ((*p&0xc0)!=0x80) ++characters;
            size_t limit=f->offset==offsetof(AppConfig,teletext.extra)?sizeof(s->teletext.extra)-1:f->offset==offsetof(AppConfig,teletext.cells)?TELETEXT_CELLS*2:f->offset==offsetof(AppConfig,teletext.header)?32:f->offset==offsetof(AppConfig,teletext.text)?TELETEXT_CELLS:f->offset==offsetof(AppConfig,udp.video.audio.source)?255:f->size==16?15:(f->size-1)/4;
            if (characters>limit) return 0;
        }
        else if (*(const int *)value<f->min || *(const int *)value>f->max) return 0;
    }
    if (s->udp.video.audio.bitrate && (s->genius!=3 || !dvb_audio_allowed(s->udp_dvb))) return 0;
    if (s->ts.buffer_ms && (s->ts.buffer_ms<1000 || s->ts.buffer_ms>10000)) return 0;
    if (teletext_validate(&s->teletext)) return 0;
    if (station_validate(&s->station)) return 0;
    DatvUdpSettings udp=s->udp;
    udp.video.activity_motion=0; /* Editable settings may precede a frame-rate correction. */
    if (!udp.ip[0]) strcpy(udp.ip,"127.0.0.1"); /* An unused destination may be empty. */
    return !datv_udp_validate(udp,160,120) && dvb_bitrate(s->ts_dvb)>0 && dvb_bitrate(s->udp_dvb)>0;
}
#ifdef _WIN32
static wchar_t *config_wide(const char *path) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    wchar_t *w=n?malloc((size_t)n*sizeof(*w)):NULL;
    if (w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,n);
    return w;
}
#endif
static FILE *config_open(const char *path, int writing) {
#ifdef _WIN32
    wchar_t *w=config_wide(path);
    if (!w) { errno=EINVAL; return NULL; }
    FILE *f=_wfopen(w,writing?L"wb":L"rb"); free(w); return f;
#else
    return fopen(path,writing?"wb":"rb");
#endif
}
int teletext_import_file(TeletextSettings *page,const char *path,char *error,size_t error_size) {
    const char *ext=strrchr(path,'.'); int tti=0;
    if (ext && strlen(ext)==4 && (ext[1]|32)=='t' && (ext[2]|32)=='t' && (ext[3]|32)=='i') tti=1;
    else if (!ext || strlen(ext)!=4 || (ext[1]|32)!='t' || (ext[2]|32)!='x' || (ext[3]|32)!='t') {
        snprintf(error,error_size,"Choose a .tti or .txt page file."); return 0;
    }
    FILE *f=config_open(path,0);
    if (!f) { snprintf(error,error_size,"Cannot open the page file: %s",strerror(errno)); return 0; }
    unsigned char data[TELETEXT_IMPORT_LIMIT+1]; size_t n=fread(data,1,sizeof(data),f);
    int failed=ferror(f); if (fclose(f)) failed=1;
    if (failed) { snprintf(error,error_size,"Cannot read the page file."); return 0; }
    return teletext_import_data(page,data,n,tti,error,error_size);
}
int config_load(const char *path, AppConfig *out) {
    FILE *f=config_open(path,0);
    if (!f) return errno==ENOENT?0:-1;
    AppConfig s=config_defaults(); uint64_t seen=0;
    int version=0, ok=1, migrated_ts=0, migrated_udp=0; char line[sizeof(s.teletext.extra)+100];
    while (fgets(line,sizeof(line),f)) {
        size_t n=strlen(line);
        if (!n || (line[n-1]!='\n' && !feof(f))) { ok=0; break; }
        while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n]=0;
        if (!n || line[0]=='#') continue;
        char *value=strchr(line,'=');
        if (!value) { ok=0; break; } *value++=0;
        if (!strcmp(line,"version")) {
            if (version || (strcmp(value,"1") && strcmp(value,"2") && strcmp(value,"3") && strcmp(value,"4") && strcmp(value,"5") && strcmp(value,"6") && strcmp(value,"7") && strcmp(value,"8") && strcmp(value,"9"))) { ok=0; break; }
            version=value[0]-'0'; continue;
        }
        size_t i;
        for (i=0;i<CONFIG_FIELDS;++i) if (!strcmp(line,config_fields[i].key)) break;
        if (i==CONFIG_FIELDS || (seen&(UINT64_C(1)<<i))) { ok=0; break; }
        seen|=UINT64_C(1)<<i;
        const ConfigField *field=&config_fields[i]; char *target=(char *)&s+field->offset;
        if (field->size) {
            if (strlen(value)>=field->size) { ok=0; break; }
            strcpy(target,value);
        } else {
            char *end; errno=0; long v=strtol(value,&end,10);
            /* Retired 31/32 ksym/s IDs migrate to the next supported rate. */
            if (!errno && *value && !*end && (v==9 || v==10)) {
                if (field->offset==offsetof(AppConfig,ts_dvb.symbol_rate)) {
                    v=8; migrated_ts=1;
                } else if (field->offset==offsetof(AppConfig,udp_dvb.symbol_rate)) {
                    v=8; migrated_udp=1;
                }
            }
            if (errno || !*value || *end || v<field->min || v>field->max) { ok=0; break; }
            *(int *)target=(int)v;
        }
    }
    if (ferror(f)) ok=0;
    if (fclose(f)) ok=0;
    /* Configurations saved before the EBU options keep both strips off. */
    uint64_t required = (UINT64_C(1)<<CONFIG_FIELDS)-1;
    for (size_t i=0; i<CONFIG_FIELDS; ++i)
        if (config_fields[i].offset==offsetof(AppConfig,ebu_top) ||
            config_fields[i].offset==offsetof(AppConfig,ebu_bottom) ||
            config_fields[i].offset==offsetof(AppConfig,ts.buffer_ms) ||
            config_fields[i].offset==offsetof(AppConfig,udp.video.buffer_ms)) required &= ~(UINT64_C(1)<<i);
    if (version<9) for (size_t i=0;i<CONFIG_FIELDS;++i)
        if (config_fields[i].offset==offsetof(AppConfig,activity_ebu) ||
            config_fields[i].offset==offsetof(AppConfig,activity_motion)) required &= ~(UINT64_C(1)<<i);
    if (version<8) for (size_t i=0;i<CONFIG_FIELDS;++i)
        if (config_fields[i].offset==offsetof(AppConfig,teletext.cells) ||
            config_fields[i].offset==offsetof(AppConfig,teletext.header) ||
            config_fields[i].offset==offsetof(AppConfig,teletext.interval) ||
            config_fields[i].offset==offsetof(AppConfig,teletext.extra_count) ||
            config_fields[i].offset==offsetof(AppConfig,teletext.extra)) required &= ~(UINT64_C(1)<<i);
    uint64_t audio_fields=0;
    for (size_t i=0;i<CONFIG_FIELDS;++i)
        if (!strncmp(config_fields[i].key,"udp.video.audio.",16)) audio_fields|=UINT64_C(1)<<i;
    if (version<7 && !(seen&audio_fields)) required&=~audio_fields;
    uint64_t tt_fields=0;
    for (size_t i=0;i<CONFIG_FIELDS;++i)
        if (!strncmp(config_fields[i].key,"teletext.",9)) tt_fields|=UINT64_C(1)<<i;
    if (version<5 && !(seen&tt_fields)) required&=~tt_fields;
    uint64_t eit_fields=0;
    for (size_t i=0;i<CONFIG_FIELDS;++i)
        if (!strcmp(config_fields[i].key,"ts.eit_enabled") ||
            !strcmp(config_fields[i].key,"udp.video.eit_enabled") ||
            !strncmp(config_fields[i].key,"station.",8)) eit_fields|=UINT64_C(1)<<i;
    if (version<4 && !(seen&eit_fields)) required&=~eit_fields;
    /* Old files have no RF parameters. Keep their stored bitrates until the
     * user opens a dialog, where the calculated value is explicitly shown. */
    uint64_t dvb_fields=0;
    for (size_t i=0; i<CONFIG_FIELDS; ++i)
        if (config_fields[i].offset>=offsetof(AppConfig,ts_dvb)) dvb_fields|=UINT64_C(1)<<i;
    if (version<3 && !(seen&dvb_fields)) {
        required&=~dvb_fields;
        DvbSettings *radio[]={&s.ts_dvb,&s.udp_dvb};
        const int rates[]={s.ts.bitrate,s.udp.video.bitrate};
        for (int j=0;j<2;++j) {
            int found=0;
            for (int system=DVB_S;system<=DVB_S2 && !found;++system)
            for (int sr=0;sr<DVB_SYMBOL_RATE_COUNT && !found;++sr)
            for (int fec=0;fec<DVB_FEC_COUNT && !found;++fec)
            for (int pilots=0;pilots<=(system==DVB_S2) && !found;++pilots) {
                DvbSettings candidate=dvb_defaults(); candidate.system=system;
                candidate.symbol_rate=sr; candidate.fec=fec; candidate.pilots=pilots;
                if (dvb_bitrate(candidate)==rates[j]) { *radio[j]=candidate; found=1; }
            }
        }
    }
    if (migrated_ts) s.ts.bitrate=dvb_bitrate(s.ts_dvb);
    if (migrated_udp) s.udp.video.bitrate=dvb_bitrate(s.udp_dvb);
    s.ts.motion=s.udp.video.motion=s.genius==4;
    s.ts.activity_motion=s.udp.video.activity_motion=s.genius!=4 && s.mode==IMAGE_ACTIVITY && s.activity_motion;
    if (!ok || !version || (seen&required)!=required || !config_valid(&s)) return -1;
    if (version<6 && s.resolution>=11) return -1;
    /* Version 1 stored 240px at index 10; version 2 sorts by width. */
    if (version==1) {
        if (s.resolution==10) s.resolution=2;
        else if (s.resolution>=2) ++s.resolution;
    }
    /* Version 6 inserts 128px and 144px after 120px. */
    if (version<6 && s.resolution>=1) s.resolution+=2;
    *out=s; return 1;
}
int config_save(const char *path, const AppConfig *s) {
    if (!config_valid(s)) return 0;
    size_t n=strlen(path)+5; char *temporary=malloc(n);
    if (!temporary) return 0;
    snprintf(temporary,n,"%s.tmp",path);
    FILE *f=config_open(temporary,1);
    if (!f) { free(temporary); return 0; }
    int ok=fprintf(f,"# ATV contest number generator\nversion=9\n")>=0;
    for (size_t i=0;i<CONFIG_FIELDS && ok;++i) {
        const ConfigField *field=&config_fields[i]; const char *value=(const char *)s+field->offset;
        ok=(field->size?fprintf(f,"%s=%s\n",field->key,value):
            fprintf(f,"%s=%d\n",field->key,*(const int *)value))>=0;
    }
    if (fflush(f)) ok=0;
    if (fclose(f)) ok=0;
#ifdef _WIN32
    wchar_t *a=config_wide(temporary), *b=config_wide(path);
    if (!a || !b || !ok || !MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) ok=0;
    if (!ok && a) DeleteFileW(a);
    free(a); free(b);
#else
    if (ok && rename(temporary,path)) ok=0;
    if (!ok) remove(temporary);
#endif
    free(temporary); return ok;
}
