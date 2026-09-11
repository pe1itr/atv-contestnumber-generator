#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wincodec.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>
#include "resource.h"
#include "core.h"
#include "app_info.h"
#include "pm5544.h"
#include "datv.h"

static IWICImagingFactory *factory;
static int ready;
static wchar_t contest_square[7];
static DatvSettings ts_settings;
static DatvUdpSettings udp_settings;
static int genius_level=1;

static void read_text(HWND window, int id, wchar_t *text, int capacity) {
    GetDlgItemTextW(window, id, text, capacity);
    CharUpperBuffW(text, (DWORD)wcslen(text));
}
static Resolution resolution(HWND window) {
    int aspect = (int)SendDlgItemMessageW(window, IDC_ASPECT, CB_GETCURSEL, 0, 0);
    int index = (int)SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
    if (index < 0 || index >= RESOLUTION_COUNT) index = DEFAULT_RESOLUTION_INDEX;
    return (aspect == 1 ? resolutions169 : resolutions43)[index];
}
static void update_resolutions(HWND window) {
    int old = (int)SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
    int aspect = (int)SendDlgItemMessageW(window, IDC_ASPECT, CB_GETCURSEL, 0, 0);
    const Resolution *list = aspect == 1 ? resolutions169 : resolutions43;
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_RESETCONTENT, 0, 0);
    for (int i=0; i<RESOLUTION_COUNT; ++i) {
        wchar_t label[32];
        swprintf(label, 32, L"%d x %d", list[i].width, list[i].height);
        SendDlgItemMessageW(window, IDC_RESOLUTION, CB_ADDSTRING, 0, (LPARAM)label);
    }
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, old < 0 ? DEFAULT_RESOLUTION_INDEX : old, 0);
}
static int random_code(wchar_t code[5]) {
    unsigned value;
    do {
        if (BCryptGenRandom(NULL, (PUCHAR)&value, sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
        /* Reject the incomplete modulo bucket, then choose a valid PHP-style code. */
        if (value >= 4294962000u) continue;
        value = 1000 + value % 9000;
        if (generated_code_valid(value)) break;
    } while (1);
    swprintf(code, 5, L"%04u", value);
    return 1;
}

static int new_code(HWND window) {
    wchar_t code[5], previous[5];
    GetDlgItemTextW(window, IDC_CODE, previous, 5);
    do {
        if (!random_code(code)) {
            MessageBoxW(window, L"Een nieuw nummer maken is mislukt. Probeer het opnieuw.", L"ATV contestnummer", MB_OK | MB_ICONERROR);
            return 0;
        }
    } while (!wcscmp(code, previous));
    SetDlgItemTextW(window, IDC_CODE, code);
    wchar_t locator[LOCATOR_MAX_LENGTH+1];
    read_text(window, IDC_LOCATOR, locator, LOCATOR_MAX_LENGTH+1);
    locator_square_changed(contest_square, locator);
    return 1;
}

/* Fit each line independently so portable callsigns and long locators stay inside the image. */
static int draw_line(HDC dc, const wchar_t *text, RECT box, int height, UINT alignment) {
    if (!*text) return 1;
    HFONT font = NULL;
    HGDIOBJ previous;
    SIZE extent;
    for (;;) {
        font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
        if (!font) return 0;
        previous = SelectObject(dc, font);
        if (!GetTextExtentPoint32W(dc, text, (int)wcslen(text), &extent)) {
            SelectObject(dc, previous); DeleteObject(font); return 0;
        }
        if ((extent.cx <= box.right-box.left && extent.cy <= box.bottom-box.top) || height <= 4) break;
        SelectObject(dc, previous);
        DeleteObject(font);
        --height;
    }
    int result = DrawTextW(dc, text, -1, &box, alignment | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, previous);
    DeleteObject(font);
    return result != 0;
}
static HBITMAP render(int width, int height, const wchar_t *call, const wchar_t *code,
                      const wchar_t *locator, int show_locator, const wchar_t *band, int inverse, int show_sum, int top_code, int blue_yellow) {
    BITMAPINFO info = {0};
    void *pixels;
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return NULL;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!bitmap) { DeleteDC(dc); return NULL; }
    HGDIOBJ old = SelectObject(dc, bitmap);
    ContestPalette palette = contest_palette(blue_yellow, inverse);
    SetDCBrushColor(dc, RGB(palette.background.red, palette.background.green, palette.background.blue));
    RECT background = {0, 0, width, height};
    FillRect(dc, &background, (HBRUSH)GetStockObject(DC_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(palette.foreground.red, palette.foreground.green, palette.foreground.blue));
    int margin = width / 40;
    int small_size = height*6/100;
    if (small_size < 7) small_size = 7;
    RECT top = {margin, height/50, width-margin, height*22/100};
    RECT center = {margin, height*22/100, width-margin, height*80/100};
    RECT bottom = {margin, height*80/100, width-margin, height*91/100};
    RECT footer = {width/2, height*91/100, width-margin, height*99/100};
    RECT sum_box = {margin, height*91/100, width/2, height*99/100};
    if (!show_locator) center.bottom = height*91/100;
    if (top_code) top.top = height*9/100;
    int ok = draw_line(dc, call, top, height*17/100, DT_CENTER)
        && draw_line(dc, code, center, height*70/100, DT_CENTER);
    if (show_locator) ok = ok && draw_line(dc, locator, bottom, height*14/100, DT_CENTER);
    ok = ok && draw_line(dc, band, footer, small_size, DT_RIGHT);
    if (top_code && valid_code(code)) {
        RECT corner = {width/2, height/100, width-margin, height*9/100};
        ok = ok && draw_line(dc, code, corner, small_size, DT_RIGHT);
    }
    int sum = code_digit_sum(code);
    if (show_sum && sum >= 0) {
        wchar_t label[32];
        swprintf(label, 32, L"de som is %d", sum);
        ok = ok && draw_line(dc, label, sum_box, small_size, DT_LEFT);
    }
    GdiFlush();
    SelectObject(dc, old);
    DeleteDC(dc);
    if (!ok) { DeleteObject(bitmap); return NULL; }
    return bitmap;
}

static int pm_mode(HWND window) {
    return SendDlgItemMessageW(window, IDC_MODE, CB_GETCURSEL, 0, 0) == 1;
}
static void locator_changed(HWND window) {
    if (pm_mode(window) || IsDlgButtonChecked(window, IDC_AUTO) != BST_CHECKED) return;
    wchar_t locator[LOCATOR_MAX_LENGTH+1];
    read_text(window, IDC_LOCATOR, locator, LOCATOR_MAX_LENGTH+1);
    if (locator_square_changed(contest_square, locator)) new_code(window);
}
static void update_mode(HWND window) {
    int pm = pm_mode(window);
    locator_changed(window);
    const int controls[] = {IDC_AUTO, IDC_MANUAL, IDC_CODE, IDC_INVERSE, IDC_BLUE_YELLOW, IDC_BAND,
                            IDC_SHOW_SUM, IDC_TOP_CODE, IDC_SHOW_LOCATOR};
    for (size_t i=0; i<sizeof(controls)/sizeof(controls[0]); ++i)
        EnableWindow(GetDlgItem(window, controls[i]), !pm);
    EnableWindow(GetDlgItem(window, IDC_NEW_CODE), !pm && IsDlgButtonChecked(window, IDC_AUTO) == BST_CHECKED);
    EnableWindow(GetDlgItem(window, IDC_LOCATOR), pm || IsDlgButtonChecked(window, IDC_SHOW_LOCATOR) == BST_CHECKED);
}
static HBITMAP render_pm(Resolution r, const wchar_t *call, const wchar_t *locator) {
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = r.width;
    info.bmiHeader.biHeight = -r.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels;
    HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (bitmap && !pm5544_render(pixels, r.width, r.height, call, locator)) {
        DeleteObject(bitmap); return NULL;
    }
    return bitmap;
}

/* Native Windows JPEG encoder: no additional DLLs are distributed. */
static HRESULT save_jpeg(HBITMAP bitmap, const wchar_t *path) {
    IWICStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IPropertyBag2 *options = NULL;
    IWICBitmap *source = NULL;
    IWICFormatConverter *converter = NULL;
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    HRESULT hr;
#define CHECK(expr) do { hr = (expr); if (FAILED(hr)) goto cleanup; } while (0)
    CHECK(IWICImagingFactory_CreateStream(factory, &stream));
    CHECK(IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE));
    CHECK(IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatJpeg, NULL, &encoder));
    CHECK(IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache));
    CHECK(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &options));
    PROPBAG2 property = {0};
    VARIANT quality;
    property.pstrName = L"ImageQuality";
    VariantInit(&quality);
    V_VT(&quality) = VT_R4;
    V_R4(&quality) = 0.98f;
    CHECK(IPropertyBag2_Write(options, 1, &property, &quality));
    CHECK(IWICBitmapFrameEncode_Initialize(frame, options));
    CHECK(IWICBitmapFrameEncode_SetPixelFormat(frame, &format));
    CHECK(IWICImagingFactory_CreateBitmapFromHBITMAP(factory, bitmap, NULL, WICBitmapIgnoreAlpha, &source));
    CHECK(IWICImagingFactory_CreateFormatConverter(factory, &converter));
    CHECK(IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)source, &format,
        WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom));
    CHECK(IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)converter, NULL));
    CHECK(IWICBitmapFrameEncode_Commit(frame));
    CHECK(IWICBitmapEncoder_Commit(encoder));
