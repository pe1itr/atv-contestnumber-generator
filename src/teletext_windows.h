/* Native Win32 view over the same page model and renderer as GTK. */
enum { TT_PAGE=380, TT_ADD, TT_REMOVE, TT_HEADER, TT_COLOR, TT_TEXT, TT_MOSAIC,
       TT_BACKGROUND, TT_BLACK, TT_LOAD, TT_CLEAR, TT_PREVIEW, TT_INTERVAL, TT_ATTRIBUTE, TT_INSERT, TT_REVEAL };
typedef struct {
    TeletextSettings book;
    TeletextEditor editor, undo;
    int selected;
    HWND window, area;
} TeletextDialog;
static void tt_win_rect(void *ctx,int x,int y,int w,int h,unsigned rgb) {
    HDC dc=ctx; RECT r={x,y,x+w,y+h};
    SetDCBrushColor(dc,RGB(rgb>>16,(rgb>>8)&255,rgb&255)); FillRect(dc,&r,GetStockObject(DC_BRUSH));
}
static void tt_win_refresh(TeletextDialog *d,const char *error) {
    wchar_t text[256];
    if (error) MultiByteToWideChar(CP_UTF8,0,error,-1,text,256);
    else swprintf(text,256,L"Page %d | Row %d, column %d | %d pages: each repeats every %d seconds.",
        100+d->selected,d->editor.cursor/40+1,d->editor.cursor%40+1,
        teletext_page_count(&d->book),teletext_interval(&d->book)*teletext_page_count(&d->book));
    SetDlgItemTextW(d->window,IDC_TELETEXT_STATUS,text); InvalidateRect(d->area,NULL,FALSE);
}
static LRESULT CALLBACK tt_win_grid(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id; TeletextDialog *d=(TeletextDialog *)data;
    if (msg==WM_GETDLGCODE) return DLGC_WANTARROWS|DLGC_WANTCHARS|DLGC_WANTALLKEYS;
    if (msg==WM_PAINT) {
        PAINTSTRUCT ps; HDC dc=BeginPaint(window,&ps); RECT r; GetClientRect(window,&r);
        wchar_t wide[33]; char header[140];
        GetDlgItemTextW(d->window,TT_HEADER,wide,33);
        WideCharToMultiByte(CP_UTF8,0,wide,-1,header,sizeof(header),NULL,NULL);
        teletext_view_plot(&d->editor,header,100+d->selected,r.right,r.bottom,tt_win_rect,dc);
        EndPaint(window,&ps); return 0;
    }
    if (msg==WM_LBUTTONDOWN || msg==WM_RBUTTONDOWN) {
        if (!d->editor.grid) return 0;
        RECT r; GetClientRect(window,&r); TeletextEditor before=d->editor;
        teletext_view_click(&d->editor,(short)LOWORD(lp),(short)HIWORD(lp),r.right,r.bottom,msg==WM_RBUTTONDOWN);
        if (memcmp(before.raw,d->editor.raw,sizeof(before.raw))) d->undo=before;
        SetFocus(window); tt_win_refresh(d,NULL); return 0;
    }
    if (msg==WM_KEYDOWN) {
        if (wp=='Z' && GetKeyState(VK_CONTROL)<0) {
            teletext_editor_undo(&d->editor,&d->undo); tt_win_refresh(d,NULL); return 0;
        }
        if (wp=='V' && GetKeyState(VK_CONTROL)<0 && d->editor.grid) {
            const char *error=NULL;
            if (OpenClipboard(window)) {
                HANDLE handle=GetClipboardData(CF_UNICODETEXT);
                const wchar_t *wide=handle?GlobalLock(handle):NULL;
                if (wide) {
                    int n=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL);
                    char *text=n>0?malloc((size_t)n):NULL;
                    if (text) {
                        WideCharToMultiByte(CP_UTF8,0,wide,-1,text,n,NULL,NULL);
                        d->undo=d->editor; error=teletext_editor_paste(&d->editor,text); free(text);
                    }
                    GlobalUnlock(handle);
                }
                CloseClipboard();
            }
            tt_win_refresh(d,error); return 0;
        }
        int key=-1;
        switch (wp) {
        case VK_LEFT: key=TT_LEFT; break; case VK_RIGHT: key=TT_RIGHT; break;
        case VK_UP: key=TT_UP; break; case VK_DOWN: key=TT_DOWN; break;
        case VK_HOME: key=TT_HOME; break; case VK_END: key=TT_END; break;
        case VK_RETURN: key=TT_ENTER; break; case VK_BACK: key=TT_BACKSPACE; break;
        case VK_DELETE: key=TT_DELETE; break;
        case VK_TAB: SetFocus(GetNextDlgTabItem(d->window,window,GetKeyState(VK_SHIFT)<0)); return 0;
        case VK_ESCAPE: PostMessageW(d->window,WM_COMMAND,IDCANCEL,0); return 0;
        }
        if (key>=0 && d->editor.grid) { if (key==TT_DELETE || key==TT_BACKSPACE) d->undo=d->editor; teletext_editor_key(&d->editor,key); tt_win_refresh(d,NULL); return 0; }
    }
    if (msg==WM_CHAR) {
        if (wp>=32 && d->editor.grid && GetKeyState(VK_CONTROL)>=0) {
            d->undo=d->editor; const char *error=teletext_editor_type(&d->editor,(unsigned)wp); tt_win_refresh(d,error);
        }
        return 0;
    }
    return DefSubclassProc(window,msg,wp,lp);
}
static const char *tt_win_store(TeletextDialog *d) {
    TeletextSettings page={0}; page.enabled=d->book.enabled;
    wchar_t wide[34]; char header[140]; GetDlgItemTextW(d->window,TT_HEADER,wide,34);
    WideCharToMultiByte(CP_UTF8,0,wide,-1,header,sizeof(header),NULL,NULL);
    if (strlen(header)>32) return "Use up to 32 unaccented header characters.";
    strcpy(page.header,header); teletext_editor_save(&d->editor,&page);
    const char *error=teletext_validate(&page); if (!error) teletext_page_set(&d->book,d->selected,&page);
    return error;
}
static void tt_win_load(TeletextDialog *d) {
    TeletextSettings page; teletext_page_get(&d->book,d->selected,&page);
    int reveal=d->editor.reveal,off=d->editor.flash_off;
    teletext_editor_load(&d->editor,&page); d->editor.reveal=reveal; d->editor.flash_off=off; d->undo=d->editor;
    wchar_t wide[33]; MultiByteToWideChar(CP_UTF8,0,page.header,-1,wide,33); SetDlgItemTextW(d->window,TT_HEADER,wide);
    wchar_t fallback[33]; swprintf(fallback,33,L"ATV TELETEXT %d",100+d->selected);
    SendDlgItemMessageW(d->window,TT_HEADER,EM_SETCUEBANNER,0,(LPARAM)fallback);
    EnableWindow(GetDlgItem(d->window,TT_REMOVE),d->selected>0); tt_win_refresh(d,NULL);
}
static void tt_win_pages(TeletextDialog *d) {
    SendDlgItemMessageW(d->window,TT_PAGE,CB_RESETCONTENT,0,0);
    for (int i=0;i<teletext_page_count(&d->book);++i) {
        wchar_t text[20]; swprintf(text,20,L"Page %d",100+i); SendDlgItemMessageW(d->window,TT_PAGE,CB_ADDSTRING,0,(LPARAM)text);
    }
    SendDlgItemMessageW(d->window,TT_PAGE,CB_SETCURSEL,d->selected,0); tt_win_load(d);
}
static int tt_win_import_path(TeletextDialog *d,const char *path) {
    TeletextSettings page; teletext_page_get(&d->book,d->selected,&page);
    char error[256];
    if (!teletext_import_file(&page,path,error,sizeof(error))) { tt_win_refresh(d,error); return 0; }
    TeletextEditor before=d->editor;
    int grid=d->editor.grid,reveal=d->editor.reveal,off=d->editor.flash_off;
    teletext_editor_load(&d->editor,&page);
    d->editor.grid=grid; d->editor.reveal=reveal; d->editor.flash_off=off; d->undo=before;
    tt_win_refresh(d,"Page loaded into the selected page. Header and interval kept. Ctrl+Z: undo. Save to keep it.");
    return 1;
}
static void tt_win_import(TeletextDialog *d) {
    wchar_t path[32768]=L"";
    OPENFILENAMEW picker={0}; picker.lStructSize=sizeof(picker); picker.hwndOwner=d->window;
    picker.lpstrFilter=L"Teletext pages (*.tti, *.txt)\0*.tti;*.txt\0\0";
    picker.lpstrFile=path; picker.nMaxFile=32768; picker.lpstrTitle=L"Load teletext page";
    picker.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    if (GetOpenFileNameW(&picker)) {
        int n=WideCharToMultiByte(CP_UTF8,0,path,-1,NULL,0,NULL,NULL);
        char *utf8=n>0?malloc((size_t)n):NULL;
        if (utf8) { WideCharToMultiByte(CP_UTF8,0,path,-1,utf8,n,NULL,NULL); tt_win_import_path(d,utf8); free(utf8); }
        else tt_win_refresh(d,"Cannot allocate the page file path.");
    } else if (CommDlgExtendedError()) tt_win_refresh(d,"Cannot open the page file chooser.");
    SetFocus(d->area);
}
static INT_PTR CALLBACK teletext_dialog(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    (void)lp; TeletextDialog *d=(TeletextDialog *)GetWindowLongPtrW(window,DWLP_USER);
    if (message==WM_INITDIALOG) {
        d=calloc(1,sizeof(*d)); if (!d) { EndDialog(window,IDCANCEL); return TRUE; }
        SetWindowLongPtrW(window,DWLP_USER,(LONG_PTR)d); d->window=window; d->book=teletext_settings;
        d->area=GetDlgItem(window,IDC_TELETEXT_TEXT);
        SetWindowSubclass(d->area,tt_win_grid,1,(DWORD_PTR)d);
        SendDlgItemMessageW(window,TT_HEADER,EM_SETLIMITTEXT,32,0);
        SendDlgItemMessageW(window,TT_HEADER,EM_SETCUEBANNER,0,(LPARAM)L"ATV TELETEXT 100");
        for (int i=0;i<7;++i) {
            wchar_t name[20]; MultiByteToWideChar(CP_UTF8,0,teletext_color_names[i],-1,name,20);
            SendDlgItemMessageW(window,TT_COLOR,CB_ADDSTRING,0,(LPARAM)name);
        }
        SendDlgItemMessageW(window,TT_COLOR,CB_SETCURSEL,6,0);
        const wchar_t *intervals[]={L"2 seconds",L"5 seconds",L"10 seconds"};
        for (int i=0;i<3;++i) SendDlgItemMessageW(window,TT_INTERVAL,CB_ADDSTRING,0,(LPARAM)intervals[i]);
        SendDlgItemMessageW(window,TT_INTERVAL,CB_SETCURSEL,teletext_interval(&d->book)==10?2:teletext_interval(&d->book)==5?1:0,0);
        for (int i=0;i<TELETEXT_ATTRIBUTE_COUNT;++i) {
            wchar_t name[40]; MultiByteToWideChar(CP_UTF8,0,teletext_attributes[i].name,-1,name,40);
            SendDlgItemMessageW(window,TT_ATTRIBUTE,CB_ADDSTRING,0,(LPARAM)name);
        }
        SendDlgItemMessageW(window,TT_ATTRIBUTE,CB_SETCURSEL,0,0);
        SetTimer(window,1001,50,NULL);
        tt_win_pages(d); return TRUE;
    }
    if (!d) return FALSE;
    if (message==WM_TIMER && wp==1001) {
        int off=GetTickCount64()%1000>=750;
        if (off!=d->editor.flash_off) { d->editor.flash_off=off; InvalidateRect(d->area,NULL,FALSE); }
        return TRUE;
    }
    if (message==WM_COMMAND) {
        int id=LOWORD(wp); const char *error=NULL;
        if (id==TT_HEADER && HIWORD(wp)==EN_CHANGE) { InvalidateRect(d->area,NULL,FALSE); return TRUE; }
        if (id==TT_REVEAL) {
            d->editor.reveal=IsDlgButtonChecked(window,TT_REVEAL)==BST_CHECKED;
            tt_win_refresh(d,NULL); return TRUE;
        }
        if (id==TT_INSERT) {
            int selected=(int)SendDlgItemMessageW(window,TT_ATTRIBUTE,CB_GETCURSEL,0,0);
            TeletextEditor before=d->editor;
            if (selected>=0 && selected<TELETEXT_ATTRIBUTE_COUNT)
                error=teletext_editor_control(&d->editor,teletext_attributes[selected].code);
            if (!error) d->undo=before;
            tt_win_refresh(d,error); SetFocus(d->area); return TRUE;
        }
        if (id==TT_INTERVAL && HIWORD(wp)==CBN_SELCHANGE) {
            const int values[]={2,5,10}; int selected=(int)SendDlgItemMessageW(window,TT_INTERVAL,CB_GETCURSEL,0,0);
            if (selected>=0 && selected<3) d->book.interval=values[selected];
            tt_win_refresh(d,NULL); return TRUE;
        }
        if (id==TT_PAGE && HIWORD(wp)==CBN_SELCHANGE) {
            int selected=(int)SendDlgItemMessageW(window,TT_PAGE,CB_GETCURSEL,0,0);
            error=tt_win_store(d);
            if (error) { SendDlgItemMessageW(window,TT_PAGE,CB_SETCURSEL,d->selected,0); tt_win_refresh(d,error); }
            else if (selected>=0) { d->selected=selected; tt_win_load(d); }
            return TRUE;
        }
        if (id==TT_ADD || id==TT_REMOVE) {
            error=tt_win_store(d); if (error) { tt_win_refresh(d,error); return TRUE; }
            if (id==TT_ADD) {
                if (!teletext_page_add(&d->book)) { tt_win_refresh(d,"Up to four pages (100-103)."); return TRUE; }
                d->selected=d->book.extra_count;
            } else { teletext_page_delete(&d->book,d->selected); d->selected=0; }
            tt_win_pages(d); return TRUE;
        }
        if (id==TT_LOAD) { tt_win_import(d); return TRUE; }
        if (id>=TT_TEXT && id<=TT_PREVIEW) {
            TeletextEditor before=d->editor;
            int color=(int)SendDlgItemMessageW(window,TT_COLOR,CB_GETCURSEL,0,0)+1;
            if (id==TT_TEXT || id==TT_MOSAIC) error=teletext_editor_control(&d->editor,(unsigned)(color+(id==TT_MOSAIC?16:0)));
            if (id==TT_BACKGROUND) error=teletext_editor_background(&d->editor,color);
            if (id==TT_BLACK) error=teletext_editor_control(&d->editor,28);
            if (id==TT_CLEAR) { memset(d->editor.raw,' ',sizeof(d->editor.raw)); d->editor.cursor=0; }
            if (id==TT_PREVIEW) d->editor.grid=!d->editor.grid;
            if (!error && id!=TT_PREVIEW) d->undo=before;
            tt_win_refresh(d,error?error:id==TT_BACKGROUND?"Selected background applied. Three control cells inserted; continue at the cursor. Colour extends to the right.":NULL);
            SetFocus(d->area); return TRUE;
        }
        if (id==IDOK) {
            error=tt_win_store(d); if (error) { tt_win_refresh(d,error); return TRUE; }
            TeletextSettings previous=teletext_settings; teletext_settings=d->book;
            AppConfig settings=capture_config(GetParent(window)); char *path=config_path();
            int saved=path && config_save(path,&settings); free(path);
            if (!saved) { teletext_settings=previous; tt_win_refresh(d,"Save failed. Check write permissions in the application directory."); return TRUE; }
            SetDlgItemTextW(GetParent(window),IDC_STATUS,L"Teletext pages saved. Restart UDP to transmit changes.");
            EndDialog(window,IDOK); return TRUE;
        }
    }
    if (message==WM_CLOSE || (message==WM_COMMAND && LOWORD(wp)==IDCANCEL)) { EndDialog(window,IDCANCEL); return TRUE; }
    if (message==WM_DESTROY) { KillTimer(window,1001); RemoveWindowSubclass(d->area,tt_win_grid,1); free(d); SetWindowLongPtrW(window,DWLP_USER,0); }
    return FALSE;
}
