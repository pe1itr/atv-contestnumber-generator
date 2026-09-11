#include "pm5544.h"
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
static uint32_t *backgrounds[2];

static uint32_t *decode(int wide) {
    if (backgrounds[wide]) return backgrounds[wide];
    size_t count = wide ? 1280u*720u : 720u*576u;
    size_t length = wide ? sizeof(pm169) : sizeof(pm43);
    const unsigned char *data = wide ? pm169 : pm43;
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
    backgrounds[wide] = pixels;
    return pixels;
}

static void label(uint32_t *pixels, int width, int height, int source_width, int source_height,
                  int left, int top, int right, int bottom, const wchar_t *text) {
    /* Keep a border inside the original black panel; never alter the surrounding pattern. */
    int x0 = (left+3)*width/source_width, x1 = (right-3)*width/source_width;
    int y0 = (top+4)*height/source_height, y1 = (bottom-4)*height/source_height;
    size_t length = wcslen(text);
    if (!length || length > 24) return;
    int cells = (int)length*6-1;
    double scale = (double)(y1-y0)/7;
    if (scale > (double)(x1-x0)/cells) scale = (double)(x1-x0)/cells;
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
        label(pixels, width, height, sw, sh, 533,67,747,120, call);
        label(pixels, width, height, sw, sh, 480,547,800,600, locator);
    } else {
        label(pixels, width, height, sw, sh, 279,51,441,95, call);
        label(pixels, width, height, sw, sh, 237,439,483,482, locator);
    }
    return 1;
}

void pm5544_cleanup(void) {
    free(backgrounds[0]); free(backgrounds[1]);
    backgrounds[0] = backgrounds[1] = NULL;
}
