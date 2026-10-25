#include "pm5544.h"
#include "core.h"
#include <string.h>
#include <stdlib.h>
#include "pm_assets.h"

/* Original 5 x 7 block lettering, embedded for identical rendering on both platforms. */
static const unsigned char glyphs[][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {15,16,16,16,16,16,15},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {15,16,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}, {1,2,2,4,8,8,16}
};
static uint32_t *backgrounds[6];

static uint32_t *decode(int index) {
    if (backgrounds[index]) return backgrounds[index];
    const size_t counts[] = {720u*576u, 1280u*720u, 768u*576u, 2000u*1125u, 720u*576u, 720u*576u};
    const size_t lengths[] = {sizeof(pm43), sizeof(pm169), sizeof(fubk43), sizeof(fubk169), sizeof(pm5644_43), sizeof(pm5644_169)};
    const unsigned char *const sources[] = {pm43, pm169, fubk43, fubk169, pm5644_43, pm5644_169};
    size_t count = counts[index], length = lengths[index];
    const unsigned char *data = sources[index];
    uint32_t *pixels = malloc(count*sizeof(*pixels));
    if (!pixels) return NULL;
    size_t offset = 0;
    for (size_t i=0; i+4<length; i+=5) {
        size_t run = data[i] | ((size_t)data[i+1]<<8);
        if (!run || run > count-offset) { free(pixels); return NULL; }
        uint32_t color = ((uint32_t)data[i+2]<<16) | ((uint32_t)data[i+3]<<8) | data[i+4];
        while (run--) pixels[offset++] = color;
    }
    if (offset != count) { free(pixels); return NULL; }
    backgrounds[index] = pixels;
    return pixels;
}

static void label(uint32_t *pixels, int width, int height, int source_width, int source_height,
                  int left, int top, int right, int bottom, const wchar_t *text, int min_cells) {
    /* Keep a border inside the original black panel; never alter the surrounding pattern. */
    int x0 = (left+3)*width/source_width, x1 = (right-3)*width/source_width;
    int y0 = (top+4)*height/source_height, y1 = (bottom-4)*height/source_height;
    size_t length = wcslen(text);
    if (!length || length > 24) return;
    int cells = (int)length*6-1;
    int fit_cells = cells>min_cells ? cells : min_cells;
    double scale = (double)(y1-y0)/7;
    if (scale > (double)(x1-x0)/fit_cells) scale = (double)(x1-x0)/fit_cells;
    int tw = (int)(cells*scale), th = (int)(7*scale);
    if (tw < 1 || y1 <= y0) return;
    if (th < 1) th = 1; /* Keep very long labels present even in tiny images. */
    x0 += (x1-x0-tw)/2;
    y0 += (y1-y0-th)/2;
    for (int y=0; y<th; ++y) for (int x=0; x<tw; ++x) {
        int cell = x*cells/tw, column = cell%6, row = y*7/th;
        wchar_t c = text[cell/6];
        int index = c >= L'0' && c <= L'9' ? c-L'0' :
                    c >= L'A' && c <= L'Z' ? c-L'A'+10 : c == L'/' ? 36 : -1;
        if (column < 5 && index >= 0 && (glyphs[index][row] & (1 << (4-column))))
            pixels[(y0+y)*width+x0+x] = 0x00ffffff;
    }
}

/* Original 7 x 11 lettering for motion: finer strokes at the same line height.
 * Embedded, shared by Linux and Windows; 0 has a slash, I has bars. */
