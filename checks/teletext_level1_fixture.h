/* Shared transmitted fixture; expected behavior is checked independently. */
static void teletext_level1_fixture(TeletextEditor *e) {
    const unsigned char held[]={22,127,30,26,31,127,25,127};
    const unsigned char separated[]={22,26,127,30,25,31,127};
    const unsigned char hidden[]={7,24,'H','I',1,'R',8,'F',9,'S'};
    const unsigned char height[]={4,29,7,'N',13,'A',22,127,12,'Z'};
    const unsigned char reset[]={22,127,30,7,22,29,127,12,29,13,29,12,29};
    const unsigned char flash[]={22,127,8,30,9,31,127};
    memcpy(e->raw+6*40,held,sizeof(held));
    memcpy(e->raw+7*40,separated,sizeof(separated));
    memcpy(e->raw+8*40,hidden,sizeof(hidden));
    memcpy(e->raw+9*40,height,sizeof(height));
    memcpy(e->raw+10*40,"THIS LOWER ROW MUST BE IGNORED",sizeof("THIS LOWER ROW MUST BE IGNORED")-1);
    memcpy(e->raw+11*40,reset,sizeof(reset));
    memcpy(e->raw+13*40,flash,sizeof(flash));
}
