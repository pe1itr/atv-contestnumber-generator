#define main atv_application_main
#include "../src/linux.c"
#undef main
#include <assert.h>

static gboolean close_about(gpointer data) {
    (void)data;
    GList *windows = gtk_window_list_toplevels();
    gboolean found = FALSE;
    for (GList *item=windows; item; item=item->next) if (GTK_IS_MESSAGE_DIALOG(item->data)) {
        char *text = NULL;
        g_object_get(item->data, "text", &text, NULL);
        assert(text && !strcmp(text, app_info_text()));
        assert(strstr(text, APP_VERSION) && strstr(text, APP_AUTHOR));
        g_free(text);
        gtk_dialog_response(GTK_DIALOG(item->data), GTK_RESPONSE_CLOSE);
        found = TRUE;
    }
    g_list_free(windows);
    assert(found);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) return 2;
    App app = {0};
    app.directory = g_dir_make_tmp("atv-pm-ui-XXXXXX", NULL);
    assert(app.directory);
    create_ui(&app);
    g_idle_add(close_about, NULL);
    gtk_menu_item_activate(GTK_MENU_ITEM(app.about_menu));
    gtk_entry_set_text(GTK_ENTRY(app.call), "PE1ITR");
    gtk_entry_set_text(GTK_ENTRY(app.locator), "JO21QK");
    assert(validate(gtk_entry_get_text(GTK_ENTRY(app.code)), valid_code));
    assert(generated_code_valid((unsigned)atoi(gtk_entry_get_text(GTK_ENTRY(app.code)))));
    char *initial = g_strdup(gtk_entry_get_text(GTK_ENTRY(app.code)));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.resolution), 2);
    gtk_menu_item_activate(GTK_MENU_ITEM(app.export_menu));
    assert(!strcmp(initial, gtk_entry_get_text(GTK_ENTRY(app.code))));
    char *auto_path = g_strdup_printf("%s/PE1ITR-%s-436MHz-320x240.jpg", app.directory, initial);
    assert(g_file_test(auto_path, G_FILE_TEST_IS_REGULAR));
    unlink(auto_path); g_free(auto_path);
    gtk_button_clicked(GTK_BUTTON(app.new_code));
    assert(strcmp(initial, gtk_entry_get_text(GTK_ENTRY(app.code))));
    assert(validate(gtk_entry_get_text(GTK_ENTRY(app.code)), valid_code));
    g_free(initial);
    gtk_entry_set_text(GTK_ENTRY(app.code), "----");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.show), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.inverse), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.resolution), 2);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.mode), 1);
    assert(!gtk_widget_get_sensitive(app.code));
    assert(!gtk_widget_get_sensitive(app.automatic));
    assert(!gtk_widget_get_sensitive(app.new_code));
    assert(!gtk_widget_get_sensitive(app.manual));
    assert(!gtk_widget_get_sensitive(app.inverse));
    assert(!gtk_widget_get_sensitive(app.band));
    assert(!gtk_widget_get_sensitive(app.show));
    assert(!gtk_widget_get_sensitive(app.show_sum));
    assert(!gtk_widget_get_sensitive(app.top_code));
    assert(gtk_widget_get_sensitive(app.locator));
    gtk_menu_item_activate(GTK_MENU_ITEM(app.export_menu)); /* Invalid contest code and inverse must be ignored. */
    char *path = g_build_filename(app.directory, "PE1ITR-PM5544-320x240.jpg", NULL);
    assert(g_file_test(path, G_FILE_TEST_IS_REGULAR));
    assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(app.code)), "----"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.mode), 0);
    assert(gtk_widget_get_sensitive(app.code));
    assert(gtk_widget_get_sensitive(app.inverse));
    assert(!gtk_widget_get_sensitive(app.locator));
    assert(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app.inverse)));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.manual), TRUE);
    assert(!gtk_widget_get_sensitive(app.new_code));
    gtk_entry_set_text(GTK_ENTRY(app.code), "0001");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.resolution), 0);
    for (int wide=0; wide<2; ++wide) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.aspect), wide);
        gtk_menu_item_activate(GTK_MENU_ITEM(app.export_menu));
        char *contest = g_build_filename(app.directory, wide ? "PE1ITR-0001-436MHz-120x68-inverse.jpg" : "PE1ITR-0001-436MHz-120x90-inverse.jpg", NULL);
        assert(g_file_test(contest, G_FILE_TEST_IS_REGULAR));
        unlink(contest); g_free(contest);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.automatic), TRUE);
    assert(gtk_widget_get_sensitive(app.new_code));
    assert(generated_code_valid((unsigned)atoi(gtk_entry_get_text(GTK_ENTRY(app.code)))));
    g_signal_handlers_disconnect_by_func(app.window, G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_destroy(app.window);
    unlink(path); rmdir(app.directory);
    g_free(path); g_free(app.directory);
    pm5544_cleanup();
    puts("Linux UI: File export and Info dialog, visible automatic code, stable export, mode switching and low-resolution filenames passed.");
    return 0;
}
