#include <winsock2.h>
#include "../src/main.c"
#include <assert.h>
#include <dlgs.h>
static int stage=-3;
static wchar_t target[MAX_PATH];
static SOCKET receiver;
static int port, received;
static ULONGLONG udp_deadline;
static void select_dvb(HWND window,int id,int index) {
    SendDlgItemMessageW(window,id,CB_SETCURSEL,index,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),0);
}
static void check_dvb(HWND window,int field) {
    assert(GetWindowLongPtrW(GetDlgItem(window,field),GWL_STYLE)&ES_READONLY);
    select_dvb(window,IDC_DVB_SR,9); select_dvb(window,IDC_DVB_SYSTEM,DVB_S2);
    select_dvb(window,IDC_DVB_FEC,0);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==30654);
    CheckDlgButton(window,IDC_DVB_PILOTS,BST_CHECKED);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_PILOTS,BN_CLICKED),0);
    assert(dvb_window_read(window).fec==7);
    select_dvb(window,IDC_DVB_SR,10); select_dvb(window,IDC_DVB_FEC,0);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==30890);
    select_dvb(window,IDC_DVB_SR,7);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==34800);
    CheckDlgButton(window,IDC_DVB_PILOTS,BST_UNCHECKED);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_PILOTS,BN_CLICKED),0);
    select_dvb(window,IDC_DVB_SR,2);
    const UINT rates[]={30718,36862,40549};
    select_dvb(window,IDC_DVB_SYSTEM,DVB_S); select_dvb(window,IDC_DVB_FEC,0);
    for (int i=0;i<3;++i) {
        select_dvb(window,IDC_DVB_SR,6+i);
        assert(GetDlgItemInt(window,field,NULL,FALSE)==rates[i]);
        assert(SendDlgItemMessageW(window,IDC_DVB_FEC,CB_GETCOUNT,0,0)==(i<2?4:5));
        assert(dvb_window_read(window).fec==1);
    }
    select_dvb(window,IDC_DVB_SR,2); select_dvb(window,IDC_DVB_FEC,0);
    select_dvb(window,IDC_DVB_SYSTEM,DVB_S);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==115196);
    select_dvb(window,IDC_DVB_SYSTEM,DVB_S2);
    assert(IsWindowEnabled(GetDlgItem(window,IDC_DVB_PILOTS)));
    CheckDlgButton(window,IDC_DVB_PILOTS,BST_CHECKED);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_PILOTS,BN_CLICKED),0);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==120665);
    CheckDlgButton(window,IDC_DVB_PILOTS,BST_UNCHECKED);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_DVB_PILOTS,BN_CLICKED),0);
    assert(GetDlgItemInt(window,field,NULL,FALSE)==123607);
    select_dvb(window,IDC_DVB_SYSTEM,DVB_T);
    select_dvb(window,IDC_DVB_BW,0); select_dvb(window,IDC_DVB_GUARD,0);
    assert(!IsWindowEnabled(GetDlgItem(window,IDC_DVB_SR)) && !IsWindowEnabled(GetDlgItem(window,IDC_DVB_PILOTS)));
    assert(IsWindowEnabled(GetDlgItem(window,IDC_DVB_BW)) && IsWindowEnabled(GetDlgItem(window,IDC_DVB_GUARD)));
    assert(GetDlgItemInt(window,field,NULL,FALSE)==103676);
    select_dvb(window,IDC_DVB_SYSTEM,field==IDC_TS_BITRATE?DVB_S2:DVB_S);
}
static void receive_udp(void) {
    char bytes[2048]; int n;
    while ((n=recv(receiver,bytes,sizeof(bytes),0))>=0) { assert(n==1316); ++received; }
    assert(WSAGetLastError()==WSAEWOULDBLOCK);
}
static BOOL CALLBACK drive_window(HWND window,LPARAM unused) {
    (void)unused;
    wchar_t title[100]; GetWindowTextW(window,title,100);
    if (stage<0 && !wcscmp(title,L"EIT-programma-informatie")) {
        assert(GetWindowLongPtrW(GetDlgItem(window,IDC_EIT_CALL),GWL_STYLE)&ES_READONLY);
        wchar_t locator[32]; GetDlgItemTextW(window,IDC_EIT_LOCATOR,locator,32);
        assert(!wcscmp(locator,L"JO21QK"));
        SetDlgItemTextW(window,IDC_EIT_CITY,stage==-3?L"Eindhoven":L"Annuleren");
        SetDlgItemTextW(window,IDC_EIT_OPERATOR,L"René");
        SetDlgItemTextW(window,IDC_EIT_DESCRIPTION,L"70 cm ATV-station");
        PostMessageW(window,WM_COMMAND,stage==-3?IDOK:IDCANCEL,0); ++stage;
    }
    if (stage>=10 && !wcscmp(title,L"DATV: UDP-uitvoer")) {
        assert(GetTickCount64()<udp_deadline);
        wchar_t status[512]; GetDlgItemTextW(window,IDC_UDP_STATUS,status,512);
        if (stage==10) {
            stage=-10; /* Synchronous control changes may dispatch the timer again. */
            check_dvb(window,IDC_UDP_BITRATE);
            CheckDlgButton(window,IDC_INCLUDE_EIT,BST_CHECKED);
            assert(GetDlgItemInt(window,IDC_UDP_PORT,NULL,FALSE)==10000);
            SetDlgItemTextW(window,IDC_UDP_IP,L"999.1.2.3"); stage=11; PostMessageW(window,WM_COMMAND,IDC_UDP_START,0);
        } else if (stage==11) {
            assert(wcsstr(status,L"geldig")); SetDlgItemTextW(window,IDC_UDP_IP,L"127.0.0.1");
            SetDlgItemInt(window,IDC_UDP_PORT,port,FALSE); stage=12; PostMessageW(window,WM_COMMAND,IDC_UDP_START,0);
        } else if (stage==12) {
            receive_udp();
            if (received>=4) {
                assert(wcsstr(status,L"UDP-uitvoer actief")); assert(!IsWindowEnabled(GetDlgItem(window,IDC_UDP_IP)));
                assert(!IsWindowEnabled(GetDlgItem(window,IDC_DVB_SYSTEM)));
                assert(!IsWindowEnabled(GetDlgItem(window,IDC_DVB_FEC)));
                assert(!IsWindowEnabled(GetDlgItem(window,IDC_INCLUDE_EIT)));
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
        stage=-10; /* Avoid re-entering this long sequence of UI assertions. */
        SetDlgItemInt(window,IDC_TS_BITRATE,60000,FALSE);
        check_dvb(window,IDC_TS_BITRATE);
        CheckDlgButton(window,IDC_INCLUDE_EIT,BST_CHECKED);
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
    SendMessageW(window,WM_COMMAND,IDM_EIT,0); SendMessageW(window,WM_COMMAND,IDM_EIT,0);
    assert(!strcmp(station_info.city,"Eindhoven") && !strcmp(station_info.operator_name,"René"));
    stage=0;
    SendMessageW(window,WM_COMMAND,IDM_EXPORT_TS,0); KillTimer(NULL,timer);
    assert(ts_settings.eit_enabled);
    assert(stage==2 && ts_settings.bitrate==123607 && ts_settings.gop==1);
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
    assert(udp_settings.video.bitrate==115196 && udp_settings.video.eit_enabled);
    closesocket(receiver); WSACleanup();
    puts("Windows UDP UI: adresvalidatie, Start/Stop, opnieuw starten en sluiten tijdens uitzending OK.");
    DestroyWindow(window); pm5544_cleanup(); CoUninitialize();
    puts("Windows UI: levels, TS-instellingen, export, Unicode-pad en ongewijzigd nummer OK.");
    return 0;
}
