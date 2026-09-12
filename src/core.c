#include "core.h"
#include "datv.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

DatvSettings datv_defaults(void) { return (DatvSettings){120000, 10, 5, 5}; }
DatvUdpSettings datv_udp_defaults(void) {
    DatvUdpSettings s={"",10000,{120000,10,10,2}};
    return s;
}
void datv_udp_status_text(DatvUdpSettings s, DatvUdpStatus status, char *text, size_t size) {
    if (status.state==DATV_FAILED) snprintf(text,size,"%s",status.error);
    else if (status.state==DATV_PREPARING) snprintf(text,size,"Beeld voorbereiden en bitrate controleren...");
    else if (status.state==DATV_STOPPED) snprintf(text,size,"UDP gestopt (%llu pakketten).",(unsigned long long)status.packets);
    else snprintf(text,size,"UDP-uitvoer actief naar %s:%d\n%d bit/s, %llu pakketten van 1316 bytes, QP %d.",
        s.ip,s.port,s.video.bitrate,(unsigned long long)status.packets,status.qp);
    if (size && (status.state==DATV_RUNNING || status.state==DATV_STOPPED) && status.refusals) {
        size_t used=strlen(text);
        if (used<size) snprintf(text+used,size-used,
            "\nPoortweigeringen gemeld: %llu. Controleer IPTS-ingang/poort; UDP bevestigt geen ontvangst.",
            (unsigned long long)status.refusals);
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
const char *datv_validate(DatvSettings s, int width, int height) {
    if (s.bitrate < 48000 || s.bitrate > 2000000)
        return "TS-bitrate moet tussen 48000 en 2000000 bit/s liggen.";
    if (s.seconds < 1 || s.seconds > 60) return "Duur moet tussen 1 en 60 seconden liggen.";
    if (s.fps < 1 || s.fps > 25) return "Beeldfrequentie moet tussen 1 en 25 beelden/s liggen.";
    if (s.gop < 1 || s.gop > 250) return "GOP moet tussen 1 en 250 beelden liggen (1 = alleen IDR).";
    if (width < 16 || height < 16 || width > 640 || height > 480 || width % 2 || height % 2)
        return "Kies voor deze TS-proef een even resolutie van maximaal 640 x 480.";
    return NULL;
}
const Resolution resolutions43[RESOLUTION_COUNT] = {
    {120,90},{160,120},
    {320,240},{640,480},{800,600},{1024,768},
    {1080,810},{1280,960},{1600,1200},{1920,1440},
    {240,180} /* Append to preserve resolution indices in saved settings. */
};
const Resolution resolutions169[RESOLUTION_COUNT] = {
    {120,68},{160,90},
    {320,180},{640,360},{800,450},{960,540},
    {1024,576},{1280,720},{1600,900},{1920,1080},
    {240,136} /* Even height required by the shared H.264 encoder. */
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

typedef struct { const char *key; size_t offset, size; int min, max; } ConfigField;
#define CONFIG_INT_FIELD(field, low, high) {#field, offsetof(AppConfig, field), 0, low, high}
#define CONFIG_TEXT_FIELD(field) {#field, offsetof(AppConfig, field), sizeof(((AppConfig *)0)->field), 0, 0}
static const ConfigField config_fields[]={
    CONFIG_TEXT_FIELD(call), CONFIG_TEXT_FIELD(locator), CONFIG_TEXT_FIELD(code),
    CONFIG_INT_FIELD(mode,0,1), CONFIG_INT_FIELD(automatic,0,1), CONFIG_INT_FIELD(aspect,0,1),
    CONFIG_INT_FIELD(resolution,0,RESOLUTION_COUNT-1), CONFIG_INT_FIELD(band,0,10),
    CONFIG_INT_FIELD(show,0,1), CONFIG_INT_FIELD(inverse,0,1), CONFIG_INT_FIELD(blue_yellow,0,1),
    CONFIG_INT_FIELD(show_sum,0,1), CONFIG_INT_FIELD(top_code,0,1), CONFIG_INT_FIELD(genius,1,2),
    CONFIG_INT_FIELD(ts.bitrate,48000,2000000), CONFIG_INT_FIELD(ts.seconds,1,60),
    CONFIG_INT_FIELD(ts.fps,1,25), CONFIG_INT_FIELD(ts.gop,1,250),
    CONFIG_TEXT_FIELD(udp.ip), CONFIG_INT_FIELD(udp.port,1,65535),
    CONFIG_INT_FIELD(udp.video.bitrate,48000,2000000), CONFIG_INT_FIELD(udp.video.fps,1,25),
    CONFIG_INT_FIELD(udp.video.gop,1,250)
};
#define CONFIG_FIELDS (sizeof(config_fields)/sizeof(config_fields[0]))
AppConfig config_defaults(void) {
    AppConfig s={0}; s.automatic=1; s.resolution=DEFAULT_RESOLUTION_INDEX;
    s.band=3; s.show=1; s.genius=1; s.ts=datv_defaults(); s.udp=datv_udp_defaults();
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
static int config_valid(const AppConfig *s) {
    for (size_t i=0;i<CONFIG_FIELDS;++i) {
        const ConfigField *f=&config_fields[i]; const char *value=(const char *)s+f->offset;
        if (f->size) {
            if (!config_text_valid(value,f->size)) return 0;
            size_t characters=0;
            for (const unsigned char *p=(const unsigned char *)value; *p; ++p)
                if ((*p&0xc0)!=0x80) ++characters;
            size_t limit=f->size==16?15:(f->size-1)/4;
            if (characters>limit) return 0;
        }
        else if (*(const int *)value<f->min || *(const int *)value>f->max) return 0;
    }
    DatvUdpSettings udp=s->udp;
    if (!udp.ip[0]) strcpy(udp.ip,"127.0.0.1"); /* An unused destination may be empty. */
    return !datv_udp_validate(udp,160,120);
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
    AppConfig s=config_defaults(); unsigned long seen=0;
    int version=0, ok=1; char line[256];
    while (fgets(line,sizeof(line),f)) {
        size_t n=strlen(line);
        if (!n || (line[n-1]!='\n' && !feof(f))) { ok=0; break; }
        while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n]=0;
        if (!n || line[0]=='#') continue;
        char *value=strchr(line,'=');
        if (!value) { ok=0; break; } *value++=0;
        if (!strcmp(line,"version")) {
            if (version || strcmp(value,"1")) { ok=0; break; }
            version=1; continue;
        }
        size_t i;
        for (i=0;i<CONFIG_FIELDS;++i) if (!strcmp(line,config_fields[i].key)) break;
        if (i==CONFIG_FIELDS || (seen&(1UL<<i))) { ok=0; break; }
        seen|=1UL<<i;
        const ConfigField *field=&config_fields[i]; char *target=(char *)&s+field->offset;
        if (field->size) {
            if (strlen(value)>=field->size) { ok=0; break; }
            strcpy(target,value);
        } else {
            char *end; errno=0; long v=strtol(value,&end,10);
            if (errno || !*value || *end || v<field->min || v>field->max) { ok=0; break; }
            *(int *)target=(int)v;
        }
    }
    if (ferror(f)) ok=0;
    if (fclose(f)) ok=0;
    if (!ok || !version || seen!=(1UL<<CONFIG_FIELDS)-1 || !config_valid(&s)) return -1;
    *out=s; return 1;
}
int config_save(const char *path, const AppConfig *s) {
    if (!config_valid(s)) return 0;
    size_t n=strlen(path)+5; char *temporary=malloc(n);
    if (!temporary) return 0;
    snprintf(temporary,n,"%s.tmp",path);
    FILE *f=config_open(temporary,1);
    if (!f) { free(temporary); return 0; }
    int ok=fprintf(f,"# ATV contestnummer generator\nversion=1\n")>=0;
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
