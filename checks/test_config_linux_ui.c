#define main atv_application_main
#include "../src/linux.c"
#undef main
#include <assert.h>
static gboolean apply_udp(gpointer unused) {
    (void)unused;
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(w)),"DATV: UDP-uitvoer")) continue;
        GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
        GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
        gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,0)),"192.168.1.50");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,1)),12345);
        GtkGrid *radio=GTK_GRID(gtk_grid_get_child_at(grid,0,5));
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,0)),DVB_T);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,2)),2);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,3)),2);
        gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_grid_get_child_at(radio,1,5)),1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,3)),12);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,4)),3);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,8)),TRUE);
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
    char *path=g_build_filename(directory,CONFIG_FILENAME,NULL); assert(!remove(path)); g_free(path);
    assert(!rmdir(directory)); g_free(directory); pm5544_cleanup();
    puts("Linux config UI: save/reopen, all fields, UDP apply without Start, Genius visibility and automatic code preservation OK.");
    return 0;
}
