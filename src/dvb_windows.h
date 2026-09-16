/* The same core calculator and choices as the GTK controls. */
static DvbSettings dvb_window_read(HWND window) {
    DvbSettings s=dvb_defaults();
    s.system=(int)SendDlgItemMessageW(window,IDC_DVB_SYSTEM,CB_GETCURSEL,0,0);
    s.symbol_rate=(int)SendDlgItemMessageW(window,IDC_DVB_SR,CB_GETCURSEL,0,0);
    s.fec=(int)SendDlgItemMessageW(window,IDC_DVB_FEC,CB_GETCURSEL,0,0);
    s.guard=(int)SendDlgItemMessageW(window,IDC_DVB_GUARD,CB_GETCURSEL,0,0);
    s.pilots=IsDlgButtonChecked(window,IDC_DVB_PILOTS)==BST_CHECKED;
    int bw=(int)SendDlgItemMessageW(window,IDC_DVB_BW,CB_GETCURSEL,0,0);
    s.bandwidth_khz=bw>=0 && bw<DVB_BANDWIDTH_COUNT?dvb_bandwidths[bw]:0;
    return s;
}
static void dvb_window_enable(HWND window,BOOL enabled) {
    DvbSettings s=dvb_window_read(window);
    EnableWindow(GetDlgItem(window,IDC_DVB_SYSTEM),enabled);
    EnableWindow(GetDlgItem(window,IDC_DVB_FEC),enabled);
    EnableWindow(GetDlgItem(window,IDC_DVB_SR),enabled && s.system!=DVB_T);
    EnableWindow(GetDlgItem(window,IDC_DVB_BW),enabled && s.system==DVB_T);
    EnableWindow(GetDlgItem(window,IDC_DVB_GUARD),enabled && s.system==DVB_T);
    EnableWindow(GetDlgItem(window,IDC_DVB_PILOTS),enabled && s.system==DVB_S2);
}
static void dvb_window_refresh(HWND window,int bitrate) {
    SetDlgItemInt(window,bitrate,dvb_bitrate(dvb_window_read(window)),FALSE);
    dvb_window_enable(window,TRUE);
}
static BOOL dvb_window_event(HWND window,WPARAM wp,int bitrate) {
    int id=LOWORD(wp), event=HIWORD(wp);
    if (id<IDC_DVB_SYSTEM || id>IDC_DVB_GUARD ||
        (event!=CBN_SELCHANGE && !(id==IDC_DVB_PILOTS && event==BN_CLICKED))) return FALSE;
    if (IsWindowEnabled(GetDlgItem(window,id))) dvb_window_refresh(window,bitrate);
    return TRUE;
}
static void dvb_window_init(HWND window,int bitrate,DvbSettings s) {
    const int ids[]={IDC_DVB_SYSTEM,IDC_DVB_FEC,IDC_DVB_GUARD};
    const int counts[]={DVB_SYSTEM_COUNT,DVB_FEC_COUNT,DVB_GUARD_COUNT};
    const wchar_t *const *names[]={dvb_system_names,dvb_fec_names,dvb_guard_names};
    for (int n=0;n<3;++n) for (int i=0;i<counts[n];++i)
        SendDlgItemMessageW(window,ids[n],CB_ADDSTRING,0,(LPARAM)names[n][i]);
    for (int i=0;i<DVB_SYMBOL_RATE_COUNT;++i) {
        wchar_t text[24]; swprintf(text,24,L"%d ksym/s",dvb_symbol_rates[i]);
        SendDlgItemMessageW(window,IDC_DVB_SR,CB_ADDSTRING,0,(LPARAM)text);
    }
    int bw=0;
    for (int i=0;i<DVB_BANDWIDTH_COUNT;++i) {
        wchar_t text[24]; swprintf(text,24,L"%dk",dvb_bandwidths[i]);
        SendDlgItemMessageW(window,IDC_DVB_BW,CB_ADDSTRING,0,(LPARAM)text);
        if (s.bandwidth_khz==dvb_bandwidths[i]) bw=i;
    }
    SendDlgItemMessageW(window,IDC_DVB_SYSTEM,CB_SETCURSEL,s.system,0);
    SendDlgItemMessageW(window,IDC_DVB_SR,CB_SETCURSEL,s.symbol_rate,0);
    SendDlgItemMessageW(window,IDC_DVB_BW,CB_SETCURSEL,bw,0);
    SendDlgItemMessageW(window,IDC_DVB_FEC,CB_SETCURSEL,s.fec,0);
    SendDlgItemMessageW(window,IDC_DVB_GUARD,CB_SETCURSEL,s.guard,0);
    CheckDlgButton(window,IDC_DVB_PILOTS,s.pilots?BST_CHECKED:BST_UNCHECKED);
    SendDlgItemMessageW(window,bitrate,EM_SETREADONLY,TRUE,0);
    dvb_window_refresh(window,bitrate);
}
