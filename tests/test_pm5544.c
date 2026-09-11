#include "core.h"
#include "pm5544.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    for (int wide=0; wide<2; ++wide) for (int i=0; i<RESOLUTION_COUNT; ++i) {
        Resolution r = (wide ? resolutions169 : resolutions43)[i];
        size_t size = (size_t)r.width*r.height;
        uint32_t *blank = malloc(size*sizeof(*blank)), *text = malloc(size*sizeof(*text));
        assert(blank && text);
        assert(pm5544_render(blank, r.width, r.height, L"", L""));
        const wchar_t *calls[] = {L"PE1ITR/P", L"DL/PE1ITR/ABCDEFGHIJKLMN"};
        for (int c=0; c<2; ++c) {
            assert(pm5544_render(text, r.width, r.height, calls[c], L"JO21QK86DV"));
            int top = 0, bottom = 0;
            int sw = wide ? 1280 : 720, sh = wide ? 720 : 576;
            int boxes[2][4] = {{wide?533:279, wide?67:51, wide?747:441, wide?120:95},
                               {wide?480:237, wide?547:439, wide?800:483, wide?600:482}};
            for (int y=0; y<r.height; ++y) for (int x=0; x<r.width; ++x) {
                if (blank[y*r.width+x] == text[y*r.width+x]) continue;
                assert(text[y*r.width+x] == 0x00ffffff);
                int inside = 0;
                for (int b=0; b<2; ++b) if (x >= boxes[b][0]*r.width/sw && x < boxes[b][2]*r.width/sw &&
                                           y >= boxes[b][1]*r.height/sh && y < boxes[b][3]*r.height/sh) {
                    inside = 1;
                    if (b) ++bottom; else ++top;
                }
                assert(inside); /* No changes to the test pattern outside the black name panels. */
            }
            assert(top > 0 && bottom > 0);
        }
        free(blank); free(text);
    }
    pm5544_cleanup();
    puts("PM5544: all resolutions, portable/long callsigns and panel boundaries passed.");
    return 0;
}