static const unsigned char motion_glyphs[][11] = {
    {28,34,65,67,69,73,81,97,65,34,28}, /* 0 */
    {8,24,40,8,8,8,8,8,8,8,62}, /* 1 */
    {62,65,65,1,2,4,8,16,32,64,127}, /* 2 */
    {62,65,1,1,2,28,2,1,1,65,62}, /* 3 */
    {2,6,10,18,34,66,127,2,2,2,2}, /* 4 */
    {127,64,64,64,124,2,1,1,1,65,62}, /* 5 */
    {30,32,64,64,124,66,65,65,65,34,28}, /* 6 */
    {127,1,2,2,4,4,8,8,16,16,16}, /* 7 */
    {62,65,65,65,34,28,34,65,65,65,62}, /* 8 */
    {28,34,65,65,65,63,1,1,1,2,60}, /* 9 */
    {8,20,34,65,65,65,127,65,65,65,65}, /* A */
    {126,65,65,65,66,124,66,65,65,65,126}, /* B */
    {62,65,64,64,64,64,64,64,64,65,62}, /* C */
    {124,66,65,65,65,65,65,65,65,66,124}, /* D */
    {127,64,64,64,64,124,64,64,64,64,127}, /* E */
    {127,64,64,64,64,124,64,64,64,64,64}, /* F */
    {62,65,64,64,64,79,65,65,65,65,62}, /* G */
    {65,65,65,65,65,127,65,65,65,65,65}, /* H */
    {62,8,8,8,8,8,8,8,8,8,62}, /* I */
    {31,2,2,2,2,2,2,2,66,66,60}, /* J */
    {65,66,68,72,80,96,80,72,68,66,65}, /* K */
    {64,64,64,64,64,64,64,64,64,64,127}, /* L */
    {65,99,85,73,65,65,65,65,65,65,65}, /* M */
    {65,97,97,81,81,73,69,69,67,67,65}, /* N */
    {62,65,65,65,65,65,65,65,65,65,62}, /* O */
    {126,65,65,65,65,126,64,64,64,64,64}, /* P */
    {62,65,65,65,65,65,65,73,69,66,61}, /* Q */
    {126,65,65,65,65,126,72,68,66,65,65}, /* R */
    {62,65,64,64,32,28,2,1,1,65,62}, /* S */
    {127,8,8,8,8,8,8,8,8,8,8}, /* T */
    {65,65,65,65,65,65,65,65,65,34,28}, /* U */
    {65,65,65,65,65,34,34,34,20,20,8}, /* V */
    {65,65,65,65,65,73,73,73,85,85,34}, /* W */
    {65,65,34,34,20,8,20,34,34,65,65}, /* X */
    {65,65,34,34,20,8,8,8,8,8,8}, /* Y */
    {127,1,2,2,4,8,16,32,32,64,127}, /* Z */
    {1,1,2,2,4,8,16,32,32,64,64}, /* / */
};

static void motion_label(uint32_t *pixels, int top, int bottom, const wchar_t *text) {
    size_t length=wcslen(text);
    if (!length || length>24) return;
    int cells=(int)length*8-1;
    /* Keep every character present for unusually long callsigns: the compact
     * original alphabet still fits 24 characters without dropping columns. */
    if (cells>154) {
        label(pixels,160,120,160,120,0,top,160,bottom,text,0);
        return;
    }
    int th=bottom-top-8, tw=cells*th/11;
    if (tw>154) { tw=154; th=11*tw/cells; }
    if (th<1 || tw<1) return;
    int x0=(160-tw)/2, y0=top+(bottom-top-th)/2;
    for (int y=0;y<th;++y) for (int x=0;x<tw;++x) {
        int cell=x*cells/tw, column=cell%8, row=y*11/th;
        wchar_t c=text[cell/8];
        int index=c>=L'0' && c<=L'9'?c-L'0':c>=L'A' && c<=L'Z'?c-L'A'+10:c==L'/'?36:-1;
        if (column<7 && index>=0 && (motion_glyphs[index][row]&(1<<(6-column)))) {
            pixels[(y0+y)*160+x0+x]=0xffffff;
            /* Medium weight: add one output pixel, not a whole glyph cell.
             * Keep compact labels intact and preserve the inter-letter gap. */
            if (th>=14) {
                int next=(x+1)*cells/tw;
                if (x+1<tw && next/8==cell/8 && next%8<7)
                    pixels[(y0+y)*160+x0+x+1]=0xffffff;
                if (y+1<th) pixels[(y0+y+1)*160+x0+x]=0xffffff;
            }
        }
    }
}

