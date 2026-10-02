/* Shared DVB controls for file export and UDP, using the core calculator. */
typedef struct {
    GtkWidget *box, *system, *sr, *bw, *fec, *pilots, *guard, *bitrate;
    int fec_ids[DVB_FEC_COUNT], updating;
} DvbControls;
static DvbSettings dvb_controls_read(DvbControls *d) {
    DvbSettings s=dvb_defaults();
    s.system=gtk_combo_box_get_active(GTK_COMBO_BOX(d->system));
    int sr_row=gtk_combo_box_get_active(GTK_COMBO_BOX(d->sr));
    s.symbol_rate=sr_row>=0 && sr_row<DVB_SYMBOL_RATE_CHOICE_COUNT?dvb_symbol_rate_order[sr_row]:-1;
    int fec_row=gtk_combo_box_get_active(GTK_COMBO_BOX(d->fec));
    s.fec=fec_row>=0 && fec_row<DVB_FEC_COUNT?d->fec_ids[fec_row]:-1;
    s.guard=gtk_combo_box_get_active(GTK_COMBO_BOX(d->guard));
    s.pilots=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->pilots));
    int bw=gtk_combo_box_get_active(GTK_COMBO_BOX(d->bw));
    s.bandwidth_khz=bw>=0 && bw<DVB_BANDWIDTH_COUNT?dvb_bandwidths[bw]:0;
    return s;
}
static void dvb_controls_changed(GtkWidget *widget,gpointer data) {
    (void)widget; DvbControls *d=data;
    if (d->updating) return;
    DvbSettings s=dvb_controls_read(d);
    d->updating=1;
    int count=dvb_fec_choices(s,d->fec_ids), selected=0;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(d->fec));
    for (int i=0;i<count;++i) {
        char *text=utf8(dvb_fec_names[d->fec_ids[i]]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d->fec),text); g_free(text);
        if (s.fec==d->fec_ids[i]) selected=i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->fec),count?selected:-1);
    s.fec=count?d->fec_ids[selected]:-1;
    d->updating=0;
    gtk_widget_set_sensitive(d->sr,s.system!=DVB_T);
    gtk_widget_set_sensitive(d->bw,s.system==DVB_T);
    gtk_widget_set_sensitive(d->guard,s.system==DVB_T);
    gtk_widget_set_sensitive(d->pilots,s.system==DVB_S2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->bitrate),dvb_bitrate(s));
}
static void dvb_controls_init(DvbControls *d,GtkWidget *bitrate,DvbSettings s) {
    for (int i=0;i<DVB_FEC_COUNT;++i) d->fec_ids[i]=i;
    d->updating=0;
    d->bitrate=bitrate; d->box=gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(d->box),6);
    gtk_grid_set_column_spacing(GTK_GRID(d->box),12);
    d->system=gtk_combo_box_text_new(); d->sr=gtk_combo_box_text_new();
    d->bw=gtk_combo_box_text_new(); d->fec=gtk_combo_box_text_new();
    d->guard=gtk_combo_box_text_new(); d->pilots=gtk_check_button_new_with_label("On");
    const wchar_t *const *names[]={dvb_system_names,dvb_fec_names,dvb_guard_names};
    GtkWidget *combos[]={d->system,d->fec,d->guard};
    const int counts[]={DVB_SYSTEM_COUNT,DVB_FEC_COUNT,DVB_GUARD_COUNT};
    for (int n=0;n<3;++n) for (int i=0;i<counts[n];++i) {
        char *text=utf8(names[n][i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combos[n]),text); g_free(text);
    }
    for (int i=0;i<DVB_SYMBOL_RATE_CHOICE_COUNT;++i) {
        char text[24]; g_snprintf(text,sizeof(text),"%d ksym/s",dvb_symbol_rates[dvb_symbol_rate_order[i]]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d->sr),text);
    }
    int bw=0;
    for (int i=0;i<DVB_BANDWIDTH_COUNT;++i) {
        char text[24]; g_snprintf(text,sizeof(text),"%dk",dvb_bandwidths[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d->bw),text);
        if (s.bandwidth_khz==dvb_bandwidths[i]) bw=i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->system),s.system);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->sr),dvb_symbol_rate_row(s.symbol_rate));
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->bw),bw);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->fec),s.fec);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->guard),s.guard);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->pilots),s.pilots);
    GtkWidget *fields[]={d->system,d->sr,d->bw,d->fec,d->pilots,d->guard};
    const char *labels[]={"System (QPSK)","Symbol rate (DVB-S/S2)","SR/BW (Portsdown, DVB-T)","FEC","Pilots (DVB-S2)","Guard interval (DVB-T, 2K)"};
    for (int i=0;i<6;++i) {
        GtkWidget *label=gtk_label_new(labels[i]); gtk_label_set_xalign(GTK_LABEL(label),0);
        gtk_grid_attach(GTK_GRID(d->box),label,0,i,1,1);
        gtk_grid_attach(GTK_GRID(d->box),fields[i],1,i,1,1);
        g_signal_connect(fields[i],i==4?"toggled":"changed",G_CALLBACK(dvb_controls_changed),d);
    }
    gtk_grid_attach(GTK_GRID(d->box),gtk_label_new("DVB-S2: normal frames. DVB-T: SR/BW is bandwidth in kHz."),0,6,2,1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(bitrate),0,2000000);
    gtk_editable_set_editable(GTK_EDITABLE(bitrate),FALSE);
    gtk_widget_set_sensitive(bitrate,FALSE);
    dvb_controls_changed(NULL,d);
}