cleanup:
    if (converter) IWICFormatConverter_Release(converter);
    if (source) IWICBitmap_Release(source);
    if (options) IPropertyBag2_Release(options);
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (encoder) IWICBitmapEncoder_Release(encoder);
    if (stream) IWICStream_Release(stream);
    return hr;
#undef CHECK
}
static void error(HWND window, const wchar_t *message) {
    MessageBoxW(window, message, L"ATV contestnummer", MB_OK | MB_ICONERROR);
}
static void ts_error(HWND window, const char *message) {
    wchar_t text[256]; MultiByteToWideChar(CP_UTF8,0,message,-1,text,256); error(window,text);
}
typedef struct {
    HBITMAP bitmap;
    Resolution size;
    char call[25];
    DatvStream *stream;
} UdpDialog;
static void udp_status_message(HWND window,const char *message) {
    wchar_t text[512]; MultiByteToWideChar(CP_UTF8,0,message,-1,text,512);
    SetDlgItemTextW(window,IDC_UDP_STATUS,text);
}
static void udp_poll(HWND window,UdpDialog *d) {
    if (!d->stream) return;
    DatvUdpStatus status; datv_udp_status(d->stream,&status);
    char text[512]; datv_udp_status_text(udp_settings,status,text,sizeof(text));
    udp_status_message(window,text);
    BOOL busy=status.state==DATV_PREPARING || status.state==DATV_RUNNING;
    const int fields[]={IDC_UDP_IP,IDC_UDP_PORT,IDC_UDP_BITRATE,IDC_UDP_FPS,IDC_UDP_GOP,IDC_UDP_START};
    for (int i=0;i<6;++i) EnableWindow(GetDlgItem(window,fields[i]),!busy);
    EnableWindow(GetDlgItem(window,IDC_UDP_STOP),busy);
}
static INT_PTR CALLBACK udp_dialog(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    UdpDialog *d=(UdpDialog *)GetWindowLongPtrW(window,DWLP_USER);
    if (message==WM_INITDIALOG) {
        d=(UdpDialog *)lp; SetWindowLongPtrW(window,DWLP_USER,lp);
        SetDlgItemTextA(window,IDC_UDP_IP,udp_settings.ip);
        SendDlgItemMessageW(window,IDC_UDP_IP,EM_SETLIMITTEXT,15,0);
        SetDlgItemInt(window,IDC_UDP_PORT,udp_settings.port,FALSE);
        SetDlgItemInt(window,IDC_UDP_BITRATE,udp_settings.video.bitrate,FALSE);
        SetDlgItemInt(window,IDC_UDP_FPS,udp_settings.video.fps,FALSE);
        SetDlgItemInt(window,IDC_UDP_GOP,udp_settings.video.gop,FALSE);
        EnableWindow(GetDlgItem(window,IDC_UDP_STOP),FALSE);
        if (!SetTimer(window,1,200,NULL)) { EndDialog(window,0); return TRUE; }
        return TRUE;
    }
    if (message==WM_TIMER) { udp_poll(window,d); return TRUE; }
    if (message==WM_CLOSE || (message==WM_COMMAND && LOWORD(wp)==IDCANCEL)) {
        KillTimer(window,1); datv_udp_destroy(d->stream); d->stream=NULL;
        EndDialog(window,0); return TRUE;
    }
    if (message==WM_COMMAND && LOWORD(wp)==IDC_UDP_STOP) {
        datv_udp_stop(d->stream); return TRUE;
    }
    if (message==WM_COMMAND && LOWORD(wp)==IDC_UDP_START) {
        if (d->stream) {
            DatvUdpStatus status; datv_udp_status(d->stream,&status);
            if (status.state==DATV_PREPARING || status.state==DATV_RUNNING) return TRUE;
            datv_udp_destroy(d->stream); d->stream=NULL;
        }
        DatvUdpSettings s=udp_settings;
        GetDlgItemTextA(window,IDC_UDP_IP,s.ip,sizeof(s.ip));
        char text[3][32]; const char *values[4];
        int fields[]={IDC_UDP_BITRATE,IDC_UDP_FPS,IDC_UDP_GOP};
        for (int i=0;i<3;++i) GetDlgItemTextA(window,fields[i],text[i],32);
        values[0]=text[0]; values[1]="10"; values[2]=text[1]; values[3]=text[2];
        int w,h; BOOL valid=FALSE;
        s.port=(int)GetDlgItemInt(window,IDC_UDP_PORT,&valid,FALSE);
        if (!valid || !datv_test_options(4,values,&s.video,&w,&h)) {
            udp_status_message(window,"Gebruik poort 1-65535, bitrate 48000-2000000 bit/s, 1-25 beelden/s en GOP 1-250."); return TRUE;
        }
        const char *error=datv_udp_validate(s,d->size.width,d->size.height);
        if (error) { udp_status_message(window,error); return TRUE; }
        DIBSECTION dib={0};
        if (GetObjectW(d->bitmap,sizeof(dib),&dib)!=sizeof(dib) || !dib.dsBm.bmBits) {
            udp_status_message(window,"Kan het beeld niet lezen."); return TRUE;
        }
        datv_udp_destroy(d->stream);
        char detail[256];
        d->stream=datv_udp_start(dib.dsBm.bmBits,d->size.width,d->size.height,dib.dsBm.bmWidthBytes,d->call,s,detail);
        if (!d->stream) udp_status_message(window,detail);
        else { udp_settings=s; udp_poll(window,d); }
        return TRUE;
    }
    return FALSE;
}
static void output_udp(HWND window) {
    if (genius_level!=2) return;
    wchar_t call[25],locator[LOCATOR_MAX_LENGTH+1],code[5];
    read_text(window,IDC_CALL,call,25); read_text(window,IDC_LOCATOR,locator,LOCATOR_MAX_LENGTH+1); read_text(window,IDC_CODE,code,5);
    BOOL pm=pm_mode(window), show=pm||IsDlgButtonChecked(window,IDC_SHOW_LOCATOR)==BST_CHECKED;
    if (!valid_call(call) || ((show||locator[0])&&!valid_locator(locator)) || (!pm&&!valid_code(code))) {
        error(window,L"Vul eerst een geldige roepnaam, locator en contestcode in."); return;
    }
    UdpDialog d={0}; d.size=resolution(window);
    const char *invalid=datv_validate(udp_settings.video,d.size.width,d.size.height);
    if (invalid) { ts_error(window,invalid); return; }
    int band=(int)SendDlgItemMessageW(window,IDC_BAND,CB_GETCURSEL,0,0);
    if (band<0 || band>10) return;
    WideCharToMultiByte(CP_UTF8,0,call,-1,d.call,sizeof(d.call),NULL,NULL);
    d.bitmap=pm?render_pm(d.size,call,locator):render(d.size.width,d.size.height,call,code,locator,show,bands[band],
        IsDlgButtonChecked(window,IDC_INVERSE)==BST_CHECKED,IsDlgButtonChecked(window,IDC_SHOW_SUM)==BST_CHECKED,
        IsDlgButtonChecked(window,IDC_TOP_CODE)==BST_CHECKED,IsDlgButtonChecked(window,IDC_BLUE_YELLOW)==BST_CHECKED);
    if (!d.bitmap) { error(window,L"Kan het beeld niet maken."); return; }
    HINSTANCE instance=(HINSTANCE)GetWindowLongPtrW(window,GWLP_HINSTANCE);
    if (DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_UDP),window,udp_dialog,(LPARAM)&d)==-1)
        error(window,L"Kan het UDP-venster niet openen.");
    datv_udp_destroy(d.stream); DeleteObject(d.bitmap);
}
typedef struct {
    HBITMAP bitmap;
    wchar_t path[MAX_PATH];
    char call[25], error[256];
    DatvSettings settings;
    DatvResult result;
    BOOL overwrite;
    HANDLE thread;
    int ok;
} TsJob;
static DWORD WINAPI ts_worker(LPVOID data) {
    TsJob *job=data;
    wchar_t directory[MAX_PATH],temporary[MAX_PATH];
    DWORD path_length=GetFullPathNameW(job->path,MAX_PATH,directory,NULL);
    if (!path_length || path_length>=MAX_PATH) goto failure;
    wchar_t *last=wcsrchr(directory,L'\\');
    if (!last) goto failure;
    last[1]=0;
    if (!GetTempFileNameW(directory,L"atv",0,temporary)) goto failure;
    FILE *file=_wfopen(temporary,L"wb");
    DIBSECTION dib={0};
    int ok=file && GetObjectW(job->bitmap,sizeof(dib),&dib)==sizeof(dib) && dib.dsBm.bmBits &&
        datv_write(file,dib.dsBm.bmBits,dib.dsBm.bmWidth,dib.dsBm.bmHeight,dib.dsBm.bmWidthBytes,
            job->call,job->settings,&job->result,job->error);
    if (file && fclose(file)!=0) ok=0;
    if (ok && !MoveFileExW(temporary,job->path,MOVEFILE_WRITE_THROUGH|(job->overwrite?MOVEFILE_REPLACE_EXISTING:0))) ok=0;
    DeleteFileW(temporary);
    if (ok) { job->ok=1; return 0; }
failure:
    if (!job->error[0]) strcpy(job->error,"TS opslaan is mislukt. Controleer pad, vrije ruimte en schrijfrechten.");
    return 1;
}
static INT_PTR CALLBACK ts_progress(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    (void)wp;
    TsJob *job=(TsJob *)GetWindowLongPtrW(window,DWLP_USER);
    if (message==WM_INITDIALOG) {
        job=(TsJob *)lp; SetWindowLongPtrW(window,DWLP_USER,lp);
        job->thread=CreateThread(NULL,0,ts_worker,job,0,NULL);
        if (!job->thread) { strcpy(job->error,"Kan de TS-exporttaak niet starten."); EndDialog(window,0); }
        else if (!SetTimer(window,1,100,NULL)) {
            WaitForSingleObject(job->thread,INFINITE); CloseHandle(job->thread); EndDialog(window,job->ok);
        }
        return TRUE;
    }
    if (message==WM_TIMER && job && WaitForSingleObject(job->thread,0)==WAIT_OBJECT_0) {
        KillTimer(window,1); CloseHandle(job->thread); EndDialog(window,job->ok); return TRUE;
    }
    if (message==WM_CLOSE || message==WM_COMMAND) return TRUE;
    return FALSE;
}
static INT_PTR CALLBACK ts_options(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (message==WM_INITDIALOG) {
        SetDlgItemInt(window,IDC_TS_BITRATE,ts_settings.bitrate,FALSE);
        SetDlgItemInt(window,IDC_TS_SECONDS,ts_settings.seconds,FALSE);
        SetDlgItemInt(window,IDC_TS_FPS,ts_settings.fps,FALSE);
        SetDlgItemInt(window,IDC_TS_GOP,ts_settings.gop,FALSE);
        return TRUE;
    }
    if (message==WM_COMMAND && LOWORD(wp)==IDOK) {
        const int ids[]={IDC_TS_BITRATE,IDC_TS_SECONDS,IDC_TS_FPS,IDC_TS_GOP};
        char text[4][32]; const char *values[4];
        for (int i=0; i<4; ++i) { GetDlgItemTextA(window,ids[i],text[i],32); values[i]=text[i]; }
        DatvSettings s; int w,h;
        if (!datv_test_options(4,values,&s,&w,&h)) {
            error(window,L"Gebruik bitrate 48000-2000000 bit/s, duur 1-60 s, 1-25 beelden/s en GOP 1-250."); return TRUE;
        }
        ts_settings=s; EndDialog(window,IDOK); return TRUE;
    }
    if (message==WM_CLOSE || (message==WM_COMMAND && LOWORD(wp)==IDCANCEL)) { EndDialog(window,IDCANCEL); return TRUE; }
    return FALSE;
}
static void export_ts(HWND window) {
    if (genius_level!=2) return;
    wchar_t call[25],locator[LOCATOR_MAX_LENGTH+1],code[5],safe[25];
    read_text(window,IDC_CALL,call,25); read_text(window,IDC_LOCATOR,locator,LOCATOR_MAX_LENGTH+1); read_text(window,IDC_CODE,code,5);
    BOOL pm=pm_mode(window), show=pm||IsDlgButtonChecked(window,IDC_SHOW_LOCATOR)==BST_CHECKED;
    if (!valid_call(call) || ((show||locator[0])&&!valid_locator(locator)) || (!pm&&!valid_code(code))) {
        error(window,L"Vul eerst een geldige roepnaam, locator en contestcode in."); return;
    }
    Resolution r=resolution(window);
    const char *invalid=datv_validate(ts_settings,r.width,r.height);
    if (invalid) { ts_error(window,invalid); return; }
    HINSTANCE instance=(HINSTANCE)GetWindowLongPtrW(window,GWLP_HINSTANCE);
    if (DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_TS),window,ts_options,0)!=IDOK) return;
    TsJob job={0}; job.settings=ts_settings;
    WideCharToMultiByte(CP_UTF8,0,call,-1,job.call,25,NULL,NULL);
    filename_call(safe,call);
    swprintf(job.path,MAX_PATH,L"%ls-%ls-%dx%d-%dbps.ts",safe,pm?L"PM5544":code,r.width,r.height,ts_settings.bitrate);
    OPENFILENAMEW picker={0}; picker.lStructSize=sizeof(picker); picker.hwndOwner=window;
    picker.lpstrFilter=L"MPEG-TS (*.ts)\0*.ts\0\0"; picker.lpstrFile=job.path; picker.nMaxFile=MAX_PATH;
    picker.lpstrTitle=L"TS opslaan"; picker.lpstrDefExt=L"ts";
    picker.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&picker)) {
        if (CommDlgExtendedError()) error(window,L"Het opslagvenster kon niet worden geopend.");
        return;
    }
    job.overwrite=GetFileAttributesW(job.path)!=INVALID_FILE_ATTRIBUTES;
    int band=(int)SendDlgItemMessageW(window,IDC_BAND,CB_GETCURSEL,0,0);
    if (band<0 || band>10) return;
    job.bitmap=pm?render_pm(r,call,locator):render(r.width,r.height,call,code,locator,show,bands[band],
        IsDlgButtonChecked(window,IDC_INVERSE)==BST_CHECKED,IsDlgButtonChecked(window,IDC_SHOW_SUM)==BST_CHECKED,
        IsDlgButtonChecked(window,IDC_TOP_CODE)==BST_CHECKED,IsDlgButtonChecked(window,IDC_BLUE_YELLOW)==BST_CHECKED);
    if (!job.bitmap) { error(window,L"Kan het beeld niet maken."); return; }
    if (DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_TS_PROGRESS),window,ts_progress,(LPARAM)&job)==-1)
        strcpy(job.error,"Kan het voortgangsvenster niet openen.");
    DeleteObject(job.bitmap);
    if (!job.ok) ts_error(window,job.error);
    else {
        wchar_t message[512];
        swprintf(message,512,L"TS opgeslagen: %ls (QP %d, %d beelden, grootste IDR %d bytes)",job.path,job.result.qp,job.result.frames,job.result.largest_idr);
        SetDlgItemTextW(window,IDC_STATUS,message);
    }
}
static void generate(HWND window, BOOL choose_path) {
    wchar_t call[25], locator[LOCATOR_MAX_LENGTH+1], code[5], safe_call[25];
    wchar_t path[MAX_PATH], temporary[MAX_PATH], filename[100], message[400];
    read_text(window, IDC_CALL, call, 25);
    read_text(window, IDC_LOCATOR, locator, LOCATOR_MAX_LENGTH+1);
    read_text(window, IDC_CODE, code, 5);
    int pm = pm_mode(window);
    int show = pm || IsDlgButtonChecked(window, IDC_SHOW_LOCATOR) == BST_CHECKED;
    int inverse = IsDlgButtonChecked(window, IDC_INVERSE) == BST_CHECKED;
    int blue_yellow = IsDlgButtonChecked(window, IDC_BLUE_YELLOW) == BST_CHECKED;
    if (!valid_call(call)) { error(window, L"Vul een roepnaam in met letters en cijfers, eventueel met / (3 tot 24 tekens)."); return; }
    if ((show || locator[0]) && !valid_locator(locator)) { error(window, L"Vul een geldige Maidenheadlocator in, bijvoorbeeld JO21QK (4, 6, 8, 10 of 12 tekens)."); return; }
    if (!pm && !valid_code(code)) { error(window, L"Vul vier cijfers in of klik op Nieuw nummer."); return; }
    int band = (int)SendDlgItemMessageW(window, IDC_BAND, CB_GETCURSEL, 0, 0);
    if (band < 0 || band > 10) return;
    filename_call(safe_call, call);
    Resolution r = resolution(window);
    wchar_t short_locator[7], locator_suffix[8] = L"";
    filename_locator(short_locator, locator);
    if (short_locator[0]) swprintf(locator_suffix, 8, L"-%ls", short_locator);
    if (pm) swprintf(filename, 100, L"%ls%ls-PM5544-%dx%d.jpg", safe_call, locator_suffix, r.width, r.height);
    else swprintf(filename, 100, L"%ls%ls-%ls-%ls-%dx%d%ls%ls.jpg", safe_call, locator_suffix, code, band_files[band], r.width, r.height, contest_color_suffix(blue_yellow), inverse ? L"-inverse" : L"");
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH) { error(window, L"Het pad naar het programma is te lang."); return; }
    wchar_t *last = wcsrchr(path, L'\\');
    if (!last) { error(window, L"De programmamap is niet gevonden."); return; }
    last[1] = 0;
    if (choose_path) {
        wchar_t directory[MAX_PATH];
        wcscpy(directory, path);
        wcscpy(path, filename);
        OPENFILENAMEW picker = {0};
        picker.lStructSize = sizeof(picker);
        picker.hwndOwner = window;
        picker.lpstrFilter = L"JPG-afbeeldingen (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0\0";
        picker.lpstrFile = path;
        picker.nMaxFile = MAX_PATH;
        picker.lpstrInitialDir = directory;
        picker.lpstrTitle = L"Exporteren naar...";
        picker.lpstrDefExt = L"jpg";
        picker.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&picker)) {
            if (CommDlgExtendedError()) error(window, L"Het opslagvenster kon niet worden geopend of het gekozen pad is te lang.");
            return;
        }
    } else {
        if (wcslen(path) + wcslen(filename) >= MAX_PATH) { error(window, L"De bestandsnaam is te lang. Gebruik Exporteren naar... voor een korter pad."); return; }
        wcscat(path, filename);
    }
    BOOL overwrite = GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
    if (overwrite) {
        swprintf(message, 400, L"%ls bestaat al. Wil je dit bestand vervangen?", path);
        if (MessageBoxW(window, message, L"Bestand bestaat al", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    }
    /* Encode in the destination directory before publishing the complete image. */
    wchar_t directory[MAX_PATH];
    wcscpy(directory, path);
    last = wcsrchr(directory, L'\\');
    if (!last) { error(window, L"De uitvoermap is niet gevonden."); return; }
    last[1] = 0;
    if (!GetTempFileNameW(directory, L"atv", 0, temporary)) {
        error(window, L"Kan niet schrijven in de gekozen map. Controleer de schrijfrechten of kies een andere map via Exporteren naar..."); return;
    }
    HBITMAP bitmap = pm ? render_pm(r, call, locator) : render(r.width, r.height, call, code, locator, show, bands[band], inverse,
        IsDlgButtonChecked(window, IDC_SHOW_SUM) == BST_CHECKED,
        IsDlgButtonChecked(window, IDC_TOP_CODE) == BST_CHECKED,
        IsDlgButtonChecked(window, IDC_BLUE_YELLOW) == BST_CHECKED);
    HRESULT hr = bitmap ? save_jpeg(bitmap, temporary) : E_OUTOFMEMORY;
    if (bitmap) DeleteObject(bitmap);
    if (SUCCEEDED(hr) && !MoveFileExW(temporary, path, overwrite ? MOVEFILE_REPLACE_EXISTING : 0)) hr = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(hr)) {
        DeleteFileW(temporary);
        swprintf(message, 400, L"Opslaan is mislukt (0x%08lX). Controleer vrije ruimte en schrijfrechten.", (unsigned long)hr);
        error(window, message); return;
    }
    swprintf(message, 400, L"Opgeslagen: %ls (%d x %d)", path, r.width, r.height);
    SetDlgItemTextW(window, IDC_STATUS, message);
}
static void preview(HWND window, DRAWITEMSTRUCT *item) {
    wchar_t call[25], locator[LOCATOR_MAX_LENGTH+1], code[5];
    read_text(window, IDC_CALL, call, 25);
    read_text(window, IDC_LOCATOR, locator, LOCATOR_MAX_LENGTH+1);
    read_text(window, IDC_CODE, code, 5);
    FillRect(item->hDC, &item->rcItem, GetSysColorBrush(COLOR_3DFACE));
    Resolution r = resolution(window);
    int band = (int)SendDlgItemMessageW(window, IDC_BAND, CB_GETCURSEL, 0, 0);
    if (band < 0 || band > 10) band = 3;
    HBITMAP bitmap = pm_mode(window) ? render_pm(r, call, locator) : render(r.width, r.height, call, code, locator,
        IsDlgButtonChecked(window, IDC_SHOW_LOCATOR) == BST_CHECKED, bands[band],
        IsDlgButtonChecked(window, IDC_INVERSE) == BST_CHECKED,
        IsDlgButtonChecked(window, IDC_SHOW_SUM) == BST_CHECKED,
        IsDlgButtonChecked(window, IDC_TOP_CODE) == BST_CHECKED,
        IsDlgButtonChecked(window, IDC_BLUE_YELLOW) == BST_CHECKED);
    if (!bitmap) return;
    HDC dc = CreateCompatibleDC(item->hDC);
    if (!dc) { DeleteObject(bitmap); return; }
    HGDIOBJ old = SelectObject(dc, bitmap);
    int w = item->rcItem.right-item->rcItem.left, h = w*r.height/r.width;
    int available_h = item->rcItem.bottom-item->rcItem.top;
    if (h > available_h) { h = available_h; w = h*r.width/r.height; }
    int x = item->rcItem.left + (item->rcItem.right-item->rcItem.left-w)/2;
    int y = item->rcItem.top + (available_h-h)/2;
    SetStretchBltMode(item->hDC, HALFTONE);
    SetBrushOrgEx(item->hDC, 0, 0, NULL);
    StretchBlt(item->hDC, x, y, w, h, dc, 0, 0, r.width, r.height, SRCCOPY);
    SelectObject(dc, old);
    DeleteDC(dc);
    DeleteObject(bitmap);
}
static void show_about(HWND window) {
    wchar_t text[2048];
    MultiByteToWideChar(CP_UTF8, 0, app_info_text(), -1, text, 2048);
    MessageBoxW(window, text, L"Over dit programma", MB_OK | MB_ICONINFORMATION);
}
static INT_PTR CALLBACK dialog(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_INITDIALOG: {
        ready = 0;
        ts_settings=datv_defaults(); udp_settings=datv_udp_defaults(); genius_level=1;
        contest_square[0] = 0;
        SendDlgItemMessageW(window, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)L"Contest");
        SendDlgItemMessageW(window, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)L"PM5544");
        SendDlgItemMessageW(window, IDC_MODE, CB_SETCURSEL, 0, 0);
        SendDlgItemMessageW(window, IDC_CALL, EM_SETLIMITTEXT, 24, 0);
        SendDlgItemMessageW(window, IDC_LOCATOR, EM_SETLIMITTEXT, LOCATOR_MAX_LENGTH, 0);
        SendDlgItemMessageW(window, IDC_CODE, EM_SETLIMITTEXT, 4, 0);
        CheckDlgButton(window, IDC_SHOW_LOCATOR, BST_CHECKED);
        CheckRadioButton(window, IDC_AUTO, IDC_MANUAL, IDC_AUTO);
        SendDlgItemMessageW(window, IDC_CODE, EM_SETREADONLY, TRUE, 0);
        new_code(window);
        SendDlgItemMessageW(window, IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)L"4:3");
        SendDlgItemMessageW(window, IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)L"16:9");
        SendDlgItemMessageW(window, IDC_ASPECT, CB_SETCURSEL, 0, 0);
        update_resolutions(window);
        for (int i=0; bands[i]; ++i) SendDlgItemMessageW(window, IDC_BAND, CB_ADDSTRING, 0, (LPARAM)bands[i]);
        SendDlgItemMessageW(window, IDC_BAND, CB_SETCURSEL, 3, 0);
        SetDlgItemTextW(window, IDC_STATUS, L"Vul je gegevens in. Exporteer JPG slaat het beeld naast de .exe op.");
        ready = 1;
        return TRUE;
    }
    case WM_DRAWITEM:
        if (ready && wp == IDC_PREVIEW) { preview(window, (DRAWITEMSTRUCT *)lp); return TRUE; }
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL) { EndDialog(window, 0); return TRUE; }
        if (!ready) break;
        if (LOWORD(wp)==IDM_LEVEL1 || LOWORD(wp)==IDM_LEVEL2) {
            int level=LOWORD(wp)==IDM_LEVEL2?2:1;
            HMENU menu=GetMenu(window), file=GetSubMenu(menu,0);
            if (level!=genius_level) {
                if (level==2) {
                    InsertMenuW(file,2,MF_BYPOSITION|MF_STRING,IDM_EXPORT_TS,L"Exporteer TS-proefbestand...");
                    InsertMenuW(file,3,MF_BYPOSITION|MF_STRING,IDM_UDP,L"DATV UDP-uitvoer...");
                } else {
                    DeleteMenu(file,IDM_EXPORT_TS,MF_BYCOMMAND); DeleteMenu(file,IDM_UDP,MF_BYCOMMAND);
                }
                genius_level=level;
            }
            CheckMenuRadioItem(GetSubMenu(menu,1),IDM_LEVEL1,IDM_LEVEL2,level==2?IDM_LEVEL2:IDM_LEVEL1,MF_BYCOMMAND);
            DrawMenuBar(window); return TRUE;
        }
        if (LOWORD(wp)==IDM_EXPORT_TS) { export_ts(window); return TRUE; }
        if (LOWORD(wp)==IDM_UDP) { output_udp(window); return TRUE; }
        if (LOWORD(wp)==IDM_CODEC_LICENSE) {
            wchar_t text[2048]; MultiByteToWideChar(CP_UTF8,0,app_codec_license(),-1,text,2048);
            MessageBoxW(window,text,L"OpenH264-licentie",MB_OK|MB_ICONINFORMATION); return TRUE;
        }
        if (LOWORD(wp) == IDM_QUIT) { EndDialog(window, 0); return TRUE; }
        if (LOWORD(wp) == IDM_EXPORT) { generate(window, FALSE); return TRUE; }
        if (LOWORD(wp) == IDM_EXPORT_AS) { generate(window, TRUE); return TRUE; }
        if (LOWORD(wp) == IDM_ABOUT) { show_about(window); return TRUE; }
        if (LOWORD(wp) == IDC_LOCATOR && HIWORD(wp) == EN_CHANGE) locator_changed(window);
        if (LOWORD(wp) == IDC_NEW_CODE && HIWORD(wp) == BN_CLICKED) new_code(window);
        if (LOWORD(wp) == IDC_GENERATE && HIWORD(wp) == BN_CLICKED) generate(window, FALSE);
        if (LOWORD(wp) == IDC_MODE && HIWORD(wp) == CBN_SELCHANGE) update_mode(window);
        if (LOWORD(wp) == IDC_ASPECT && HIWORD(wp) == CBN_SELCHANGE) update_resolutions(window);
        if ((LOWORD(wp) == IDC_AUTO || LOWORD(wp) == IDC_MANUAL) && HIWORD(wp) == BN_CLICKED) {
            SendDlgItemMessageW(window, IDC_CODE, EM_SETREADONLY, IsDlgButtonChecked(window, IDC_AUTO) == BST_CHECKED, 0);
            update_mode(window);
            if (LOWORD(wp) == IDC_AUTO) new_code(window);
        }
        if (LOWORD(wp) == IDC_SHOW_LOCATOR && HIWORD(wp) == BN_CLICKED)
            EnableWindow(GetDlgItem(window, IDC_LOCATOR), IsDlgButtonChecked(window, IDC_SHOW_LOCATOR) == BST_CHECKED);
        if (HIWORD(wp) == EN_CHANGE || HIWORD(wp) == CBN_SELCHANGE || HIWORD(wp) == BN_CLICKED)
            InvalidateRect(GetDlgItem(window, IDC_PREVIEW), NULL, FALSE);
        return TRUE;
    case WM_CLOSE: EndDialog(window, 0); return TRUE;
    }
    return FALSE;
}

