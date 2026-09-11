#include "core.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(valid_call(L"PE1ITR"));
    assert(valid_call(L"DL/PE1ITR/P"));
    assert(!valid_call(L"PE1ITR/../../"));
    assert(!valid_call(L"CON"));
    assert(!valid_call(L"PE1 ITR"));
    assert(valid_locator(L"JO21"));
    assert(valid_locator(L"JO21QK"));
    assert(valid_locator(L"JO21QK86"));
    assert(valid_locator(L"JO21QK86DV"));
    assert(!valid_locator(L"SO21QK"));
    assert(!valid_locator(L"JO21YK"));
    assert(!valid_locator(L"JO21Q"));
    assert(!valid_locator(L"JOAA"));
    assert(valid_code(L"0001"));
    assert(valid_code(L"2222"));
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
    assert(!generated_code_valid(1234));
    assert(!generated_code_valid(2222));
    assert(generated_code_valid(1957));
    for (int i=0; i<RESOLUTION_COUNT; ++i) {
        assert(resolutions43[i].width*3 == resolutions43[i].height*4);
        /* 120 pixels wide needs a rounded height of 68 instead of 67.5. */
        assert(resolutions169[i].height == (resolutions169[i].width*9+8)/16);
    }
    wchar_t filename[25];
    filename_call(filename, L"DL/PE1ITR/P");
    assert(wcscmp(filename, L"DL_PE1ITR_P") == 0);
    printf("Core checks passed; %u valid automatic codes.\n", count);
    return 0;
}
