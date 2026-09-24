#include "core.h"
#include "datv.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

const wchar_t *const image_modes[IMAGE_MODE_COUNT] = {L"Contest", L"PM5544", L"FUBK", L"PM5644"};
/* Display order is independent of the persistent configuration IDs. */
const int image_mode_order[IMAGE_MODE_COUNT] = {IMAGE_CONTEST, IMAGE_PM5544, IMAGE_PM5644, IMAGE_FUBK};
int image_mode_row(int mode) {
    for (int row=0; row<IMAGE_MODE_COUNT; ++row)
        if (image_mode_order[row]==mode) return row;
    return 0;
}
int valid_fubk_locator(const wchar_t *s) { return valid_locator(s) && wcslen(s)>=6; }

const wchar_t *const dvb_system_names[DVB_SYSTEM_COUNT]={L"DVB-S",L"DVB-S2",L"DVB-T"};
/* Indices are stored in configuration files: append new rates. */
const int dvb_symbol_rates[DVB_SYMBOL_RATE_COUNT]={35,66,125,150,333,500,25,30,33};
/* Sorted display order keeps the persistent IDs of remaining rates intact. */
const int dvb_symbol_rate_order[DVB_SYMBOL_RATE_COUNT]={6,7,8,0,1,2,3,4,5};
int dvb_symbol_rate_row(int id) {
    for (int row=0;row<DVB_SYMBOL_RATE_COUNT;++row)
        if (dvb_symbol_rate_order[row]==id) return row;
    return -1;
}
const int dvb_bandwidths[DVB_BANDWIDTH_COUNT]={150,250,333,500};
/* Preserve existing FEC IDs 0..2. The added choices match Portsdown menus. */
const wchar_t *const dvb_fec_names[DVB_FEC_COUNT]={L"1/2",L"2/3",L"3/4",L"5/6",L"7/8",L"1/4",L"1/3",L"3/5",L"8/9",L"9/10"};
static const int fec_num[DVB_FEC_COUNT]={1,2,3,5,7,1,1,3,8,9};
static const int fec_den[DVB_FEC_COUNT]={2,3,4,6,8,4,3,5,9,10};
static const int fec_kbch[DVB_FEC_COUNT]={32208,43040,48408,53840,0,16008,21408,38688,57472,58192};
const wchar_t *const dvb_guard_names[DVB_GUARD_COUNT]={L"1/8",L"1/16",L"1/32"};
DvbSettings dvb_defaults(void) { return (DvbSettings){DVB_S,2,0,0,2,500}; }
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
        s.symbol_rate>=DVB_SYMBOL_RATE_COUNT || s.fec<0 || s.fec>=DVB_FEC_COUNT ||
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

DatvSettings datv_defaults(void) { return (DatvSettings){.bitrate=120000, .seconds=10, .fps=5, .gop=5}; }
DatvUdpSettings datv_udp_defaults(void) {
    DatvUdpSettings s={.port=10000,.video={.bitrate=120000,.seconds=10,.fps=10,.gop=2}};
    return s;
}
void datv_udp_status_text(DatvUdpSettings s, DatvUdpStatus status, char *text, size_t size) {
    if (status.state==DATV_FAILED) snprintf(text,size,"%s",status.error);
    else if (status.state==DATV_PREPARING) snprintf(text,size,"Beeld voorbereiden en bitrate controleren...");
    else if (status.state==DATV_STOPPED) snprintf(text,size,"UDP gestopt (%llu pakketten).",(unsigned long long)status.packets);
    else snprintf(text,size,"UDP-uitvoer actief naar %s:%d\n%d bit/s, %llu pakketten van 1316 bytes, QP %d.",
        s.ip,s.port,s.video.bitrate,(unsigned long long)status.packets,status.qp);
    if (size && status.state==DATV_RUNNING && status.refusals &&
        status.seconds-status.last_refusal_seconds<3.0) {
        size_t used=strlen(text);
        if (used<size) snprintf(text+used,size-used,
            "\nOntvanger meldde zojuist een gesloten UDP-poort.\nVerzending gaat door; controleer of IPTS actief is.");
    }
}
const char *datv_udp_validate(DatvUdpSettings s, int width, int height) {
    if (s.port<1 || s.port>65535) return "Poort moet tussen 1 en 65535 liggen.";
    /* Numeric unicast IPv4 only; no DNS lookup, ambiguous octal or broadcast. */
    int octets[4]={0}; size_t pos=0;
    for (int part=0; part<4; ++part) {
        size_t start=pos;
        while (pos<sizeof(s.ip) && s.ip[pos]>='0' && s.ip[pos]<='9') {
            if (pos-start>=3) return "Vul een geldig unicast IPv4-adres in.";
            octets[part]=octets[part]*10+s.ip[pos++]-'0';
        }
        if (pos==start || octets[part]>255 || (pos-start>1 && s.ip[start]=='0') ||
            pos>=sizeof(s.ip) || s.ip[pos]!=(part==3?'\0':'.'))
            return "Vul een geldig unicast IPv4-adres in.";
        ++pos;
    }
    if (octets[0]==0 || octets[0]>=224) return "Gebruik een unicast IPv4-adres (geen multicast of broadcast).";
    s.video.seconds=10; /* A live stream has no preset duration. */
    return datv_validate(s.video,width,height);
}
int datv_test_options(int count, const char *const *values, DatvSettings *s, int *w, int *h) {
    *s=datv_defaults(); *w=320; *h=240;
    if (count<0 || count>6) return 0;
    int *fields[]={&s->bitrate,&s->seconds,&s->fps,&s->gop,w,h};
    for (int i=0; i<count; ++i) {
        char *end; errno=0;
        long n=strtol(values[i],&end,10);
        if (errno || !*values[i] || *end || n<1 || n>INT_MAX) return 0;
        *fields[i]=(int)n;
    }
    return datv_validate(*s,*w,*h)==NULL;
}
/* Level 1 Latin G0 invariant characters. National-option positions are
 * rejected rather than silently displaying another glyph on receivers. */
