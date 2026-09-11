#include "core.h"
#include <stdlib.h>
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
    if (n < 4 || n > 10 || n % 2) return 0;
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