int motion_render(uint32_t *pixels, const wchar_t *call, const wchar_t *code,
                  const wchar_t *locator, unsigned frame) {
    if (!pixels) return 0;
    memset(pixels,0,160*120*sizeof(*pixels));
    motion_label(pixels,0,30,call);
    motion_label(pixels,28,60,code);
    motion_label(pixels,58,80,locator);
    unsigned char y[160*120];
    motion_luma(y,frame);
    for (int i=160*80;i<160*120;++i) pixels[i]=y[i]>128?0xffffff:0;
    return 1;
}

int pm5544_render(uint32_t *pixels, int width, int height,
                  const wchar_t *call, const wchar_t *locator) {
    if (!pixels || width <= 0 || height <= 0) return 0;
    int wide = width*3 != height*4;
    int sw = wide ? 1280 : 720, sh = wide ? 720 : 576;
    uint32_t *source = decode(wide);
    if (!source) return 0;
    /* Nearest-neighbour scaling preserves the hard edges of the supplied test patterns. */
    for (int y=0; y<height; ++y) for (int x=0; x<width; ++x)
        pixels[y*width+x] = source[(y*sh/height)*sw+x*sw/width];
    if (wide) {
        label(pixels, width, height, sw, sh, 533,67,747,120, call, 0);
        label(pixels, width, height, sw, sh, 480,547,800,600, locator, 0);
    } else {
        label(pixels, width, height, sw, sh, 279,51,441,95, call, 0);
        label(pixels, width, height, sw, sh, 237,439,483,482, locator, 0);
    }
    return 1;
}

int fubk_render(uint32_t *pixels, int width, int height,
                const wchar_t *call, const wchar_t *locator) {
    if (!pixels || width<=0 || height<=0) return 0;
    int wide = width*3 != height*4;
    int sw = wide ? 2000 : 768, sh = wide ? 1125 : 576;
    uint32_t *source = decode(2+wide);
    if (!source) return 0;
    for (int y=0; y<height; ++y) for (int x=0; x<width; ++x)
        pixels[y*width+x] = source[(y*sh/height)*sw+x*sw/width];
    wchar_t short_locator[7] = {0};
    for (int i=0; i<6 && locator[i]; ++i) short_locator[i]=locator[i];
    int longest = (int)wcslen(call);
    if (longest<6) longest=6;
    int min_cells = longest*6-1;
    /* Keep the central white divider and all surrounding test elements intact. */
    if (wide) {
        label(pixels,width,height,sw,sh,545,565,999,638,call,min_cells);
        label(pixels,width,height,sw,sh,1002,565,1455,638,short_locator,min_cells);
    } else {
        label(pixels,width,height,sw,sh,240,290,382,327,call,min_cells);
        label(pixels,width,height,sw,sh,386,290,528,327,short_locator,min_cells);
    }
    return 1;
}

/* Both ROM-derived rasters are 720x576; display aspect selects G00/G924.
 * Variant 2 preserves the grid without unused date/time insert boxes. */
int pm5644_render(uint32_t *pixels, int width, int height,
                  const wchar_t *call, const wchar_t *locator) {
    if (!pixels || width <= 0 || height <= 0) return 0;
    int wide = width*3 != height*4;
    uint32_t *source = decode(4+wide);
    if (!source) return 0;
    for (int y=0; y<height; ++y) for (int x=0; x<width; ++x)
        pixels[y*width+x] = source[(y*576/height)*720+x*720/width];
    if (wide) {
        label(pixels,width,height,720,576,303,59,416,101,call,0);
        label(pixels,width,height,720,576,274,437,445,479,locator,0);
    } else {
        label(pixels,width,height,720,576,285,55,434,97,call,0);
        label(pixels,width,height,720,576,266,433,473,475,locator,0);
    }
    return 1;
}

void pm5544_cleanup(void) {
    for (int i=0; i<6; ++i) { free(backgrounds[i]); backgrounds[i]=NULL; }
}
