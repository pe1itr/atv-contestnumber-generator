#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define TEST_PATH "build/config-windows-test.conf"
#else
#define TEST_PATH "build/config-linux-test.conf"
#endif
static void append(const char *text) {
    FILE *f=fopen(TEST_PATH,"ab"); assert(f);
    assert(fputs(text,f)>=0); assert(!fclose(f));
}
int main(void) {
    AppConfig original=config_defaults(), loaded=config_defaults();
    assert(config_load(TEST_PATH,&loaded)==0);
    original.mode=1; original.automatic=0; original.aspect=1; original.resolution=9;
    original.band=10; original.show=0; original.inverse=1; original.blue_yellow=1;
    original.show_sum=1; original.top_code=1; original.genius=2;
    strcpy(original.call,"PE1ITR/P"); strcpy(original.locator,"JO21QK86DV12"); strcpy(original.code,"1957");
    original.ts=(DatvSettings){60000,60,2,1};
    strcpy(original.udp.ip,"192.168.1.50"); original.udp.port=12345;
    original.udp.video=(DatvSettings){240000,10,10,2};
    assert(config_save(TEST_PATH,&original)); assert(config_load(TEST_PATH,&loaded)==1);
    assert(!memcmp(&original,&loaded,sizeof(original)));
    /* Invalid saves preserve the earlier file. */
    AppConfig bad=original; bad.ts.seconds=61;
    assert(!config_save(TEST_PATH,&bad)); assert(config_load(TEST_PATH,&loaded)==1);
    assert(!memcmp(&original,&loaded,sizeof(original)));
    bad=original; strcpy(bad.udp.ip,"999.1.2.3"); assert(!config_save(TEST_PATH,&bad));
    bad=original; strcpy(bad.call,"PE1\nITR"); assert(!config_save(TEST_PATH,&bad));
    bad=original; strcpy(bad.call,"\xc0\x80"); assert(!config_save(TEST_PATH,&bad));
    bad=original; strcpy(bad.call,"1234567890123456789012345"); assert(!config_save(TEST_PATH,&bad));
    /* Malformed, duplicate, unknown and unsupported data never partially apply. */
    const char *invalid[]={"version=1\n","version=2\n","udp.port=0\n","udp.port=999999999999999999999999\n","unknown=1\n","broken\n"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        assert(config_save(TEST_PATH,&original)); append(invalid[i]);
        loaded=config_defaults(); AppConfig before=loaded;
        assert(config_load(TEST_PATH,&loaded)==-1); assert(!memcmp(&loaded,&before,sizeof(loaded)));
    }
    FILE *f=fopen(TEST_PATH,"wb"); assert(f); assert(fputs("version=1\ncall=PE1ITR\n",f)>=0); assert(!fclose(f));
    assert(config_load(TEST_PATH,&loaded)==-1);
    /* Blank inputs and an unused UDP address can be saved before setup. */
    original=config_defaults(); assert(config_save(TEST_PATH,&original));
    assert(config_load(TEST_PATH,&loaded)==1); assert(!memcmp(&original,&loaded,sizeof(original)));
    assert(!remove(TEST_PATH));
    puts("Config: roundtrip, defaults, ranges, malformed files and protected previous settings OK.");
    return 0;
}
