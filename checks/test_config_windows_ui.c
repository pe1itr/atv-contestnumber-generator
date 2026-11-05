#include "../src/main.c"
#include <assert.h>
static void screenshot_teletext(HWND window) {
    RECT r; GetClientRect(window,&r); HDC dc=GetDC(window),mem=CreateCompatibleDC(dc);
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=r.right; info.bmiHeader.biHeight=-r.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void *pixels; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); assert(bitmap);
    HGDIOBJ old=SelectObject(mem,bitmap);
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
    assert(BitBlt(mem,0,0,r.right,r.bottom,dc,0,0,SRCCOPY));
    BITMAPFILEHEADER header={0}; header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+r.right*r.bottom*4;
    FILE *file=fopen("build/teletext-windows-editor.bmp","wb"); assert(file);
    assert(fwrite(&header,sizeof(header),1,file)==1);
    assert(fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,file)==1);
    assert(fwrite(pixels,(size_t)r.right*r.bottom*4,1,file)==1); assert(!fclose(file));
    SelectObject(mem,old); DeleteObject(bitmap); DeleteDC(mem); ReleaseDC(window,dc);
}
static BOOL CALLBACK apply_teletext(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[80]; GetWindowTextW(window,title,80);
    if (wcscmp(title,L"Teletext pages")) return TRUE;
    if (GetPropW(window,L"teletext-tested")) {
        screenshot_teletext(window); PostMessageW(window,WM_COMMAND,IDOK,0); return TRUE;
    }
    SetPropW(window,L"teletext-tested",(HANDLE)1);
    TeletextDialog *d=(TeletextDialog *)GetWindowLongPtrW(window,DWLP_USER); assert(d);
    const wchar_t *text=L"ATV CONTEST";
    for (const wchar_t *c=text;*c;++c) SendMessageW(d->area,WM_CHAR,*c,0);
    SendMessageW(window,WM_COMMAND,TT_ADD,0); assert(d->selected==1);
    SetDlgItemTextW(window,TT_HEADER,L"IMPORTED PAGE");
    assert(tt_win_import_path(d,"examples/teletext/atv-station.tti")); assert(d->editor.raw[120]==22);
    TeletextEditor loaded=d->editor;
    assert(!tt_win_import_path(d,"build/nonexistent-page.tti"));
    assert(!memcmp(loaded.raw,d->editor.raw,sizeof(loaded.raw)));
    teletext_editor_undo(&d->editor,&d->undo); assert(d->editor.raw[120]==' ');
    teletext_editor_undo(&d->editor,&d->undo); assert(d->editor.raw[120]==22);
    SendDlgItemMessageW(window,TT_COLOR,CB_SETCURSEL,3,0); /* Blue, while active text is white. */
    d->editor.cursor=14*40; SendMessageW(window,WM_COMMAND,TT_BACKGROUND,0);
    assert(d->editor.cursor==14*40+3);
    assert(teletext_editor_cell(&d->editor,14*40+5).background==4);
    assert(teletext_editor_cell(&d->editor,14*40+5).foreground==7);
    assert(d->editor.raw[14*40+5]=='W');
    d->editor.cursor=40;
    SendDlgItemMessageW(window,TT_ATTRIBUTE,CB_SETCURSEL,1,0);
    SendMessageW(window,WM_COMMAND,TT_INSERT,0);
    assert(d->editor.raw[40]==13 && d->editor.cursor==41);
    SendMessageW(d->area,WM_CHAR,'B',0);
    assert(teletext_editor_cell(&d->editor,81).lower);
    CheckDlgButton(window,TT_REVEAL,BST_CHECKED);
    SendMessageW(window,WM_COMMAND,TT_REVEAL,0); assert(d->editor.reveal);

    RECT r; GetClientRect(d->area,&r);
    int left,top,pw,ph; teletext_viewport(r.right,r.bottom,&left,&top,&pw,&ph);
    SendMessageW(d->area,WM_LBUTTONDOWN,0,MAKELPARAM(left+20*pw/40+3,top+ph/24+3*(ph-ph/24)/23+3));
    assert(!tt_win_store(d));
    SendDlgItemMessageW(window,TT_PAGE,CB_SETCURSEL,0,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(TT_PAGE,CBN_SELCHANGE),0);
    assert(d->selected==0 && d->editor.raw[0]=='A' && d->editor.reveal);
    SendDlgItemMessageW(window,TT_PAGE,CB_SETCURSEL,1,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(TT_PAGE,CBN_SELCHANGE),0);
    assert(d->selected==1);
    SendDlgItemMessageW(window,TT_INTERVAL,CB_SETCURSEL,1,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(TT_INTERVAL,CBN_SELCHANGE),0);
    assert(d->book.interval==5);
    return TRUE;
}
static void CALLBACK teletext_timer(HWND w,UINT m,UINT_PTR id,DWORD t) {
    (void)w; (void)m; (void)id; (void)t; EnumThreadWindows(GetCurrentThreadId(),apply_teletext,0);
}
static BOOL CALLBACK apply_udp(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[80]; GetWindowTextW(window,title,80);
    if (wcscmp(title,L"DATV: UDP output")) return TRUE;
    SetDlgItemTextW(window,IDC_UDP_IP,L"192.168.1.50");
    SetDlgItemInt(window,IDC_UDP_PORT,12345,FALSE);
    SendDlgItemMessageW(window,IDC_DVB_SYSTEM,CB_SETCURSEL,DVB_T,0);
    SendDlgItemMessageW(window,IDC_DVB_BW,CB_SETCURSEL,4,0);
    SendDlgItemMessageW(window,IDC_DVB_FEC,CB_SETCURSEL,2,0);
    SendDlgItemMessageW(window,IDC_DVB_GUARD,CB_SETCURSEL,1,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_SYSTEM,CBN_SELCHANGE),0);
    SetDlgItemInt(window,IDC_UDP_FPS,12,FALSE); SetDlgItemInt(window,IDC_UDP_GOP,3,FALSE);
    CheckDlgButton(window,IDC_INCLUDE_EIT,BST_CHECKED);
    CheckDlgButton(window,IDC_INCLUDE_TELETEXT,BST_CHECKED);
    PostMessageW(window,WM_COMMAND,IDC_UDP_APPLY,0); return TRUE;
}
static void CALLBACK drive_timer(HWND w,UINT m,UINT_PTR id,DWORD t) {
    (void)w; (void)m; (void)id; (void)t; EnumThreadWindows(GetCurrentThreadId(),apply_udp,0);
}
static int motion_system;
static BOOL CALLBACK apply_motion_udp(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[100]; GetWindowTextW(window,title,100);
    if (wcsncmp(title,L"Motion challenge:",17)) return TRUE;
    SendDlgItemMessageW(window,IDC_DVB_SYSTEM,CB_SETCURSEL,motion_system,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_SYSTEM,CBN_SELCHANGE),0);
    SendDlgItemMessageW(window,IDC_DVB_SR,CB_SETCURSEL,dvb_symbol_rate_row(2),0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_SR,CBN_SELCHANGE),0);
    CheckDlgButton(window,IDC_DVB_PILOTS,motion_system==DVB_S2?BST_CHECKED:BST_UNCHECKED);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_PILOTS,BN_CLICKED),0);
    SetDlgItemInt(window,IDC_UDP_GOP,5,FALSE);
    assert(GetDlgItemInt(window,IDC_UDP_BITRATE,NULL,FALSE)==(motion_system==DVB_S?115196u:120665u));
    PostMessageW(window,WM_COMMAND,IDC_UDP_APPLY,0); return TRUE;
}
static void CALLBACK motion_timer(HWND w,UINT m,UINT_PTR id,DWORD t) {
    (void)w; (void)m; (void)id; (void)t; EnumThreadWindows(GetCurrentThreadId(),apply_motion_udp,0);
}
static HWND open_app(void) {
    HWND w=CreateDialogParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_MAIN),NULL,dialog,0);
    assert(w); return w;
}
int wmain(void) {
    assert(SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED)));
    assert(SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,(void **)&factory)));
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    char *path=config_path(); assert(path); AppConfig scratch;
    assert(config_load(path,&scratch)==0); /* Never overwrite a pre-existing config. */
    HWND w=open_app(); AppConfig initial=capture_config(w); assert(initial.genius==1 && !initial.call[0]);
    AppConfig wanted=config_defaults();
    strcpy(wanted.call,"PE1ITR/P"); strcpy(wanted.locator,"JO21QK86DV12"); strcpy(wanted.code,"1957");
    strcpy(wanted.station.city,"Eindhoven"); strcpy(wanted.station.operator_name,"René");
    strcpy(wanted.station.description,"70 cm ATV-station");
    wanted.aspect=1; wanted.resolution=1; wanted.band=10; wanted.genius=2;
    wanted.show=0; wanted.inverse=1; wanted.blue_yellow=1; wanted.show_sum=1; wanted.top_code=1; wanted.ebu_top=1; wanted.ebu_bottom=1;
    wanted.ts=(DatvSettings){.bitrate=60000,.seconds=30,.fps=2,.gop=1,.eit_enabled=1}; apply_config(w,&wanted);
    UINT_PTR timer=SetTimer(NULL,0,100,drive_timer); assert(timer);
    SendMessageW(w,WM_COMMAND,IDM_UDP,0); KillTimer(NULL,timer);
    assert(udp_settings.port==12345 && udp_settings.video.fps==12 && udp_settings.video.gop==3);
    timer=SetTimer(NULL,0,100,teletext_timer); assert(timer);
    SendMessageW(w,WM_COMMAND,IDM_TELETEXT,0); KillTimer(NULL,timer);
    assert(teletext_settings.enabled && teletext_byte(&teletext_settings,0)=='A' && teletext_settings.extra_count==1);
    wanted.teletext=teletext_settings;
    wanted.udp=udp_settings; wanted.udp_dvb=udp_dvb;
    assert(wanted.udp_dvb.system==DVB_T && wanted.udp_dvb.bandwidth_khz==333 && wanted.udp_dvb.guard==1);
    SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); AppConfig actual=capture_config(w);
    assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)!=(UINT)-1);
    assert(GetWindowLongPtrW(GetDlgItem(w,IDC_CODE),GWL_STYLE)&ES_READONLY);
    wchar_t code[5];
    SetDlgItemTextW(w,IDC_LOCATOR,L"JO21QK99AA99"); GetDlgItemTextW(w,IDC_CODE,code,5); assert(!wcscmp(code,L"1957"));
    SetDlgItemTextW(w,IDC_LOCATOR,L"JO22QK99AA99"); GetDlgItemTextW(w,IDC_CODE,code,5); assert(wcscmp(code,L"1957"));
    wanted.mode=IMAGE_FUBK; wanted.automatic=0; wanted.genius=1; wanted.resolution=9;
    apply_config(w,&wanted); SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); actual=capture_config(w); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)==(UINT)-1);
    assert(!IsWindowEnabled(GetDlgItem(w,IDC_CODE))); DestroyWindow(w);
    wanted.mode=IMAGE_PM5644; w=open_app(); apply_config(w,&wanted);
    SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); actual=capture_config(w); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(pattern_mode(w) && !IsWindowEnabled(GetDlgItem(w,IDC_CODE)));
    wchar_t mode_text[32]; GetDlgItemTextW(w,IDC_MODE,mode_text,32);
    assert(!wcscmp(mode_text,L"PM5644"));
    assert(SendDlgItemMessageW(w,IDC_MODE,CB_GETCURSEL,0,0)==2); DestroyWindow(w);
    /* A Linux audio configuration must open safely without enabling audio. */
    wanted.genius=3; wanted.udp_dvb=dvb_defaults(); wanted.udp_dvb.symbol_rate=5;
    wanted.udp.video.bitrate=dvb_bitrate(wanted.udp_dvb);
    wanted.udp.video.audio.bitrate=96000; strcpy(wanted.udp.video.audio.source,"ffmix.monitor");
    assert(config_save(path,&wanted));
    w=open_app(); actual=capture_config(w);
    assert(actual.genius==2 && !actual.udp.video.audio.bitrate);
    DestroyWindow(w);
    wanted.udp_dvb.system=DVB_T; wanted.udp_dvb.bandwidth_khz=333; wanted.udp_dvb.fec=2;
    wanted.udp.video.bitrate=dvb_bitrate(wanted.udp_dvb);
    assert(config_save(path,&wanted));
    w=open_app(); actual=capture_config(w);
    assert(actual.genius==2 && !actual.udp.video.audio.bitrate);
    assert(actual.udp_dvb.system==DVB_T && actual.udp_dvb.bandwidth_khz==333 && actual.udp_dvb.fec==2);
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)!=(UINT)-1);
    SendMessageW(w,WM_COMMAND,IDM_LEVEL4,0);
    assert(genius_level==4 && ts_settings.motion && udp_settings.video.motion);
    assert(ts_settings.bitrate==30160 && ts_settings.fps==10 && ts_settings.gop==20);
    assert(resolution(w).width==160 && !pattern_mode(w));
    assert(!IsWindowEnabled(GetDlgItem(w,IDC_RESOLUTION)) && IsWindowEnabled(GetDlgItem(w,IDC_CODE)));
    for (motion_system=DVB_S;motion_system<=DVB_S2;++motion_system) {
        timer=SetTimer(NULL,0,100,motion_timer); assert(timer);
        SendMessageW(w,WM_COMMAND,IDM_UDP,0); KillTimer(NULL,timer);
        assert(udp_dvb.system==motion_system && udp_dvb.symbol_rate==2 && udp_dvb.fec==0);
        assert(udp_settings.video.motion && udp_settings.video.fps==10 && udp_settings.video.gop==5);
        assert(udp_settings.video.bitrate==(motion_system==DVB_S?115196:120665));
    }
    ts_settings.gop=10; SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app();
    assert(genius_level==4 && ts_settings.motion && ts_settings.gop==10 && udp_settings.video.motion);
    assert(udp_dvb.system==DVB_S2 && udp_dvb.symbol_rate==2 && udp_dvb.pilots);
    assert(udp_settings.video.bitrate==120665 && udp_settings.video.gop==5);
    SendMessageW(w,WM_COMMAND,IDM_LEVEL1,0);
    assert(!ts_settings.motion && IsWindowEnabled(GetDlgItem(w,IDC_RESOLUTION)));
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)==(UINT)-1);
    DestroyWindow(w);
    wanted=config_defaults(); wanted.mode=IMAGE_ACTIVITY; wanted.genius=2;
    strcpy(wanted.call,"LX/PE1ITR/P"); strcpy(wanted.locator,"JO21QK"); wanted.code[0]=0;
    wanted.activity_ebu=1; wanted.activity_motion=1; wanted.ebu_top=wanted.ebu_bottom=1;
    w=open_app(); apply_config(w,&wanted);
    assert(!IsWindowEnabled(GetDlgItem(w,IDC_CODE)) && IsWindowEnabled(GetDlgItem(w,IDC_BAND)));
    assert(IsWindowEnabled(GetDlgItem(w,IDC_LOCATOR)) && !IsWindowEnabled(GetDlgItem(w,IDC_EBU_TOP)));
    assert(ts_settings.activity_motion && udp_settings.video.activity_motion);
    HBITMAP card=render_selected_pattern(w,resolution(w),L"LX/PE1ITR/P",L"JO21QK",IMAGE_ACTIVITY); assert(card);
    assert(SUCCEEDED(save_jpeg(card,L"build/activity-windows-ui.jpg"))); DeleteObject(card);
    SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); actual=capture_config(w);
    assert(actual.mode==IMAGE_ACTIVITY && actual.activity_motion && actual.activity_ebu && !actual.code[0]);
    assert(actual.ts.activity_motion && actual.udp.video.activity_motion);
    SendDlgItemMessageW(w,IDC_MODE,CB_SETCURSEL,image_mode_row(IMAGE_CONTEST),0);
    SendMessageW(w,WM_COMMAND,MAKEWPARAM(IDC_MODE,CBN_SELCHANGE),0);
    assert(!ts_settings.activity_motion && !udp_settings.video.activity_motion);
    assert(!IsWindowEnabled(GetDlgItem(w,IDC_ACTIVITY_MOTION))); DestroyWindow(w);
    wchar_t wide[32768]; assert(MultiByteToWideChar(CP_UTF8,0,path,-1,wide,32768));
    assert(DeleteFileW(wide)); free(path); pm5544_cleanup(); IWICImagingFactory_Release(factory); CoUninitialize();
    puts("Windows config UI: save/reopen, all fields, UDP apply without Start, Genius visibility and automatic code preservation OK.");
    return 0;
}
