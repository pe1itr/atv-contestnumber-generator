#define main atv_application_main
#include "../src/linux.c"
#undef main
#include <assert.h>
static gboolean finish_teletext(gpointer data) {
    GtkWidget *w=data;
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_RGB24,
        gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w));
    cairo_t *cr=cairo_create(surface); gtk_widget_draw(w,cr); cairo_destroy(cr);
    assert(cairo_surface_write_to_png(surface,"build/teletext-linux-editor.png")==CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(surface); gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_ACCEPT);
    return G_SOURCE_REMOVE;
}
static gboolean apply_teletext(gpointer unused) {
    (void)unused;
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(w)),"Teletext pages")) continue;
        TeletextDialog *d=g_object_get_data(G_OBJECT(w),"teletext-editor"); assert(d);
        const char *text="ATV CONTEST";
        for (const char *c=text;*c;++c) {
            GdkEventKey key={0}; key.keyval=(guint)*c; assert(tt_linux_key(d->area,&key,d));
        }
        assert(!tt_linux_store(d));
        GtkWidget *button=gtk_button_new(); g_object_ref_sink(button);
        g_object_set_data(G_OBJECT(button),"action",GINT_TO_POINTER(100)); tt_linux_action(button,d);
        assert(d->selected==1);
        gtk_entry_set_text(GTK_ENTRY(d->header),"IMPORTED PAGE");
        assert(tt_linux_import_path(d,"examples/teletext/atv-station.tti"));
        assert(d->editor.raw[120]==22);
        TeletextEditor loaded=d->editor;
        assert(!tt_linux_import_path(d,"build/nonexistent-page.tti"));
        assert(!memcmp(loaded.raw,d->editor.raw,sizeof(loaded.raw)));
        teletext_editor_undo(&d->editor,&d->undo); assert(d->editor.raw[120]==' ');
        teletext_editor_undo(&d->editor,&d->undo); assert(d->editor.raw[120]==22);
        gtk_combo_box_set_active(GTK_COMBO_BOX(d->color),3); /* Blue, while active text is white. */
        d->editor.cursor=14*40;
        g_object_set_data(G_OBJECT(button),"action",GINT_TO_POINTER(29)); tt_linux_action(button,d);
        assert(d->editor.cursor==14*40+3);
        assert(teletext_editor_cell(&d->editor,14*40+5).background==4);
        assert(teletext_editor_cell(&d->editor,14*40+5).foreground==7);
        assert(d->editor.raw[14*40+5]=='W');
        d->editor.cursor=40;
        gtk_combo_box_set_active(GTK_COMBO_BOX(d->attribute),1);
        g_object_set_data(G_OBJECT(button),"action",GINT_TO_POINTER(105)); tt_linux_action(button,d);
        assert(d->editor.raw[40]==13 && d->editor.cursor==41);
        GdkEventKey big={0}; big.keyval=GDK_KEY_B; assert(tt_linux_key(d->area,&big,d));
        assert(teletext_editor_cell(&d->editor,81).lower);
        GtkWidget *reveal=gtk_check_button_new(); g_object_ref_sink(reveal);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reveal),TRUE);
        g_object_set_data(G_OBJECT(reveal),"action",GINT_TO_POINTER(106)); tt_linux_action(reveal,d);
        assert(d->editor.reveal);
        gtk_widget_destroy(reveal); g_object_unref(reveal);

        d->editor.cursor=140;
        int left,top,pw,ph;
        teletext_viewport(gtk_widget_get_allocated_width(d->area),gtk_widget_get_allocated_height(d->area),&left,&top,&pw,&ph);
        GdkEventButton click={0}; click.button=1; click.x=left+20*pw/40+3;
        click.y=top+ph/24+3*(ph-ph/24)/23+3;
        assert(tt_linux_click(d->area,&click,d));
        assert(!tt_linux_store(d));
        gtk_combo_box_set_active(GTK_COMBO_BOX(d->pages),0);
        assert(d->selected==0 && d->editor.raw[0]=='A' && d->editor.reveal);
        gtk_combo_box_set_active(GTK_COMBO_BOX(d->pages),1);
        assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(d->header)),"IMPORTED PAGE"));
        gtk_widget_destroy(button); g_object_unref(button);
        g_timeout_add(100,finish_teletext,w);
        g_list_free(windows); return G_SOURCE_REMOVE;
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
static gboolean apply_udp(gpointer unused) {
    (void)unused;
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(w)),"DATV: UDP output")) continue;
        GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
        GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
        gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,0)),"192.168.1.50");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,1)),12345);
        GtkGrid *radio=GTK_GRID(gtk_grid_get_child_at(grid,0,5));
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,0)),DVB_T);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,2)),4);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,3)),2);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,5)),1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,3)),12);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,4)),3);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,8)),TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,9)),TRUE);
        gtk_dialog_response(GTK_DIALOG(w),3); g_list_free(windows); return G_SOURCE_REMOVE;
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
static gboolean apply_audio(gpointer data) {
    int bandwidth=GPOINTER_TO_INT(data);
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(w)),"DATV: UDP output")) continue;
        GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
        GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
        GtkGrid *audio=GTK_GRID(gtk_grid_get_child_at(grid,0,11));
        assert(gtk_widget_get_visible(GTK_WIDGET(audio)));
        GtkGrid *radio=GTK_GRID(gtk_grid_get_child_at(grid,0,5));
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,0)),bandwidth?DVB_T:DVB_S);
        if (bandwidth) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,2)),bandwidth==333?4:5);
            gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,3)),2); /* FEC 3/4 */
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,1)),dvb_symbol_rate_row(5));
        }
        /* Audio sensitivity is refreshed by the dialog's periodic callback. */
        gint64 deadline=g_get_monotonic_time()+G_TIME_SPAN_SECOND;
        while (!gtk_widget_get_sensitive(gtk_grid_get_child_at(audio,0,0)) && g_get_monotonic_time()<deadline)
            g_main_context_iteration(NULL,TRUE);
        assert(gtk_widget_get_sensitive(gtk_grid_get_child_at(audio,0,0)));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(audio,0,0)),TRUE);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(audio,1,2)),1);
        gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(audio,1,1)),"testmix.monitor");
        gtk_dialog_response(GTK_DIALOG(w),3); g_list_free(windows); return G_SOURCE_REMOVE;
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
static gboolean apply_motion_udp(gpointer data) {
    int system=GPOINTER_TO_INT(data);
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(w)),"DATV: UDP output")) continue;
        GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
        GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
        GtkGrid *radio=GTK_GRID(gtk_grid_get_child_at(grid,0,5));
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,0)),system);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,1)),dvb_symbol_rate_row(2));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(radio,1,4)),system==DVB_S2);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,4)),5);
        assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,2)))==(system==DVB_S?115196:120665));
        gtk_dialog_response(GTK_DIALOG(w),3); g_list_free(windows); return G_SOURCE_REMOVE;
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
static void close_app(App *app) {
    g_signal_handlers_disconnect_by_func(app->window,gtk_main_quit,NULL);
    gtk_widget_destroy(app->window);
}
int main(int argc,char **argv) {
    gtk_init(&argc,&argv);
    char *directory=g_dir_make_tmp("atv-config-é-XXXXXX",NULL); assert(directory);
    App app={0}; app.directory=directory; create_ui(&app); load_config(&app);
    AppConfig initial=capture_config(&app); assert(initial.genius==1 && !initial.call[0]);
    AppConfig wanted=config_defaults();
    strcpy(wanted.call,"PE1ITR/P"); strcpy(wanted.locator,"JO21QK86DV12"); strcpy(wanted.code,"1957");
    strcpy(wanted.station.city,"Eindhoven"); strcpy(wanted.station.operator_name,"René");
    strcpy(wanted.station.description,"70 cm ATV-station");
    wanted.aspect=1; wanted.resolution=1; wanted.band=10; wanted.genius=2;
    wanted.show=0; wanted.inverse=1; wanted.blue_yellow=1; wanted.show_sum=1; wanted.top_code=1; wanted.ebu_top=1; wanted.ebu_bottom=1;
    wanted.ts=(DatvSettings){.bitrate=60000,.seconds=30,.fps=2,.gop=1,.eit_enabled=1};
    apply_config(&app,&wanted); gtk_widget_show_all(app.window);
    assert(gtk_widget_get_visible(app.udp_menu));
    g_timeout_add(100,apply_udp,NULL); output_udp(NULL,&app);
    assert(app.udp.port==12345 && app.udp.video.fps==12 && app.udp.video.gop==3);
    g_timeout_add(50,apply_teletext,NULL); edit_teletext(NULL,&app);
    assert(app.teletext.enabled && teletext_byte(&app.teletext,0)=='A' && app.teletext.extra_count==1);
    wanted.teletext=app.teletext;
    wanted.udp=app.udp; wanted.udp_dvb=app.udp_dvb;
    assert(wanted.udp_dvb.system==DVB_T && wanted.udp_dvb.bandwidth_khz==333 && wanted.udp_dvb.guard==1);
    save_config(NULL,&app); close_app(&app);
    App reopened={0}; reopened.directory=directory; create_ui(&reopened); load_config(&reopened);
    AppConfig actual=capture_config(&reopened);
    assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(gtk_widget_get_visible(reopened.udp_menu));
    assert(!gtk_editable_get_editable(GTK_EDITABLE(reopened.code)));
    gtk_entry_set_text(GTK_ENTRY(reopened.locator),"JO21QK99AA99");
    assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(reopened.code)),"1957"));
    gtk_entry_set_text(GTK_ENTRY(reopened.locator),"JO22QK99AA99");
    assert(strcmp(gtk_entry_get_text(GTK_ENTRY(reopened.code)),"1957"));
    wanted.mode=IMAGE_FUBK; wanted.automatic=0; wanted.genius=1; wanted.resolution=9;
    apply_config(&reopened,&wanted); save_config(NULL,&reopened); close_app(&reopened);
    App again={0}; again.directory=directory; create_ui(&again); load_config(&again);
    actual=capture_config(&again); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(!gtk_widget_get_visible(again.udp_menu)); assert(!gtk_widget_get_sensitive(again.code));
    close_app(&again);
    wanted.mode=IMAGE_PM5644;
    App pm={0}; pm.directory=directory; create_ui(&pm); apply_config(&pm,&wanted);
    save_config(NULL,&pm); close_app(&pm);
    App pm_reopened={0}; pm_reopened.directory=directory; create_ui(&pm_reopened); load_config(&pm_reopened);
    actual=capture_config(&pm_reopened); assert(!memcmp(&wanted,&actual,sizeof(wanted)));
    assert(pattern_mode(&pm_reopened) && !gtk_widget_get_sensitive(pm_reopened.code));
    char *mode_text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(pm_reopened.mode));
    assert(!strcmp(mode_text,"PM5644")); g_free(mode_text);
    assert(gtk_combo_box_get_active(GTK_COMBO_BOX(pm_reopened.mode))==2);
    close_app(&pm_reopened);
    App audio={0}; audio.directory=directory; create_ui(&audio);
    wanted.genius=3; wanted.udp_dvb=dvb_defaults(); wanted.udp_dvb.symbol_rate=4;
    wanted.udp.video.bitrate=dvb_bitrate(wanted.udp_dvb); wanted.udp.video.audio.bitrate=48000;
    strcpy(wanted.udp.video.audio.source,"ffmix.monitor");
    apply_config(&audio,&wanted);
    assert(gtk_widget_get_visible(audio.udp_menu));
    assert(capture_config(&audio).udp.video.audio.bitrate==48000);
    g_timeout_add(100,apply_audio,NULL); output_udp(NULL,&audio);
    assert(audio.udp.video.audio.bitrate==96000 && audio.udp_dvb.symbol_rate==5);
    assert(!strcmp(audio.udp.video.audio.source,"testmix.monitor"));
    for (int bw=333;bw<=500;bw+=167) {
        g_timeout_add(100,apply_audio,GINT_TO_POINTER(bw)); output_udp(NULL,&audio);
        assert(audio.udp_dvb.system==DVB_T && audio.udp_dvb.bandwidth_khz==bw && audio.udp_dvb.fec==2);
        assert(audio.udp.video.audio.bitrate==96000);
    }
    save_config(NULL,&audio); close_app(&audio);
    App audio_reopened={0}; audio_reopened.directory=directory; create_ui(&audio_reopened); load_config(&audio_reopened);
    actual=capture_config(&audio_reopened);
    assert(actual.genius==3 && actual.udp.video.audio.bitrate==96000);
    assert(actual.udp_dvb.system==DVB_T && actual.udp_dvb.bandwidth_khz==500 && actual.udp_dvb.fec==2);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(audio_reopened.level2),TRUE);
    assert(!audio_reopened.udp.video.audio.bitrate && gtk_widget_get_visible(audio_reopened.udp_menu));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(audio_reopened.level4),TRUE);
    assert(motion_mode(&audio_reopened) && audio_reopened.datv.motion && audio_reopened.udp.video.motion);
    assert(audio_reopened.datv.bitrate==30160 && audio_reopened.datv.fps==10 && audio_reopened.datv.gop==20);
    assert(selected_resolution(&audio_reopened).width==160 && !pattern_mode(&audio_reopened));
    assert(!gtk_widget_get_sensitive(audio_reopened.resolution) && gtk_widget_get_sensitive(audio_reopened.code));
    for (int system=DVB_S;system<=DVB_S2;++system) {
        g_timeout_add(100,apply_motion_udp,GINT_TO_POINTER(system)); output_udp(NULL,&audio_reopened);
        assert(audio_reopened.udp_dvb.system==system && audio_reopened.udp_dvb.symbol_rate==2 && audio_reopened.udp_dvb.fec==0);
        assert(audio_reopened.udp.video.motion && audio_reopened.udp.video.fps==10 && audio_reopened.udp.video.gop==5);
        assert(audio_reopened.udp.video.bitrate==(system==DVB_S?115196:120665));
    }
    audio_reopened.datv.gop=10; save_config(NULL,&audio_reopened);
    close_app(&audio_reopened);
    App motion={0}; motion.directory=directory; create_ui(&motion); load_config(&motion);
    assert(motion_mode(&motion) && motion.datv.motion && motion.datv.gop==10 && motion.udp.video.motion);
    assert(motion.udp_dvb.system==DVB_S2 && motion.udp_dvb.symbol_rate==2 && motion.udp_dvb.pilots);
    assert(motion.udp.video.bitrate==120665 && motion.udp.video.gop==5);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(motion.level1),TRUE);
    assert(!motion.datv.motion && !gtk_widget_get_visible(motion.ts_menu));
    assert(gtk_widget_get_sensitive(motion.resolution)); close_app(&motion);
    App activity={0}; activity.directory=directory; create_ui(&activity);
    wanted=config_defaults(); wanted.mode=IMAGE_ACTIVITY; wanted.genius=2;
    strcpy(wanted.call,"LX/PE1ITR/P"); strcpy(wanted.locator,"JO21QK"); wanted.code[0]=0;
    wanted.activity_ebu=1; wanted.activity_motion=1; wanted.ebu_top=wanted.ebu_bottom=1;
    apply_config(&activity,&wanted);
    assert(!gtk_widget_get_sensitive(activity.code) && gtk_widget_get_sensitive(activity.band));
    assert(gtk_widget_get_sensitive(activity.locator) && !gtk_widget_get_sensitive(activity.ebu_top));
    assert(activity.datv.activity_motion && activity.udp.video.activity_motion);
    cairo_surface_t *card=current_image(&activity);
    assert(cairo_surface_status(card)==CAIRO_STATUS_SUCCESS);
    assert(cairo_surface_write_to_png(card,"build/activity-linux-ui.png")==CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(card);
    save_config(NULL,&activity); close_app(&activity);
    App activity_again={0}; activity_again.directory=directory; create_ui(&activity_again); load_config(&activity_again);
    actual=capture_config(&activity_again);
    assert(actual.mode==IMAGE_ACTIVITY && actual.activity_motion && actual.activity_ebu && !actual.code[0]);
    assert(actual.ts.activity_motion && actual.udp.video.activity_motion);
    gtk_combo_box_set_active(GTK_COMBO_BOX(activity_again.mode),image_mode_row(IMAGE_CONTEST));
    assert(!activity_again.datv.activity_motion && !activity_again.udp.video.activity_motion);
    assert(!gtk_widget_get_sensitive(activity_again.activity_motion));
    close_app(&activity_again);
    char *path=g_build_filename(directory,CONFIG_FILENAME,NULL); assert(!remove(path)); g_free(path);
    assert(!rmdir(directory)); g_free(directory); pm5544_cleanup();
    puts("Linux config UI: save/reopen, all fields, UDP apply without Start, Genius visibility and automatic code preservation OK.");
    return 0;
}
