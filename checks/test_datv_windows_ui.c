#include <winsock2.h>
#include "../src/main.c"
#include <assert.h>
#include <dlgs.h>
static int stage;
static wchar_t target[MAX_PATH];
static SOCKET receiver;
static int port, received;
static ULONGLONG udp_deadline;
static void receive_udp(void) {
    char bytes[2048]; int n;
    while ((n=recv(receiver,bytes,sizeof(bytes),0))>=0) { assert(n==1316); ++received; }
    assert(WSAGetLastError()==WSAEWOULDBLOCK);
}
static BOOL CALLBACK drive_window(HWND window,LPARAM unused) {
    (void)unused;
    wchar_t title[100]; GetWindowTextW(window,title,100);
    if (stage>=10 && !wcscmp(title,L"DATV: UDP-uitvoer")) {
        assert(GetTickCount64()<udp_deadline);
        wchar_t status[512]; GetDlgItemTextW(window,IDC_UDP_STATUS,status,512);
        if (stage==10) {
            assert(GetDlgItemInt(window,IDC_UDP_PORT,NULL,FALSE)==10000);
            SetDlgItemTextW(window,IDC_UDP_IP,L"999.1.2.3"); stage=11; PostMessageW(window,WM_COMMAND,IDC_UDP_START,0);
        } else if (stage==11) {
            assert(wcsstr(status,L"geldig")); SetDlgItemTextW(window,IDC_UDP_IP,L"127.0.0.1");
            SetDlgItemInt(window,IDC_UDP_PORT,port,FALSE); stage=12; PostMessageW(window,WM_COMMAND,IDC_UDP_START,0);
        } else if (stage==12) {
            receive_udp();
            if (received>=4) {
                assert(wcsstr(status,L"UDP-uitvoer actief")); assert(!IsWindowEnabled(GetDlgItem(window,IDC_UDP_IP)));
                stage=13; PostMessageW(window,WM_COMMAND,IDC_UDP_STOP,0);
            }
        } else if (stage==13 && wcsstr(status,L"UDP gestopt")) {
            assert(IsWindowEnabled(GetDlgItem(window,IDC_UDP_IP))); stage=14; PostMessageW(window,WM_COMMAND,IDC_UDP_START,0);
        } else if (stage==14) {
            receive_udp();
            if (received>=8) { stage=15; PostMessageW(window,WM_CLOSE,0,0); }
        }
    }
    if (stage==0 && !wcscmp(title,L"DATV: TS-proefbestand")) {
        SetDlgItemInt(window,IDC_TS_BITRATE,60000,FALSE);
        SetDlgItemInt(window,IDC_TS_SECONDS,3,FALSE);
        SetDlgItemInt(window,IDC_TS_FPS,2,FALSE);
        SetDlgItemInt(window,IDC_TS_GOP,1,FALSE);
        stage=1; PostMessageW(window,WM_COMMAND,IDOK,0);
    } else if (stage==1 && !wcscmp(title,L"TS opslaan")) {
        assert(SetDlgItemTextW(window,cmb13,target));
        stage=2; PostMessageW(window,WM_COMMAND,IDOK,0);
    }
    return TRUE;
}
static void CALLBACK drive_timer(HWND w,UINT m,UINT_PTR id,DWORD t) {
    (void)w; (void)m; (void)id; (void)t;
    EnumThreadWindows(GetCurrentThreadId(),drive_window,0);
}
int wmain(void) {
    assert(SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED)));
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    HWND window=CreateDialogParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_MAIN),NULL,dialog,0);
    assert(window);
    HMENU file=GetSubMenu(GetMenu(window),0);
    assert(genius_level==1 && GetMenuState(file,IDM_EXPORT_TS,MF_BYCOMMAND)==(UINT)-1);
    assert(GetMenuState(file,IDM_UDP,MF_BYCOMMAND)==(UINT)-1);
    SendMessageW(window,WM_COMMAND,IDM_LEVEL2,0);
    assert(genius_level==2 && GetMenuState(file,IDM_EXPORT_TS,MF_BYCOMMAND)!=(UINT)-1);
    assert(GetMenuState(file,IDM_UDP,MF_BYCOMMAND)!=(UINT)-1);
    SendMessageW(window,WM_COMMAND,IDM_LEVEL1,0);
    assert(GetMenuState(file,IDM_EXPORT_TS,MF_BYCOMMAND)==(UINT)-1);
    SendMessageW(window,WM_COMMAND,IDM_LEVEL2,0);
    SetDlgItemTextW(window,IDC_CALL,L"PE1ITR"); SetDlgItemTextW(window,IDC_LOCATOR,L"JO21QK");
    CheckRadioButton(window,IDC_AUTO,IDC_MANUAL,IDC_MANUAL);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_MANUAL,BN_CLICKED),0);
    SetDlgItemTextW(window,IDC_CODE,L"1957");
    CheckDlgButton(window,IDC_BLUE_YELLOW,BST_CHECKED);
    SendDlgItemMessageW(window,IDC_RESOLUTION,CB_SETCURSEL,1,0);
    wchar_t temp[MAX_PATH]; assert(GetTempPathW(MAX_PATH,temp));
    swprintf(target,MAX_PATH,L"%lsatv-ui-%lu-é.ts",temp,GetCurrentProcessId());
    assert(GetFileAttributesW(target)==INVALID_FILE_ATTRIBUTES);
    UINT_PTR timer=SetTimer(NULL,0,100,drive_timer); assert(timer);
    SendMessageW(window,WM_COMMAND,IDM_EXPORT_TS,0); KillTimer(NULL,timer);
    assert(stage==2 && ts_settings.bitrate==60000 && ts_settings.gop==1);
    wchar_t status[512],code[5];
    GetDlgItemTextW(window,IDC_STATUS,status,512); assert(wcsstr(status,L"TS opgeslagen:"));
    GetDlgItemTextW(window,IDC_CODE,code,5); assert(!wcscmp(code,L"1957"));
    assert(GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES); DeleteFileW(target);
    WSADATA ws; assert(!WSAStartup(MAKEWORD(2,2),&ws));
    receiver=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); assert(receiver!=INVALID_SOCKET);
    struct sockaddr_in address={0}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(receiver,(struct sockaddr *)&address,sizeof(address)));
    int length=sizeof(address); assert(!getsockname(receiver,(struct sockaddr *)&address,&length)); port=ntohs(address.sin_port);
    u_long nonblocking=1; assert(!ioctlsocket(receiver,FIONBIO,&nonblocking));
    stage=10; udp_deadline=GetTickCount64()+20000;
    timer=SetTimer(NULL,0,100,drive_timer); assert(timer);
    SendMessageW(window,WM_COMMAND,IDM_UDP,0); KillTimer(NULL,timer);
    assert(stage==15 && received>=8 && udp_settings.port==port);
    closesocket(receiver); WSACleanup();
    puts("Windows UDP UI: adresvalidatie, Start/Stop, opnieuw starten en sluiten tijdens uitzending OK.");
    DestroyWindow(window); pm5544_cleanup(); CoUninitialize();
    puts("Windows UI: levels, TS-instellingen, export, Unicode-pad en ongewijzigd nummer OK.");
    return 0;
}
