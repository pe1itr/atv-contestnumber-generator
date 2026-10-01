#define main atv_application_main
#include "../src/linux.c"
#undef main
#include <assert.h>

static int stage=-3;
static char *directory;
static GSocket *receiver;
static int port, received;
static gint64 udp_deadline;
static void check_dvb(GtkGrid *grid,int row) {
    GtkWidget *field=gtk_grid_get_child_at(grid,1,row);
    GtkGrid *radio=GTK_GRID(gtk_grid_get_child_at(grid,0,row==0?4:5));
    GtkWidget *system=gtk_grid_get_child_at(radio,1,0);
    GtkWidget *sr=gtk_grid_get_child_at(radio,1,1), *bw=gtk_grid_get_child_at(radio,1,2);
    GtkWidget *fec=gtk_grid_get_child_at(radio,1,3), *pilots=gtk_grid_get_child_at(radio,1,4);
    GtkWidget *guard=gtk_grid_get_child_at(radio,1,5);
    assert(!gtk_editable_get_editable(GTK_EDITABLE(field)) && !gtk_widget_get_sensitive(field));
    const int expected[]={25,30,33,35,66,125,150,250,333,500};
    assert(gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(sr)),NULL)==10);
    for (int i=0;i<10;++i) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(sr),i);
        char wanted[24]; snprintf(wanted,sizeof(wanted),"%d ksym/s",expected[i]);
        char *text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(sr));
        assert(!strcmp(text,wanted)); g_free(text);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(sr),5);
    const int rates[]={30718,36862,40549};
    gtk_combo_box_set_active(GTK_COMBO_BOX(system),DVB_S);
    gtk_combo_box_set_active(GTK_COMBO_BOX(fec),0);
    for (int i=0;i<3;++i) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(sr),i);
        assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==rates[i]);
        GtkTreeModel *model=gtk_combo_box_get_model(GTK_COMBO_BOX(fec));
        assert(gtk_tree_model_iter_n_children(model,NULL)==(i<2?4:5));
        char *text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(fec));
        assert(!strcmp(text,"2/3")); g_free(text);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(sr),5);
    gtk_combo_box_set_active(GTK_COMBO_BOX(fec),0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(system),DVB_S);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==115196);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sr),7);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==230392);
    gtk_combo_box_set_active(GTK_COMBO_BOX(system),DVB_S2);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==247214);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sr),5);
    assert(gtk_widget_get_sensitive(pilots) && !gtk_widget_get_sensitive(guard));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pilots),TRUE);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==120665);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pilots),FALSE);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==123607);
    gtk_combo_box_set_active(GTK_COMBO_BOX(system),DVB_T);
    gtk_combo_box_set_active(GTK_COMBO_BOX(bw),1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(guard),0);
    assert(!gtk_widget_get_sensitive(sr) && !gtk_widget_get_sensitive(pilots));
    assert(gtk_widget_get_sensitive(bw) && gtk_widget_get_sensitive(guard));
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==103676);
    gtk_combo_box_set_active(GTK_COMBO_BOX(bw),0);
    char *bw_text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(bw));
    assert(!strcmp(bw_text,"35k")); g_free(bw_text);
    assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(field))==32254);
    assert(gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(fec)),NULL)==4);
    gtk_combo_box_set_active(GTK_COMBO_BOX(bw),1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(fec),0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(system),row==0?DVB_S2:DVB_S);
}
static void receive_udp(void) {
    char bytes[2048]; GError *error=NULL; gssize n;
    while ((n=g_socket_receive(receiver,bytes,sizeof(bytes),NULL,&error))>=0) { assert(n==1316); ++received; }
    assert(g_error_matches(error,G_IO_ERROR,G_IO_ERROR_WOULD_BLOCK)); g_clear_error(&error);
}
static gboolean accept_picker(gpointer data) {
    gtk_dialog_response(GTK_DIALOG(data),GTK_RESPONSE_ACCEPT); return G_SOURCE_REMOVE;
}
static gboolean drive_dialogs(gpointer unused) {
    (void)unused;
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (!gtk_widget_get_visible(w)) continue;
        const char *title=gtk_window_get_title(GTK_WINDOW(w));
        if (stage<0 && title && !strcmp(title,"EIT-programma-informatie")) {
            GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
            GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
            assert(!gtk_editable_get_editable(GTK_EDITABLE(gtk_grid_get_child_at(grid,1,0))));
            assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,1))),"JO21QK"));
            gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,2)),stage==-3?"Eindhoven":"Annuleren");
            gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,3)),"René");
            gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,4)),"70 cm ATV-station");
            gtk_dialog_response(GTK_DIALOG(w),stage==-3?GTK_RESPONSE_ACCEPT:GTK_RESPONSE_CANCEL);
            ++stage;
        }
        if (stage>=10 && title && !strcmp(title,"DATV: UDP-uitvoer")) {
            assert(g_get_monotonic_time()<udp_deadline);
            GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
            GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
            GtkWidget *ip=gtk_grid_get_child_at(grid,1,0), *port_field=gtk_grid_get_child_at(grid,1,1);
            const char *status=gtk_label_get_text(GTK_LABEL(gtk_grid_get_child_at(grid,0,7)));
            if (stage==10) {
                check_dvb(grid,2);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,8)),TRUE);
                assert(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(port_field))==10000);
                gtk_entry_set_text(GTK_ENTRY(ip),"999.1.2.3"); stage=11; gtk_dialog_response(GTK_DIALOG(w),1);
            } else if (stage==11) {
                assert(strstr(status,"geldig"));
                gtk_entry_set_text(GTK_ENTRY(ip),"127.0.0.1");
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_field),port);
                stage=12; gtk_dialog_response(GTK_DIALOG(w),1);
            } else if (stage==12) {
                receive_udp();
                if (received>=4) {
                    assert(strstr(status,"UDP-uitvoer actief")); assert(!gtk_widget_get_sensitive(ip));
                    assert(!gtk_widget_get_sensitive(gtk_grid_get_child_at(grid,0,5)));
                    assert(!gtk_widget_get_sensitive(gtk_grid_get_child_at(grid,0,8)));
                    stage=13; gtk_dialog_response(GTK_DIALOG(w),2);
                }
            } else if (stage==13 && strstr(status,"UDP gestopt")) {
                assert(gtk_widget_get_sensitive(ip)); stage=14; gtk_dialog_response(GTK_DIALOG(w),1);
            } else if (stage==14) {
                receive_udp();
                if (received>=8) { stage=15; gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_CLOSE); }
            }
        }
        if (stage==0 && title && !strcmp(title,"DATV: TS-proefbestand")) {
            GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
            GtkGrid *grid=GTK_GRID(children->data);
            int values[]={60000,3,2,1};
            for (int i=1;i<4;++i) gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,i)),values[i]);
            check_dvb(grid,0);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,6)),TRUE);
            g_list_free(children); stage=1; gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_ACCEPT);
        } else if (stage==1 && GTK_IS_FILE_CHOOSER(w)) {
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w),directory);
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(w),"kaart-é");
            stage=2; g_timeout_add(300,accept_picker,w);
        }
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv) {
    assert(gtk_init_check(&argc,&argv));
    directory=g_dir_make_tmp("atv-ts-ui-XXXXXX",NULL); assert(directory);
    App app={0}; app.directory=directory; create_ui(&app); gtk_widget_show_all(app.window);
    assert(!gtk_widget_get_visible(app.ts_menu));
    assert(!gtk_widget_get_visible(app.udp_menu));
    GtkWidget *layout=gtk_bin_get_child(GTK_BIN(app.window));
    GList *children=gtk_container_get_children(GTK_CONTAINER(layout));
    GList *menus=gtk_container_get_children(GTK_CONTAINER(children->data));
    GtkWidget *config=gtk_menu_item_get_submenu(GTK_MENU_ITEM(g_list_nth_data(menus,1)));
    GList *levels=gtk_container_get_children(GTK_CONTAINER(config));
    GtkCheckMenuItem *level2=GTK_CHECK_MENU_ITEM(g_list_nth_data(levels,1));
    gtk_check_menu_item_set_active(level2,TRUE); assert(gtk_widget_get_visible(app.ts_menu));
    assert(gtk_widget_get_visible(app.udp_menu));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(levels->data),TRUE); assert(!gtk_widget_get_visible(app.ts_menu));
    gtk_check_menu_item_set_active(level2,TRUE);
    g_list_free(levels); g_list_free(menus); g_list_free(children);
    gtk_entry_set_text(GTK_ENTRY(app.call),"PE1ITR"); gtk_entry_set_text(GTK_ENTRY(app.locator),"JO21QK");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.manual),TRUE);
    gtk_entry_set_text(GTK_ENTRY(app.code),"1957");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.blue_yellow),TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.resolution),1);
    guint timer=g_timeout_add(50,drive_dialogs,NULL);
    edit_eit(NULL,&app); edit_eit(NULL,&app);
    assert(!strcmp(app.station.city,"Eindhoven") && !strcmp(app.station.operator_name,"René"));
    stage=0;
    g_signal_emit_by_name(app.ts_menu,"activate");
    gint64 deadline=g_get_monotonic_time()+15000000;
    while (!strstr(gtk_label_get_text(GTK_LABEL(app.status)),"TS opgeslagen:")) {
        assert(g_get_monotonic_time()<deadline);
        g_main_context_iteration(NULL,FALSE); g_usleep(1000);
    }
    g_source_remove(timer);
    assert(app.datv.eit_enabled);
    assert(stage==2 && app.datv.bitrate==123607 && app.datv.gop==1);
    char *path=g_build_filename(directory,"kaart-é.ts",NULL);
    assert(g_file_test(path,G_FILE_TEST_IS_REGULAR));
    assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(app.code)),"1957"));
    assert(strstr(app_codec_license(),"Copyright (c) 2013, Cisco Systems"));
    g_print("Linux UI: levels, TS-instellingen, export, Unicode-pad, .ts-extensie en ongewijzigd nummer OK: %s\n",path);
    unlink(path); rmdir(directory); g_free(path);
    receiver=g_socket_new(G_SOCKET_FAMILY_IPV4,G_SOCKET_TYPE_DATAGRAM,G_SOCKET_PROTOCOL_UDP,NULL); assert(receiver);
    GSocketAddress *address=g_inet_socket_address_new_from_string("127.0.0.1",0);
    assert(g_socket_bind(receiver,address,FALSE,NULL)); g_object_unref(address);
    address=g_socket_get_local_address(receiver,NULL); port=g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)); g_object_unref(address);
    g_socket_set_blocking(receiver,FALSE);
    stage=10; udp_deadline=g_get_monotonic_time()+20000000;
    timer=g_timeout_add(50,drive_dialogs,NULL);
    g_signal_emit_by_name(app.udp_menu,"activate"); g_source_remove(timer);
    assert(stage==15 && received>=8 && app.udp.port==port);
    assert(app.udp.video.bitrate==115196 && app.udp.video.eit_enabled);
    g_object_unref(receiver);
    g_print("Linux UDP UI: adresvalidatie, Start/Stop, opnieuw starten en sluiten tijdens uitzending OK.\n");
    g_signal_handlers_disconnect_by_func(app.window,gtk_main_quit,NULL);
    gtk_widget_destroy(app.window); g_free(directory); pm5544_cleanup();
    return 0;
}