/* Headless rendering path used by integration checks; output directory must exist. */
static int smoke_test(const wchar_t *directory) {
    wchar_t path[MAX_PATH];
    for (int i=0; i<1000; ++i) {
        wchar_t code[5];
        if (!random_code(code) || !valid_code(code) || !generated_code_valid((unsigned)wcstoul(code, NULL, 10))) return 5;
    }
    for (int blue_yellow=0; blue_yellow<2; ++blue_yellow)
    for (int top_code=0; top_code<2; ++top_code)
    for (int show_sum=0; show_sum<2; ++show_sum)
    for (int inverse=0; inverse<2; ++inverse)
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int show=0; show<2; ++show) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        if (swprintf(path, MAX_PATH, L"%ls\\test-%dx%d-locator%d%ls%ls%ls%ls.jpg", directory, r.width, r.height, show, contest_color_suffix(blue_yellow), inverse ? L"-inverse" : L"", show_sum ? L"-sum" : L"", top_code ? L"-top" : L"") < 0) return 2;
        HBITMAP bitmap = render(r.width, r.height, L"PE1ITR/P", L"1957", L"JO21QK86DV", show, L"436 MHz", inverse, show_sum, top_code, blue_yellow);
        if (!bitmap) return 3;
        HRESULT hr = save_jpeg(bitmap, path);
        DeleteObject(bitmap);
        if (FAILED(hr)) return 4;
    }
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int text=0; text<2; ++text) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        if (swprintf(path, MAX_PATH, L"%ls\\pm-%dx%d-%d.jpg", directory, r.width, r.height, text) < 0) return 2;
        HBITMAP bitmap = render_pm(r, text ? L"PE1ITR/P" : L"", text ? L"JO21QK86DV" : L"");
        if (!bitmap) return 6;
        HRESULT hr = save_jpeg(bitmap, path);
        DeleteObject(bitmap);
        if (FAILED(hr)) return 7;
    }
    return 0;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
    (void)previous; (void)show;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { error(NULL, L"De Windows JPEG-encoder kan niet worden gestart."); CoUninitialize(); return 1; }
    int result;
    int argc=0;
    LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if (argv && argc>=3 && !wcscmp(argv[1],L"--ts-test")) {
        char text[6][32]; const char *values[6];
        result=1;
        if (argc<=9) {
            BOOL converted=TRUE;
            for (int i=3; i<argc; ++i) {
                text[i-3][0]=0;
                if (!WideCharToMultiByte(CP_UTF8,0,argv[i],-1,text[i-3],32,NULL,NULL)) converted=FALSE;
                values[i-3]=text[i-3];
            }
            TsJob job={0}; int w,h;
            DWORD length=GetFullPathNameW(argv[2],MAX_PATH,job.path,NULL);
            if (converted && length && length<MAX_PATH && datv_test_options(argc-3,values,&job.settings,&w,&h)) {
                strcpy(job.call,"PE1ITR");
                job.bitmap=render(w,h,L"PE1ITR",L"1957",L"JO21QK",TRUE,L"436 MHz",FALSE,TRUE,TRUE,FALSE);
                if (job.bitmap) { ts_worker(&job); DeleteObject(job.bitmap); result=job.ok?0:1; }
            }
        }
    }
    else if (wcsncmp(command, L"--smoke-test ", 13) == 0) result = smoke_test(command+13);
    else {
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&controls);
        result = (int)DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), NULL, dialog, 0);
    }
    if (argv) LocalFree(argv);
    pm5544_cleanup();
    IWICImagingFactory_Release(factory);
    CoUninitialize();
    return result == -1 ? 1 : result;
}
