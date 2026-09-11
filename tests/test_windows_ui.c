#include "../src/main.c"
#include <assert.h>

static HWND about_owner;
static int about_seen;
static BOOL CALLBACK inspect_about(HWND child, LPARAM unused) {
    (void)unused;
    wchar_t text[2048], expected[2048];
    GetWindowTextW(child, text, 2048);
    MultiByteToWideChar(CP_UTF8, 0, app_info_text(), -1, expected, 2048);
    if (!wcscmp(text, expected)) about_seen = 1;
    return TRUE;
}
static BOOL CALLBACK close_about(HWND window, LPARAM unused) {
    (void)unused;
    if (GetWindow(window, GW_OWNER) == about_owner) {
        EnumChildWindows(window, inspect_about, 0);
        if (about_seen) PostMessageW(window, WM_COMMAND, IDOK, 0);
    }
    return TRUE;
}
static void CALLBACK about_timer(HWND window, UINT message, UINT_PTR id, DWORD time) {
    (void)window; (void)message; (void)id; (void)time;
    EnumThreadWindows(GetCurrentThreadId(), close_about, 0);
}

int wmain(void) {
    assert(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)));
    assert(SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory, (void **)&factory)));
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    HWND window = CreateDialogParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_MAIN), NULL, dialog, 0);
    assert(window);
    assert(GetMenuItemCount(GetMenu(window)) == 2);
    assert(GetMenuItemID(GetSubMenu(GetMenu(window), 0), 0) == IDM_EXPORT);
    assert(GetMenuItemID(GetSubMenu(GetMenu(window), 1), 0) == IDM_ABOUT);
    about_owner = window;
    UINT_PTR timer = SetTimer(NULL, 0, 50, about_timer);
    assert(timer);
    SendMessageW(window, WM_COMMAND, IDM_ABOUT, 0);
    KillTimer(NULL, timer);
    assert(about_seen);
    wchar_t call[25], path[MAX_PATH];
    swprintf(call, 25, L"TEST%lu", GetCurrentProcessId());
    SetDlgItemTextW(window, IDC_CALL, call);
    SetDlgItemTextW(window, IDC_LOCATOR, L"JO21QK");
    wchar_t initial[5], next[5];
    GetDlgItemTextW(window, IDC_CODE, initial, 5);
    assert(valid_code(initial) && generated_code_valid((unsigned)wcstoul(initial, NULL, 10)));
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, 2, 0);
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *auto_name = wcsrchr(path, L'\\')+1;
    swprintf(auto_name, MAX_PATH-(auto_name-path), L"%ls-%ls-436MHz-320x240.jpg", call, initial);
    assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
    SendMessageW(window, WM_COMMAND, IDM_EXPORT, 0);
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    GetDlgItemTextW(window, IDC_CODE, next, 5);
    assert(!wcscmp(initial, next));
    DeleteFileW(path);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_NEW_CODE, BN_CLICKED), 0);
    GetDlgItemTextW(window, IDC_CODE, next, 5);
    assert(valid_code(next) && wcscmp(initial, next));
    SetDlgItemTextW(window, IDC_CODE, L"----");
    CheckDlgButton(window, IDC_SHOW_LOCATOR, BST_UNCHECKED);
    CheckDlgButton(window, IDC_INVERSE, BST_CHECKED);
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, 2, 0);
    SendDlgItemMessageW(window, IDC_MODE, CB_SETCURSEL, 1, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_MODE, CBN_SELCHANGE), 0);
    const int ids[] = {IDC_AUTO, IDC_MANUAL, IDC_CODE, IDC_INVERSE, IDC_BAND, IDC_SHOW_SUM, IDC_TOP_CODE, IDC_SHOW_LOCATOR, IDC_NEW_CODE};
    for (size_t i=0; i<sizeof(ids)/sizeof(ids[0]); ++i) assert(!IsWindowEnabled(GetDlgItem(window, ids[i])));
    assert(IsWindowEnabled(GetDlgItem(window, IDC_LOCATOR)));
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *name = wcsrchr(path, L'\\')+1;
    swprintf(name, MAX_PATH-(name-path), L"%ls-PM5544-320x240.jpg", call);
    assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
    SendMessageW(window, WM_COMMAND, IDM_EXPORT, 0);
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    wchar_t code[5];
    GetDlgItemTextW(window, IDC_CODE, code, 5);
    assert(!wcscmp(code, L"----"));
    SendDlgItemMessageW(window, IDC_MODE, CB_SETCURSEL, 0, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_MODE, CBN_SELCHANGE), 0);
    assert(IsWindowEnabled(GetDlgItem(window, IDC_CODE)));
    assert(IsWindowEnabled(GetDlgItem(window, IDC_INVERSE)));
    assert(!IsWindowEnabled(GetDlgItem(window, IDC_LOCATOR)));
    assert(IsDlgButtonChecked(window, IDC_INVERSE) == BST_CHECKED);
    DeleteFileW(path);
    CheckRadioButton(window, IDC_AUTO, IDC_MANUAL, IDC_MANUAL);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_MANUAL, BN_CLICKED), 0);
    assert(!IsWindowEnabled(GetDlgItem(window, IDC_NEW_CODE)));
    SetDlgItemTextW(window, IDC_CODE, L"0001");
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, 0, 0);
    for (int wide=0; wide<2; ++wide) {
        SendDlgItemMessageW(window, IDC_ASPECT, CB_SETCURSEL, wide, 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_ASPECT, CBN_SELCHANGE), 0);
        swprintf(name, MAX_PATH-(name-path), L"%ls-0001-436MHz-120x%d-inverse.jpg", call, wide ? 68 : 90);
        assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
        SendMessageW(window, WM_COMMAND, IDM_EXPORT, 0);
        assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
        DeleteFileW(path);
    }
    CheckRadioButton(window, IDC_AUTO, IDC_MANUAL, IDC_AUTO);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_AUTO, BN_CLICKED), 0);
    GetDlgItemTextW(window, IDC_CODE, next, 5);
    assert(IsWindowEnabled(GetDlgItem(window, IDC_NEW_CODE)));
    assert(valid_code(next) && generated_code_valid((unsigned)wcstoul(next, NULL, 10)));
    DestroyWindow(window);
    pm5544_cleanup();
    IWICImagingFactory_Release(factory);
    CoUninitialize();
    puts("Windows UI: File export and Info dialog, visible automatic code, stable export, mode switching and low-resolution filenames passed.");
    return 0;
}
