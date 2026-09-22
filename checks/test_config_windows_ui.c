#include "../src/main.c"
#include <assert.h>
static BOOL CALLBACK apply_teletext(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[80]; GetWindowTextW(window,title,80);
    if (wcscmp(title,L"Teletekst - pagina 100")) return TRUE;
    SetDlgItemTextW(window,IDC_TELETEXT_TEXT,L"ATV CONTEST\r\nPagina 100");
    PostMessageW(window,WM_COMMAND,IDOK,0); return TRUE;
}
static void CALLBACK teletext_timer(HWND w,UINT m,UINT_PTR id,DWORD t) {
    (void)w; (void)m; (void)id; (void)t; EnumThreadWindows(GetCurrentThreadId(),apply_teletext,0);
}
static BOOL CALLBACK apply_udp(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[80]; GetWindowTextW(window,title,80);
    if (wcscmp(title,L"DATV: UDP-uitvoer")) return TRUE;
    SetDlgItemTextW(window,IDC_UDP_IP,L"192.168.1.50");
    SetDlgItemInt(window,IDC_UDP_PORT,12345,FALSE);
    SendDlgItemMessageW(window,IDC_DVB_SYSTEM,CB_SETCURSEL,DVB_T,0);
    SendDlgItemMessageW(window,IDC_DVB_BW,CB_SETCURSEL,2,0);
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
static HWND open_app(void) {
    HWND w=CreateDialogParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_MAIN),NULL,dialog,0);
    assert(w); return w;
}
int wmain(void) {
    assert(SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED)));
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
    assert(teletext_settings.enabled && !strncmp(teletext_settings.text,"ATV CONTEST",11));
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
    wchar_t wide[32768]; assert(MultiByteToWideChar(CP_UTF8,0,path,-1,wide,32768));
    assert(DeleteFileW(wide)); free(path); pm5544_cleanup(); CoUninitialize();
    puts("Windows config UI: save/reopen, all fields, UDP apply without Start, Genius visibility and automatic code preservation OK.");
    return 0;
}
