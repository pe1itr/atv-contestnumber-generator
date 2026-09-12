#include "../src/main.c"
#include <assert.h>
static BOOL CALLBACK apply_udp(HWND window,LPARAM unused) {
    (void)unused; wchar_t title[80]; GetWindowTextW(window,title,80);
    if (wcscmp(title,L"DATV: UDP-uitvoer")) return TRUE;
    SetDlgItemTextW(window,IDC_UDP_IP,L"192.168.1.50");
    SetDlgItemInt(window,IDC_UDP_PORT,12345,FALSE);
    SetDlgItemInt(window,IDC_UDP_BITRATE,240000,FALSE);
    SetDlgItemInt(window,IDC_UDP_FPS,12,FALSE); SetDlgItemInt(window,IDC_UDP_GOP,3,FALSE);
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
    wanted.aspect=1; wanted.resolution=1; wanted.band=10; wanted.genius=2;
    wanted.show=0; wanted.inverse=1; wanted.blue_yellow=1; wanted.show_sum=1; wanted.top_code=1; wanted.ebu_top=1; wanted.ebu_bottom=1;
    wanted.ts=(DatvSettings){60000,30,2,1}; apply_config(w,&wanted);
    UINT_PTR timer=SetTimer(NULL,0,100,drive_timer); assert(timer);
    SendMessageW(w,WM_COMMAND,IDM_UDP,0); KillTimer(NULL,timer);
    assert(udp_settings.port==12345 && udp_settings.video.fps==12 && udp_settings.video.gop==3);
    wanted.udp=udp_settings;
    SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); AppConfig actual=capture_config(w); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)!=(UINT)-1);
    assert(GetWindowLongPtrW(GetDlgItem(w,IDC_CODE),GWL_STYLE)&ES_READONLY);
    wchar_t code[5];
    SetDlgItemTextW(w,IDC_LOCATOR,L"JO21QK99AA99"); GetDlgItemTextW(w,IDC_CODE,code,5); assert(!wcscmp(code,L"1957"));
    SetDlgItemTextW(w,IDC_LOCATOR,L"JO22QK99AA99"); GetDlgItemTextW(w,IDC_CODE,code,5); assert(wcscmp(code,L"1957"));
    wanted.mode=1; wanted.automatic=0; wanted.genius=1; wanted.resolution=9;
    apply_config(w,&wanted); SendMessageW(w,WM_COMMAND,IDM_SAVE_CONFIG,0); DestroyWindow(w);
    w=open_app(); actual=capture_config(w); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(GetMenuState(GetSubMenu(GetMenu(w),0),IDM_UDP,MF_BYCOMMAND)==(UINT)-1);
    assert(!IsWindowEnabled(GetDlgItem(w,IDC_CODE))); DestroyWindow(w);
    wchar_t wide[32768]; assert(MultiByteToWideChar(CP_UTF8,0,path,-1,wide,32768));
    assert(DeleteFileW(wide)); free(path); pm5544_cleanup(); CoUninitialize();
    puts("Windows config UI: save/reopen, all fields, UDP apply without Start, Genius visibility and automatic code preservation OK.");
    return 0;
}
