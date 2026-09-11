#include "core.h"
#include "datv.h"
#include <stdlib.h>
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
    {1080,810},{1280,960},{1600,1200},{1920,1440}
};
const Resolution resolutions169[RESOLUTION_COUNT] = {
    {120,68},{160,90},
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
int valid_code(const wchar_t *s) {
    return wcslen(s) == 4 && digit(s[0]) && digit(s[1]) && digit(s[2]) && digit(s[3]);
}
int code_digit_sum(const wchar_t *s) {
    if (!valid_code(s)) return -1;
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