static int teletext_character(unsigned char c) {
    return c>=32 && c<=126 && !strchr("#@[\\]^_`{|}~",c);
}
const char *teletext_validate(const TeletextSettings *s) {
    if (s->enabled!=0 && s->enabled!=1) return "Ongeldige teletekstkeuze.";
    size_t n=0;
    while (n<sizeof(s->text) && s->text[n]) {
        if (!teletext_character((unsigned char)s->text[n++]))
            return "Gebruik voor teletekst letters zonder accenten, cijfers en eenvoudige leestekens.";
    }
    if (n!=0 && n!=TELETEXT_CELLS) return "Ongeldige teletekstpagina.";
    return NULL;
}
const char *teletext_from_text(TeletextSettings *s, const char *text) {
    TeletextSettings page={0}; page.enabled=s->enabled;
    memset(page.text,' ',TELETEXT_CELLS);
    int row=0,col=0;
    for (const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if (*p=='\r' && p[1]=='\n') continue;
        if (*p=='\n') { ++row; col=0; continue; }
        if (row>=TELETEXT_ROWS || col>=TELETEXT_COLUMNS)
            return "Gebruik maximaal 23 regels van elk 40 tekens; druk op Enter voor een nieuwe regel.";
        if (!teletext_character(*p))
            return "Gebruik voor teletekst letters zonder accenten, cijfers en eenvoudige leestekens.";
        page.text[row*TELETEXT_COLUMNS+col++]=(char)*p;
    }
    if (row>=TELETEXT_ROWS && !(row==TELETEXT_ROWS && !col))
        return "Gebruik maximaal 23 regels van elk 40 tekens.";
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
const char *datv_validate(DatvSettings s, int width, int height) {
    if (s.bitrate < DATV_MIN_BITRATE || s.bitrate > 2000000)
        return "TS-bitrate moet tussen 30080 en 2000000 bit/s liggen. Kies een beschikbare combinatie van systeem, symbolrate, FEC en pilots.";
    if (s.seconds < 1 || s.seconds > 60) return "Duur moet tussen 1 en 60 seconden liggen.";
    if (s.fps < 1 || s.fps > 25) return "Beeldfrequentie moet tussen 1 en 25 beelden/s liggen.";
    if (s.gop < 1 || s.gop > 250) return "GOP moet tussen 1 en 250 beelden liggen (1 = alleen IDR).";
    if (width < 16 || height < 16 || width > 640 || height > 480 || width % 2 || height % 2)
        return "Kies voor deze TS-proef een even resolutie van maximaal 640 x 480.";
    const char *tt_error=teletext_validate(&s.teletext);
    if (tt_error) return tt_error;
    if (s.teletext.enabled && s.bitrate<60000)
        return "Gebruik voor teletekst een TS-bitrate van minimaal 60000 bit/s.";
    if (s.eit_enabled!=0 && s.eit_enabled!=1) return "Ongeldige EIT-keuze.";
    if (s.eit_enabled) {
        const char *error=station_validate(&s.station);
        if (error) return error;
        wchar_t locator[13]={0}; size_t n=0;
        while (n<sizeof(s.locator) && s.locator[n]) ++n;
        if (n>12) return "Ongeldige EIT-locator.";
        for (size_t i=0;i<n;++i) locator[i]=(unsigned char)s.locator[i];
        if (n && !valid_locator(locator)) return "Ongeldige EIT-locator.";
    }
    return NULL;
}
const Resolution resolutions43[RESOLUTION_COUNT] = {
    {120,90},{160,120},{240,180},
    {320,240},{640,480},{800,600},{1024,768},
    {1080,810},{1280,960},{1600,1200},{1920,1440}
};
const Resolution resolutions169[RESOLUTION_COUNT] = {
    {120,68},{160,90},{240,136}, /* Even heights for H.264. */
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

const wchar_t contest_color_advice[] = L"DATV-contest: wit op zwart aanbevolen.";

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
    CONFIG_INT_FIELD(ebu_top,0,1), CONFIG_INT_FIELD(ebu_bottom,0,1),
    CONFIG_INT_FIELD(show_sum,0,1), CONFIG_INT_FIELD(top_code,0,1), CONFIG_INT_FIELD(genius,1,2),
    CONFIG_INT_FIELD(ts.bitrate,DATV_MIN_BITRATE,2000000), CONFIG_INT_FIELD(ts.seconds,1,60),
    CONFIG_INT_FIELD(ts.fps,1,25), CONFIG_INT_FIELD(ts.gop,1,250),
    CONFIG_INT_FIELD(ts.eit_enabled,0,1), CONFIG_INT_FIELD(udp.video.eit_enabled,0,1),
    CONFIG_INT_FIELD(teletext.enabled,0,1), CONFIG_TEXT_FIELD(teletext.text),
    CONFIG_TEXT_FIELD(station.city), CONFIG_TEXT_FIELD(station.description), CONFIG_TEXT_FIELD(station.operator_name),
    CONFIG_TEXT_FIELD(udp.ip), CONFIG_INT_FIELD(udp.port,1,65535),
    CONFIG_INT_FIELD(udp.video.bitrate,DATV_MIN_BITRATE,2000000), CONFIG_INT_FIELD(udp.video.fps,1,25),
    CONFIG_INT_FIELD(udp.video.gop,1,250),
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
            return "Gebruik geldige tekst op een regel voor de EIT-stationinformatie.";
        size_t count=0;
        const unsigned char *p=(const unsigned char *)fields[i];
        while (*p) {
            unsigned c=*p++;
            if (c>=0xf0 || (c==0xc2 && *p>=0x80 && *p<=0x9f))
                return "EIT ondersteunt deze tekens niet. Gebruik tekst zonder emoji of besturingstekens.";
            if ((c&0xc0)!=0x80) ++count;
        }
        if (count>limits[i]) return "Gebruik maximaal 40 tekens voor stad/operatornaam en 240 voor stationsomschrijving.";
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
            size_t limit=f->offset==offsetof(AppConfig,teletext.text)?TELETEXT_CELLS:f->size==16?15:(f->size-1)/4;
            if (characters>limit) return 0;
        }
        else if (*(const int *)value<f->min || *(const int *)value>f->max) return 0;
    }
    if (teletext_validate(&s->teletext)) return 0;
    if (station_validate(&s->station)) return 0;
    DatvUdpSettings udp=s->udp;
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
int config_load(const char *path, AppConfig *out) {
    FILE *f=config_open(path,0);
    if (!f) return errno==ENOENT?0:-1;
    AppConfig s=config_defaults(); uint64_t seen=0;
    int version=0, ok=1, migrated_ts=0, migrated_udp=0; char line[1200];
    while (fgets(line,sizeof(line),f)) {
        size_t n=strlen(line);
        if (!n || (line[n-1]!='\n' && !feof(f))) { ok=0; break; }
        while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n]=0;
        if (!n || line[0]=='#') continue;
        char *value=strchr(line,'=');
        if (!value) { ok=0; break; } *value++=0;
        if (!strcmp(line,"version")) {
            if (version || (strcmp(value,"1") && strcmp(value,"2") && strcmp(value,"3") && strcmp(value,"4") && strcmp(value,"5"))) { ok=0; break; }
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
            config_fields[i].offset==offsetof(AppConfig,ebu_bottom)) required &= ~(UINT64_C(1)<<i);
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
    if (!ok || !version || (seen&required)!=required || !config_valid(&s)) return -1;
    /* Version 1 stored 240px at index 10; version 2 sorts by width. */
    if (version==1) {
        if (s.resolution==10) s.resolution=2;
        else if (s.resolution>=2) ++s.resolution;
    }
    *out=s; return 1;
}
int config_save(const char *path, const AppConfig *s) {
    if (!config_valid(s)) return 0;
    size_t n=strlen(path)+5; char *temporary=malloc(n);
    if (!temporary) return 0;
    snprintf(temporary,n,"%s.tmp",path);
    FILE *f=config_open(temporary,1);
    if (!f) { free(temporary); return 0; }
    int ok=fprintf(f,"# ATV contestnummer generator\nversion=5\n")>=0;
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
