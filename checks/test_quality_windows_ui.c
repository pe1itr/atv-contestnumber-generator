#include "../src/main.c"
#include <assert.h>
static int stage,qp,test_fps;
static ULONGLONG deadline;
static BOOL CALLBACK drive(HWND window,LPARAM unused) {
    (void)unused; assert(GetTickCount64()<deadline);
    wchar_t title[80]; GetWindowTextW(window,title,80);
    if (!wcscmp(title,L"DATV: UDP-uitvoer")) {
        if (stage==0) {
            SetDlgItemTextW(window,IDC_UDP_IP,L"invalid"); SetDlgItemTextW(window,IDC_UDP_PORT,L"");
            SetDlgItemInt(window,IDC_UDP_BITRATE,115196,FALSE); SetDlgItemInt(window,IDC_UDP_FPS,test_fps,FALSE); SetDlgItemInt(window,IDC_UDP_GOP,2,FALSE);
            stage=1; PostMessageW(window,WM_COMMAND,IDC_UDP_PREVIEW,0);
        } else if (stage==2) { stage=3; PostMessageW(window,WM_COMMAND,IDCANCEL,0); }
    } else if (!wcscmp(title,L"DATV: Beeld controleren")) {
        QualityDialog *d=(QualityDialog *)GetWindowLongPtrW(window,DWLP_USER);
        if (d && d->done) {
            assert(d->images[1]); DatvUdpStatus status; datv_udp_status(d->job,&status);
            qp=status.qp; assert(qp>=24 && qp<=48 && !status.packets);
            CheckDlgButton(window,IDC_QUALITY_ZOOM,BST_UNCHECKED); SendMessageW(window,WM_COMMAND,IDC_QUALITY_ZOOM,0); assert(d->zoom==1);
            CheckDlgButton(window,IDC_QUALITY_ZOOM,BST_CHECKED); SendMessageW(window,WM_COMMAND,IDC_QUALITY_ZOOM,0); assert(d->zoom==2);
            SendMessageW(d->panes[0].window,WM_HSCROLL,SB_LINERIGHT,0);
            assert(GetScrollPos(d->panes[0].window,SB_HORZ)==GetScrollPos(d->panes[1].window,SB_HORZ));
            stage=2; PostMessageW(window,WM_COMMAND,IDCANCEL,0);
        }
    }
    return TRUE;
}
static void CALLBACK tick(HWND w,UINT m,UINT_PTR id,DWORD time) {
    (void)w;(void)m;(void)id;(void)time; EnumThreadWindows(GetCurrentThreadId(),drive,0);
}
int wmain(void) {
    assert(SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED)));
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    HWND w=CreateDialogParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_MAIN),NULL,dialog,0); assert(w);
    AppConfig wanted=config_defaults(); strcpy(wanted.call,"PE1ITR"); strcpy(wanted.locator,"JO21QK"); strcpy(wanted.code,"1957");
    wanted.automatic=0; wanted.resolution=2; wanted.genius=2; wanted.show_sum=1; wanted.top_code=1; apply_config(w,&wanted);
    int high=48;
    for (int i=0;i<2;++i) {
        stage=0;qp=0;test_fps=i?4:10; deadline=GetTickCount64()+60000;
        UINT_PTR timer=SetTimer(NULL,0,50,tick); assert(timer);
        SendMessageW(w,WM_COMMAND,IDM_UDP,0); KillTimer(NULL,timer);
        assert(stage==3 && qp); if (!i) high=qp; else assert(qp<=high);
        AppConfig actual=capture_config(w); assert(!memcmp(&actual,&wanted,sizeof(wanted)));
        printf("Windows quality UI: 240px, 115196 bit/s, %d fps, GOP2 -> QP%d; no packets/settings mutation, invalid destination allowed, zoom/scroll OK.\n",test_fps,qp);
    }
    DestroyWindow(w); pm5544_cleanup(); CoUninitialize(); return 0;
}
