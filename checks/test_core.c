#include "core.h"
#include "datv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(!valid_fubk_locator(L"JO21"));
    assert(valid_fubk_locator(L"JO21QK"));
    assert(valid_fubk_locator(L"JO21QK86DV12"));
    assert(!valid_fubk_locator(L"JO21ZZ"));
    DatvUdpSettings udp=datv_udp_defaults();
    assert(udp.port==10000 && udp.video.fps==10 && udp.video.gop==2);
    DatvUdpStatus status={0}; char message[512];
    status.state=DATV_RUNNING; status.refusals=3;
    status.last_refusal_seconds=10; status.seconds=12;
    datv_udp_status_text(udp,status,message,sizeof(message));
    assert(strstr(message,"closed UDP port"));
    assert(!strstr(message,"Poortweigeringen gemeld:"));
    status.seconds=13;
    datv_udp_status_text(udp,status,message,sizeof(message));
    assert(strstr(message,"UDP output active") && !strstr(message,"closed UDP port"));
    status.last_refusal_seconds=13;
    datv_udp_status_text(udp,status,message,sizeof(message));
    assert(strstr(message,"closed UDP port"));
    status.state=DATV_STOPPED;
    datv_udp_status_text(udp,status,message,sizeof(message));
    assert(!strstr(message,"closed UDP port"));
    status.state=DATV_FAILED; strcpy(status.error,"test netwerkfout");
    datv_udp_status_text(udp,status,message,sizeof(message));
    assert(!strcmp(message,"test netwerkfout"));
    datv_udp_status_text(udp,status,NULL,0);
    assert(datv_udp_validate(udp,160,120));
    const char *bad_ips[]={"", "1.2.3", "1.2.3.256", "01.2.3.4", "1.2.3.4x", "0.0.0.0", "224.0.0.1", "255.255.255.255", "host.local"};
    for (unsigned i=0;i<sizeof(bad_ips)/sizeof(bad_ips[0]);++i) {
        snprintf(udp.ip,sizeof(udp.ip),"%s",bad_ips[i]); assert(datv_udp_validate(udp,160,120));
    }
    snprintf(udp.ip,sizeof(udp.ip),"127.0.0.1"); assert(!datv_udp_validate(udp,160,120));
    udp.port=0; assert(datv_udp_validate(udp,160,120));
    udp.port=65536; assert(datv_udp_validate(udp,160,120));
    udp.port=10000; udp.video.fps=26; assert(datv_udp_validate(udp,160,120));
    DatvSettings settings=datv_defaults();
    assert(!datv_validate(settings,320,240));
    for (int i=0;i<RESOLUTION_COUNT;++i) {
        assert(!datv_validate(settings,resolutions43[i].width,resolutions43[i].height));
        assert(!datv_validate(settings,resolutions169[i].width,resolutions169[i].height));
    }
    assert(datv_validate(settings,1922,1080));
    assert(datv_validate(settings,1920,1442));
    assert(datv_validate(settings,641,480));
    assert(datv_validate(settings,120,67));
    settings.bitrate=30080; assert(!datv_validate(settings,320,240));
    settings.bitrate=30079; assert(datv_validate(settings,320,240));
    const char *args[]={"60000","10","2","1","160","120"}; int width,height;
    assert(datv_test_options(6,args,&settings,&width,&height));
    assert(settings.bitrate==60000 && settings.gop==1 && width==160 && height==120);
    const char *bad[]={"999999999999999999999999999999"};
    assert(!datv_test_options(1,bad,&settings,&width,&height));
    assert(valid_call(L"PE1ITR"));
    assert(valid_call(L"DL/PE1ITR/P"));
    assert(!valid_call(L"PE1ITR/../../"));
    assert(!valid_call(L"CON"));
    assert(!valid_call(L"PE1 ITR"));
    assert(valid_locator(L"JO21"));
    assert(valid_locator(L"JO21QK"));
    assert(valid_locator(L"JO21QK86"));
    assert(valid_locator(L"JO21QK86DV"));
    assert(valid_locator(L"JO21QK86DW12"));
    assert(!valid_locator(L"JO21QK86DW1"));
    assert(!valid_locator(L"JO21QK86DWAB"));
    assert(!valid_locator(L"JO21QK86DW12AB"));
    wchar_t short_locator[7], previous_square[7] = L"";
    filename_locator(short_locator, L"JO21QK86DW12");
    assert(!wcscmp(short_locator, L"JO21QK"));
    filename_locator(short_locator, L"JO21");
    assert(!wcscmp(short_locator, L"JO21"));
    filename_locator(short_locator, L"../BAD");
    assert(!short_locator[0]);
    assert(!locator_square_changed(previous_square, L"JO21QK86DW12"));
    assert(!locator_square_changed(previous_square, L"JO21QK99AA99"));
    assert(!locator_square_changed(previous_square, L"JO21Q"));
    assert(!locator_square_changed(previous_square, L"JO21"));
    assert(locator_square_changed(previous_square, L"JO22QK86DW12"));
    assert(locator_square_changed(previous_square, L"JO21QK86DW12"));
    assert(!valid_locator(L"SO21QK"));
    assert(!valid_locator(L"JO21YK"));
    assert(!valid_locator(L"JO21Q"));
    assert(!valid_locator(L"JOAA"));
    assert(valid_code(L"0001"));
    assert(!valid_code(L"2222"));
    assert(!valid_code(L"0000"));
    assert(!valid_code(L"9999"));
    assert(!valid_code(L"4567"));
    assert(!valid_code(L"5432"));
    assert(!valid_code(L"0123"));
    assert(!valid_code(L"9876"));
    assert(!valid_code(L"3210"));
    assert(valid_code(L"1122"));
    assert(valid_code(L"0195"));
    assert(valid_code(L"1248"));
    assert(!valid_code(L"123"));
    assert(!valid_code(L"12345"));
    assert(!valid_code(L"12A4"));
    assert(code_digit_sum(L"1957") == 22);
    assert(code_digit_sum(L"0001") == 1);
    assert(code_digit_sum(L"0000") == 0);
    assert(code_digit_sum(L"9999") == 36);
    assert(code_digit_sum(L"12A4") == -1);
    assert(code_digit_sum(L"123") == -1);
    assert(code_digit_sum(L"----") == -1);
    unsigned count = 0;
    for (unsigned code=0; code<10000; ++code) if (generated_code_valid(code)) {
        int d[4] = {(int)code/1000, (int)code/100%10, (int)code/10%10, (int)code%10};
        assert(d[0] > 0);
        for (int i=0; i<4; ++i) {
            for (int j=0; j<i; ++j) assert(d[i] != d[j]);
            if (i) assert(d[i] != d[i-1]+1 && d[i] != d[i-1]-1);
        }
        ++count;
    }
    assert(count > 0);
    for (unsigned code=0; code<10000; ++code) {
        wchar_t text[5];
        swprintf(text,5,L"%04u",code);
        int prohibited = code % 1111 == 0;
        for (unsigned start=0; start<=6; ++start)
            prohibited |= code == start*1111+123 || code == (start+3)*1000+(start+2)*100+(start+1)*10+start;
        assert(valid_code(text) == !prohibited);
        if (generated_code_valid(code)) assert(valid_code(text));
    }
    assert(!generated_code_valid(1234));
    assert(!generated_code_valid(2222));
    assert(generated_code_valid(1957));
    for (int i=0; i<RESOLUTION_COUNT; ++i) {
        if (i) {
            assert(resolutions43[i-1].width<resolutions43[i].width);
            assert(resolutions169[i-1].width<resolutions169[i].width);
        }
        assert(resolutions43[i].width*3 == resolutions43[i].height*4);
        /* H.264 needs even heights, including 120x68 and 240x136. */
        assert(resolutions169[i].height == (resolutions169[i].width*9+16)/32*2);
    }
    wchar_t filename[25];
    filename_call(filename, L"DL/PE1ITR/P");
    assert(wcscmp(filename, L"DL_PE1ITR_P") == 0);
    printf("Core checks passed; %u valid automatic codes.\n", count);
    return 0;
}
