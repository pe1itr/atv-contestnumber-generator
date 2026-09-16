#include "core.h"
#include "pm5544.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    assert(!pm5644_render(NULL,320,240,L"",L""));
    const wchar_t *calls[]={L"PE1ITR/P",L"PA1234567890123456789012"};
    for (int wide=0; wide<2; ++wide) for (int i=0; i<RESOLUTION_COUNT; ++i) {
        Resolution r=(wide?resolutions169:resolutions43)[i];
        size_t bytes=(size_t)r.width*r.height*sizeof(uint32_t);
        uint32_t *blank=malloc(bytes), *text=malloc(bytes), *old=malloc(bytes);
        assert(blank && text && old);
        assert(pm5644_render(blank,r.width,r.height,L"",L""));
        assert(pm5544_render(old,r.width,r.height,L"",L""));
        assert(memcmp(blank,old,bytes)); /* Selection must not fall back to PM5544. */
        const int boxes43[2][4]={{285,55,434,97},{266,433,473,475}};
        const int boxes169[2][4]={{303,59,416,101},{274,437,445,479}};
        const int (*boxes)[4]=wide?boxes169:boxes43;
        for (unsigned c=0; c<sizeof(calls)/sizeof(calls[0]); ++c) {
            assert(pm5644_render(text,r.width,r.height,calls[c],L"JO21QK86DV12"));
            int changed[2]={0};
            for (int y=0; y<r.height; ++y) for (int x=0; x<r.width; ++x) {
                if (blank[y*r.width+x]==text[y*r.width+x]) continue;
                int panel=-1;
                for (int b=0; b<2; ++b)
                    if (x>=boxes[b][0]*r.width/720 && x<boxes[b][2]*r.width/720 &&
                        y>=boxes[b][1]*r.height/576 && y<boxes[b][3]*r.height/576) panel=b;
                assert(panel>=0); /* Pixel-exact preservation outside the name panels. */
                assert(text[y*r.width+x]==0xffffff);
                ++changed[panel];
            }
            if (!changed[0] || !changed[1]) fprintf(stderr,"PM5644 %dx%d call %u: changed %d/%d\n",r.width,r.height,c,changed[0],changed[1]);
            assert(changed[0] && changed[1]);
        }
        /* Releasing and rebuilding the cache must not change the pattern. */
        pm5544_cleanup();
        assert(pm5644_render(text,r.width,r.height,L"",L""));
        assert(!memcmp(blank,text,bytes));
        free(blank); free(text); free(old);
    }
    pm5544_cleanup();
    puts("PM5644: both ROM patterns, all resolutions, long labels, panel boundaries and cache reload OK.");
    return 0;
}
