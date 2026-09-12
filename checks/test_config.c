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
    /* Older complete config files omit the two new optional strip fields. */
    original=config_defaults(); original.ebu_top=1; original.ebu_bottom=1;
    assert(config_save(TEST_PATH,&original));
    char old_config[8192]="", line[1024];
    f=fopen(TEST_PATH,"rb"); assert(f);
    while (fgets(line,sizeof(line),f))
        if (strncmp(line,"ebu_top=",8) && strncmp(line,"ebu_bottom=",11)) strcat(old_config,line);
    assert(!fclose(f)); f=fopen(TEST_PATH,"wb"); assert(f);
    assert(fputs(old_config,f)>=0); assert(!fclose(f));
    assert(config_load(TEST_PATH,&loaded)==1 && !loaded.ebu_top && !loaded.ebu_bottom);
    /* Blank inputs and an unused UDP address can be saved before setup. */
    original=config_defaults(); assert(config_save(TEST_PATH,&original));
    assert(config_load(TEST_PATH,&loaded)==1); assert(!memcmp(&original,&loaded,sizeof(original)));
    /* Every old index, including appended 240px, retains its physical size. */
    const int legacy_width43[]={120,160,320,640,800,1024,1080,1280,1600,1920,240};
    const int legacy_width169[]={120,160,320,640,800,960,1024,1280,1600,1920,240};
    for (int aspect=0;aspect<2;++aspect) for (int index=0;index<11;++index) {
        original.aspect=aspect; original.resolution=index;
        original.ts.bitrate=115196; original.udp.video.bitrate=123607;
        assert(config_save(TEST_PATH,&original));
        FILE *legacy=fopen(TEST_PATH,"r+b"); assert(legacy);
        assert(!fseek(legacy,(long)strlen("# ATV contestnummer generator\nversion="),SEEK_SET));
        assert(fputc('1',legacy)!=EOF); assert(!fclose(legacy));
        assert(config_load(TEST_PATH,&loaded)==1);
        const Resolution *list=aspect?resolutions169:resolutions43;
        assert(list[loaded.resolution].width==(aspect?legacy_width169:legacy_width43)[index]);
        assert(loaded.ts.bitrate==115196 && loaded.udp.video.bitrate==123607);
        AppConfig migrated=loaded;
        assert(config_save(TEST_PATH,&migrated)); assert(config_load(TEST_PATH,&loaded)==1);
        assert(!memcmp(&loaded,&migrated,sizeof(loaded))); /* No second migration. */
    }
    assert(!remove(TEST_PATH));
    puts("Config: roundtrip, defaults, ranges, malformed files and protected previous settings OK.");
    return 0;
}
