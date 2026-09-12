/* Local before/after comparison. Both panes use the same pixel scale and scroll offsets. */
typedef struct QualityDialog QualityDialog;
typedef struct { QualityDialog *owner; HWND window; int index; } QualityPane;
struct QualityDialog {
    HBITMAP images[2];
    Resolution size;
    DatvSettings settings;
    DatvStream *job;
    QualityPane panes[2];
    int zoom, x, y, done;
};
static void quality_ranges(QualityDialog *d) {
    for (int i=0;i<2;++i) {
        RECT r; GetClientRect(d->panes[i].window,&r);
        SCROLLINFO s={sizeof(s),SIF_RANGE|SIF_PAGE|SIF_POS,0,d->size.width*d->zoom-1,(UINT)r.right,d->x,0};
        SetScrollInfo(d->panes[i].window,SB_HORZ,&s,TRUE);
        s.nMax=d->size.height*d->zoom-1; s.nPage=(UINT)r.bottom; s.nPos=d->y;
        SetScrollInfo(d->panes[i].window,SB_VERT,&s,TRUE);
        InvalidateRect(d->panes[i].window,NULL,FALSE);
    }
}
static LRESULT CALLBACK quality_pane(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id;
    QualityPane *pane=(QualityPane *)data; QualityDialog *d=pane->owner;
    if (msg==WM_PAINT) {
        PAINTSTRUCT paint; HDC dc=BeginPaint(window,&paint); RECT r; GetClientRect(window,&r);
        FillRect(dc,&r,(HBRUSH)GetStockObject(DKGRAY_BRUSH));
        if (d->images[pane->index]) {
            HDC source=CreateCompatibleDC(dc);
            if (source) {
                HGDIOBJ old=SelectObject(source,d->images[pane->index]);
                SetStretchBltMode(dc,COLORONCOLOR);
                StretchBlt(dc,-d->x,-d->y,d->size.width*d->zoom,d->size.height*d->zoom,
                    source,0,0,d->size.width,d->size.height,SRCCOPY);
                SelectObject(source,old); DeleteDC(source);
            }
        }
        EndPaint(window,&paint); return 0;
    }
    if (msg==WM_HSCROLL || msg==WM_VSCROLL) {
        int bar=msg==WM_HSCROLL?SB_HORZ:SB_VERT;
        SCROLLINFO s={0}; s.cbSize=sizeof(s); s.fMask=SIF_ALL; GetScrollInfo(window,bar,&s);
        int pos=s.nPos;
        switch (LOWORD(wp)) {
        case SB_LINELEFT: pos-=16; break;
        case SB_LINERIGHT: pos+=16; break;
        case SB_PAGELEFT: pos-=(int)s.nPage; break;
        case SB_PAGERIGHT: pos+=(int)s.nPage; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: pos=s.nTrackPos; break;
        case SB_LEFT: pos=0; break;
        case SB_RIGHT: pos=s.nMax; break;
        }
        int maximum=s.nMax-(int)s.nPage+1;
        if (maximum<0) maximum=0;
        if (pos<0) pos=0;
        if (pos>maximum) pos=maximum;
        if (bar==SB_HORZ) d->x=pos; else d->y=pos;
        quality_ranges(d); return 0;
    }
    return DefSubclassProc(window,msg,wp,lp);
}
static void quality_poll(HWND window,QualityDialog *d) {
    if (!d->job || d->done) return;
    DatvUdpStatus status; datv_udp_status(d->job,&status);
    if (status.state==DATV_PREPARING) return;
    d->done=1;
    if (status.state==DATV_FAILED) {
        wchar_t message[256]; MultiByteToWideChar(CP_UTF8,0,status.error,-1,message,256);
        SetDlgItemTextW(window,IDC_QUALITY_STATUS,message); return;
    }
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=d->size.width; info.bmiHeader.biHeight=-d->size.height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void *pixels=NULL;
    HBITMAP image=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    if (!image || !datv_preview_image(d->job,pixels,d->size.width,d->size.height,d->size.width*4)) {
        if (image) DeleteObject(image);
        SetDlgItemTextW(window,IDC_QUALITY_STATUS,L"Kan het gecomprimeerde voorbeeld niet weergeven."); return;
    }
    d->images[1]=image;
    wchar_t message[320];
    swprintf(message,320,L"%d x %d · %d bit/s · %d beelden/s · GOP %d · QP %d\nVergelijk vooral de kleine letters. Lagere QP is geen garantie voor leesbaarheid.",
        d->size.width,d->size.height,d->settings.bitrate,d->settings.fps,d->settings.gop,status.qp);
    SetDlgItemTextW(window,IDC_QUALITY_STATUS,message);
    InvalidateRect(d->panes[1].window,NULL,FALSE);
}
static INT_PTR CALLBACK quality_dialog(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    QualityDialog *d=(QualityDialog *)GetWindowLongPtrW(window,DWLP_USER);
    if (msg==WM_INITDIALOG) {
        d=(QualityDialog *)lp; SetWindowLongPtrW(window,DWLP_USER,lp);
        for (int i=0;i<2;++i) {
            d->panes[i]=(QualityPane){d,GetDlgItem(window,i?IDC_QUALITY_AFTER:IDC_QUALITY_BEFORE),i};
            if (!SetWindowSubclass(d->panes[i].window,quality_pane,1,(DWORD_PTR)&d->panes[i])) { EndDialog(window,-1); return TRUE; }
        }
        CheckDlgButton(window,IDC_QUALITY_ZOOM,d->zoom==2?BST_CHECKED:BST_UNCHECKED);
        quality_ranges(d);
        if (!SetTimer(window,1,100,NULL)) EndDialog(window,-1);
        return TRUE;
    }
    if (msg==WM_TIMER) { quality_poll(window,d); return TRUE; }
    if (msg==WM_COMMAND && LOWORD(wp)==IDC_QUALITY_ZOOM) {
        d->zoom=IsDlgButtonChecked(window,IDC_QUALITY_ZOOM)==BST_CHECKED?2:1;
        d->x=d->y=0; quality_ranges(d); return TRUE;
    }
    if (msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(wp)==IDCANCEL)) {
        KillTimer(window,1); EndDialog(window,0); return TRUE;
    }
    return FALSE;
}
static void quality_compare(HWND parent,HBITMAP original,Resolution size,const char *call,DatvSettings settings) {
    DIBSECTION dib={0};
    if (GetObjectW(original,sizeof(dib),&dib)!=sizeof(dib) || !dib.dsBm.bmBits) { error(parent,L"Kan het beeld niet lezen."); return; }
    QualityDialog d={0}; d.images[0]=original; d.size=size; d.settings=settings; d.zoom=size.width<=240?2:1;
    char detail[256];
    d.job=datv_preview_start(dib.dsBm.bmBits,size.width,size.height,dib.dsBm.bmWidthBytes,call,settings,detail);
    if (!d.job) { ts_error(parent,detail); return; }
    if (DialogBoxParamW((HINSTANCE)GetWindowLongPtrW(parent,GWLP_HINSTANCE),MAKEINTRESOURCEW(IDD_QUALITY),parent,quality_dialog,(LPARAM)&d)==-1)
        error(parent,L"Kan het vergelijkingsvenster niet openen.");
    datv_udp_destroy(d.job);
    if (d.images[1]) DeleteObject(d.images[1]);
}
