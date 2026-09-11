#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <sys/random.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include "core.h"
#include "app_info.h"
#include "pm5544.h"

typedef struct {
    GtkWidget *window, *call, *locator, *show, *automatic, *code;
    GtkWidget *aspect, *resolution, *band, *preview, *status, *inverse, *blue_yellow, *show_sum, *top_code, *mode, *manual, *new_code;
    GtkWidget *export_menu, *export_as_menu, *about_menu, *quit_menu;
    char *directory;
    wchar_t contest_square[7];
} App;

static char *utf8(const wchar_t *s) {
    GString *result = g_string_new(NULL);
    while (*s) g_string_append_unichar(result, (gunichar)*s++);
    return g_string_free(result, FALSE);
}

static char *entry_text(GtkWidget *entry) {
    return g_utf8_strup(gtk_entry_get_text(GTK_ENTRY(entry)), -1);
}

static gboolean validate(const char *s, int (*validator)(const wchar_t *)) {
    wchar_t buffer[64];
    size_t n = 0;
    while (*s && n < G_N_ELEMENTS(buffer)-1) {
        buffer[n++] = (wchar_t)g_utf8_get_char(s);
        s = g_utf8_next_char(s);
    }
    buffer[n] = 0;
    return !*s && validator(buffer);
}

static void read_locator(App *app, wchar_t out[LOCATOR_MAX_LENGTH+1]) {
    char *text = entry_text(app->locator);
    const char *p = text;
    int n = 0;
    while (*p && n < LOCATOR_MAX_LENGTH) {
        out[n++] = (wchar_t)g_utf8_get_char(p);
        p = g_utf8_next_char(p);
    }
    out[n] = 0;
    g_free(text);
}

static gboolean random_code(char code[5]) {
    uint32_t value;
    for (;;) {
        ssize_t count;
        do { count = getrandom(&value, sizeof(value), 0); } while (count < 0 && errno == EINTR);
        if (count != sizeof(value)) return FALSE;
        if (value >= UINT32_C(4294962000)) continue;
        value = 1000 + value % 9000;
        if (generated_code_valid(value)) {
            g_snprintf(code, 5, "%04u", value);
            return TRUE;
        }
    }
}

static void draw_line(cairo_t *cr, const char *text, int left, int top,
                      int right, int bottom, int size, PangoAlignment alignment) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Arial Bold");
    pango_layout_set_text(layout, text, -1);
    int width, height;
    do {
        pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
        pango_layout_set_font_description(layout, font);
        pango_layout_get_pixel_size(layout, &width, &height);
    } while ((width > right-left || height > bottom-top) && --size > 3);
    cairo_move_to(cr, alignment == PANGO_ALIGN_RIGHT ? right-width :
                  alignment == PANGO_ALIGN_LEFT ? left : left+(right-left-width)/2,
                  top+(bottom-top-height)/2);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}

static cairo_surface_t *render(Resolution r, const char *call, const char *code,
                               const char *locator, gboolean show, const char *band, gboolean inverse, gboolean show_sum, gboolean top_code, gboolean blue_yellow) {
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, r.width, r.height);
    cairo_t *cr = cairo_create(surface);
    ContestPalette palette = contest_palette(blue_yellow, inverse);
    cairo_set_source_rgb(cr, palette.background.red/255.0, palette.background.green/255.0, palette.background.blue/255.0);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, palette.foreground.red/255.0, palette.foreground.green/255.0, palette.foreground.blue/255.0);
    cairo_font_options_t *options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options(cr, options);
    cairo_font_options_destroy(options);
    int w = r.width, h = r.height, m = w/40;
    int small_size = MAX(7, h*6/100);
    draw_line(cr, call, m, top_code ? h*9/100 : h/50, w-m, h*22/100, h*17/100, PANGO_ALIGN_CENTER);
    draw_line(cr, code, m, h*22/100, w-m, h*(show ? 80 : 91)/100, h*70/100, PANGO_ALIGN_CENTER);
    if (show) draw_line(cr, locator, m, h*80/100, w-m, h*91/100, h*14/100, PANGO_ALIGN_CENTER);
    draw_line(cr, band, w/2, h*91/100, w-m, h*99/100, small_size, PANGO_ALIGN_RIGHT);
    wchar_t digits[5] = {0};
    if (strlen(code) == 4) for (int i=0; i<4; ++i) digits[i] = (unsigned char)code[i];
    int sum = code_digit_sum(digits);
    if (top_code && sum >= 0)
        draw_line(cr, code, w/2, h/100, w-m, h*9/100, small_size, PANGO_ALIGN_RIGHT);
    if (show_sum && sum >= 0) {
        char label[32];
        g_snprintf(label, sizeof(label), "de som is %d", sum);
        draw_line(cr, label, m, h*91/100, w/2, h*99/100, small_size, PANGO_ALIGN_LEFT);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    return surface;
}

static cairo_surface_t *render_pm(Resolution r, const char *call, const char *locator) {
    wchar_t wc[25] = {0}, wl[LOCATOR_MAX_LENGTH+1] = {0};
    const char *p = call;
    for (int i=0; i<24 && *p; ++i, p=g_utf8_next_char(p)) wc[i] = (wchar_t)g_utf8_get_char(p);
    p = locator;
    for (int i=0; i<LOCATOR_MAX_LENGTH && *p; ++i, p=g_utf8_next_char(p)) wl[i] = (wchar_t)g_utf8_get_char(p);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, r.width, r.height);
    if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
        cairo_surface_flush(surface);
        if (!pm5544_render((uint32_t *)cairo_image_surface_get_data(surface), r.width, r.height, wc, wl)) {
            cairo_surface_destroy(surface);
            return cairo_image_surface_create(CAIRO_FORMAT_RGB24, -1, -1);
        }
        cairo_surface_mark_dirty(surface);
    }
    return surface;
}
static gboolean pm_mode(App *app) {
    return gtk_combo_box_get_active(GTK_COMBO_BOX(app->mode)) == 1;
}

static gboolean save_jpeg(cairo_surface_t *surface, const char *path,
                           gboolean overwrite, GError **error) {
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "Kan het beeld niet maken.");
        return FALSE;
    }
    GdkPixbuf *pixels = gdk_pixbuf_get_from_surface(surface, 0, 0,
        cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
    if (!pixels) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "Onvoldoende geheugen voor het beeld.");
        return FALSE;
    }
    char *data = NULL;
    gsize length = 0;
    gboolean ok = gdk_pixbuf_save_to_buffer(pixels, &data, &length, "jpeg", error, "quality", "98", NULL);
    g_object_unref(pixels);
    if (!ok) return FALSE;
    char *temporary = g_strconcat(path, ".XXXXXX", NULL);
    int fd = g_mkstemp(temporary);
    if (fd < 0) goto failure;
    gsize done = 0;
    while (done < length) {
        ssize_t n = write(fd, data+done, length-done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { int saved = errno; close(fd); errno = saved; goto failure; }
        done += (gsize)n;
    }
    if (fsync(fd) != 0) { int saved = errno; close(fd); errno = saved; goto failure; }
    if (close(fd) != 0) goto failure;
    /* Publish a complete image; link refuses accidental overwrites of a new target. */
    if ((overwrite ? rename(temporary, path) : link(temporary, path)) != 0) goto failure;
    if (!overwrite) unlink(temporary);
    g_free(temporary);
    g_free(data);
    return TRUE;
failure:
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Opslaan mislukt: %s", g_strerror(errno));
    unlink(temporary);
    g_free(temporary);
    g_free(data);
    return FALSE;
}

static Resolution selected_resolution(App *app) {
    int i = gtk_combo_box_get_active(GTK_COMBO_BOX(app->resolution));
    if (i < 0 || i >= RESOLUTION_COUNT) i = DEFAULT_RESOLUTION_INDEX;
    return (gtk_combo_box_get_active(GTK_COMBO_BOX(app->aspect)) == 1 ? resolutions169 : resolutions43)[i];
}

static void changed(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app = data;
    gtk_widget_queue_draw(app->preview);
}

static void aspect_changed(GtkWidget *widget, gpointer data) {
    App *app = data;
    int index = gtk_combo_box_get_active(GTK_COMBO_BOX(app->resolution));
    const Resolution *list = gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == 1 ? resolutions169 : resolutions43;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(app->resolution));
    for (int i=0; i<RESOLUTION_COUNT; ++i) {
        char label[32];
        g_snprintf(label, sizeof(label), "%d x %d", list[i].width, list[i].height);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->resolution), label);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->resolution), index < 0 ? DEFAULT_RESOLUTION_INDEX : index);
    changed(widget, app);
}

static void new_code(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app = data;
    char code[5];
    do {
        if (!random_code(code)) {
            gtk_label_set_text(GTK_LABEL(app->status), "Een nieuw nummer maken is mislukt. Probeer het opnieuw.");
            return;
        }
    } while (!strcmp(code, gtk_entry_get_text(GTK_ENTRY(app->code))));
    gtk_entry_set_text(GTK_ENTRY(app->code), code);
    wchar_t locator[LOCATOR_MAX_LENGTH+1];
    read_locator(app, locator);
    locator_square_changed(app->contest_square, locator);
    changed(NULL, app);
}

static void locator_changed(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app = data;
    if (pm_mode(app) || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic))) return;
    wchar_t locator[LOCATOR_MAX_LENGTH+1];
    read_locator(app, locator);
    if (locator_square_changed(app->contest_square, locator)) new_code(NULL, app);
}

static void toggled(GtkWidget *widget, gpointer data) {
    App *app = data;
    gtk_editable_set_editable(GTK_EDITABLE(app->code), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic)));
    gboolean pm = pm_mode(app);
    locator_changed(NULL, app);
    GtkWidget *controls[] = {app->automatic, app->manual, app->code, app->inverse, app->blue_yellow,
                             app->band, app->show_sum, app->top_code, app->show};
    for (size_t i=0; i<G_N_ELEMENTS(controls); ++i) gtk_widget_set_sensitive(controls[i], !pm);
    gtk_widget_set_sensitive(app->new_code, !pm && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic)));
    if (widget == app->automatic && !pm && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic))) new_code(NULL, app);
    gtk_widget_set_sensitive(app->locator, pm || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show)));
    changed(widget, app);
}

static cairo_surface_t *current_image(App *app) {
    char *call = entry_text(app->call), *locator = entry_text(app->locator);
    char *band = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->band));
    cairo_surface_t *surface = pm_mode(app) ? render_pm(selected_resolution(app), call, locator) : render(selected_resolution(app), call,
        gtk_entry_get_text(GTK_ENTRY(app->code)), locator,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show)), band ? band : "",
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->inverse)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show_sum)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->top_code)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->blue_yellow)));
    g_free(call); g_free(locator); g_free(band);
    return surface;
}

static gboolean preview(GtkWidget *widget, cairo_t *cr, gpointer data) {
    App *app = data;
    cairo_surface_t *surface = current_image(app);
    Resolution r = selected_resolution(app);
    int w = gtk_widget_get_allocated_width(widget), h = gtk_widget_get_allocated_height(widget);
    double scale = MIN((double)w/r.width, (double)h/r.height);
    cairo_save(cr);
    cairo_translate(cr, (w-r.width*scale)/2, (h-r.height*scale)/2);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(surface);
    return FALSE;
}

static void notify_error(App *app, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void generate(GtkWidget *widget, gpointer data) {
    App *app = data;
    char *call = entry_text(app->call), *locator = entry_text(app->locator);
    if (!validate(call, valid_call)) {
        notify_error(app, "Vul een roepnaam in met letters en cijfers, eventueel met / (3 tot 24 tekens).");
        goto cleanup;
    }
    gboolean pm = pm_mode(app);
    if ((pm || locator[0] || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show))) && !validate(locator, valid_locator)) {
        notify_error(app, "Vul een geldige Maidenheadlocator in (4, 6, 8, 10 of 12 tekens), bijvoorbeeld JO21QK.");
        goto cleanup;
    }
    if (!pm && !validate(gtk_entry_get_text(GTK_ENTRY(app->code)), valid_code)) {
        notify_error(app, "Vul vier cijfers in of klik op Nieuw nummer."); goto cleanup;
    }
    gtk_entry_set_text(GTK_ENTRY(app->call), call);
    gtk_entry_set_text(GTK_ENTRY(app->locator), locator);
    for (char *c=call; *c; ++c) if (*c == '/') *c = '_';
    int band = gtk_combo_box_get_active(GTK_COMBO_BOX(app->band));
    if (band < 0 || band > 10) goto cleanup;
    char *band_file = utf8(band_files[band]);
    gboolean inverse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->inverse));
    char *color_suffix = utf8(contest_color_suffix(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->blue_yellow))));
    Resolution r = selected_resolution(app);
    wchar_t wide_locator[LOCATOR_MAX_LENGTH+1], short_locator[7];
    read_locator(app, wide_locator);
    filename_locator(short_locator, wide_locator);
    char *locator_name = utf8(short_locator);
    char *locator_suffix = *locator_name ? g_strconcat("-", locator_name, NULL) : g_strdup("");
    g_free(locator_name);
    char *name = pm ? g_strdup_printf("%s%s-PM5544-%dx%d.jpg", call, locator_suffix, r.width, r.height) : g_strdup_printf("%s%s-%s-%s-%dx%d%s%s.jpg", call, locator_suffix, gtk_entry_get_text(GTK_ENTRY(app->code)), band_file, r.width, r.height, color_suffix, inverse ? "-inverse" : "");
    g_free(locator_suffix);
    char *path = g_build_filename(app->directory, name, NULL);
    if (widget == app->export_as_menu) {
        GtkWidget *picker = gtk_file_chooser_dialog_new("Exporteren naar...", GTK_WINDOW(app->window),
            GTK_FILE_CHOOSER_ACTION_SAVE, "_Annuleren", GTK_RESPONSE_CANCEL,
            "_Opslaan", GTK_RESPONSE_ACCEPT, NULL);
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(picker);
        gtk_file_chooser_set_local_only(chooser, TRUE);
        gtk_file_chooser_set_current_folder(chooser, app->directory);
        gtk_file_chooser_set_current_name(chooser, name);
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "JPG-afbeeldingen (*.jpg;*.jpeg)");
        gtk_file_filter_add_mime_type(filter, "image/jpeg");
        gtk_file_chooser_add_filter(chooser, filter);
        int answer = gtk_dialog_run(GTK_DIALOG(picker));
        g_free(path);
        path = answer == GTK_RESPONSE_ACCEPT ? gtk_file_chooser_get_filename(chooser) : NULL;
        gtk_widget_destroy(picker);
        if (!path) goto saved;
        char *basename = g_path_get_basename(path);
        if (!strchr(basename, '.')) {
            char *with_extension = g_strconcat(path, ".jpg", NULL);
            g_free(path);
            path = with_extension;
        }
        g_free(basename);
    }
    gboolean overwrite = g_file_test(path, G_FILE_TEST_EXISTS);
    if (overwrite) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s bestaat al. Wil je dit bestand vervangen?", path);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
        int answer = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (answer != GTK_RESPONSE_YES) goto saved;
    }
    cairo_surface_t *surface = current_image(app);
    GError *error = NULL;
    if (!save_jpeg(surface, path, overwrite, &error)) {
        notify_error(app, error->message);
        g_clear_error(&error);
    } else {
        char *message = g_strdup_printf("Opgeslagen: %s", path);
        gtk_label_set_text(GTK_LABEL(app->status), message);
        g_free(message);
    }
    cairo_surface_destroy(surface);
saved:
    g_free(color_suffix); g_free(band_file); g_free(name); g_free(path);
cleanup:
    g_free(call); g_free(locator);
    changed(widget, app);
}

static void attach_field(GtkWidget *grid, const char *label, GtkWidget *field, int row) {
    GtkWidget *caption = gtk_label_new_with_mnemonic(label);
    gtk_label_set_mnemonic_widget(GTK_LABEL(caption), field);
    gtk_widget_set_halign(caption, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), caption, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), field, 0, row+1, 1, 1);
}

static void show_about(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app = data;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", app_info_text());
    gtk_window_set_title(GTK_WINDOW(dialog), "Over dit programma");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void create_ui(App *app) {
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "ATV contestnummer generator");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 940, 650);
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *info = gtk_menu_item_new_with_mnemonic("_Info");
    GtkWidget *file_menu = gtk_menu_new(), *info_menu = gtk_menu_new();
    app->export_menu = gtk_menu_item_new_with_mnemonic("_Exporteer JPG");
    app->export_as_menu = gtk_menu_item_new_with_mnemonic("Exporteren _naar...");
    app->quit_menu = gtk_menu_item_new_with_mnemonic("_Quit");
    app->about_menu = gtk_menu_item_new_with_mnemonic("_Over dit programma");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->export_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->export_as_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->quit_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(info_menu), app->about_menu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), file_menu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(info), info_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), info);
    gtk_container_add(GTK_CONTAINER(app->window), layout);
    gtk_box_pack_start(GTK_BOX(layout), menubar, FALSE, FALSE, 0);
    GtkWidget *grid = gtk_grid_new(), *fields = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(fields), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 18);
    gtk_box_pack_start(GTK_BOX(layout), grid, TRUE, TRUE, 0);
    GtkWidget *mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *mode_label = gtk_label_new_with_mnemonic("Beeld_type");
    app->mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->mode), "Contest");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->mode), "PM5544");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->mode), 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(mode_label), app->mode);
    gtk_box_pack_start(GTK_BOX(mode_row), mode_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mode_row), app->mode, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), mode_row, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), fields, 0, 1, 1, 1);
    app->call = gtk_entry_new(); app->locator = gtk_entry_new(); app->code = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(app->call), 24);
    gtk_entry_set_max_length(GTK_ENTRY(app->locator), LOCATOR_MAX_LENGTH);
    gtk_entry_set_max_length(GTK_ENTRY(app->code), 4);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->call), "PE1ITR");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->locator), "JO21QK");
    gtk_entry_set_text(GTK_ENTRY(app->code), "----");
    gtk_entry_set_input_purpose(GTK_ENTRY(app->code), GTK_INPUT_PURPOSE_DIGITS);
    attach_field(fields, "_Roepnaam", app->call, 0);
    attach_field(fields, "_QTH locator", app->locator, 2);
    app->show = gtk_check_button_new_with_mnemonic("_Locator in beeld");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show), TRUE);
    gtk_grid_attach(GTK_GRID(fields), app->show, 0, 4, 1, 1);
    app->automatic = gtk_radio_button_new_with_mnemonic(NULL, "_Automatisch nummer");
    app->manual = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(app->automatic), "Zelf _intypen");
    gtk_grid_attach(GTK_GRID(fields), app->automatic, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(fields), app->manual, 0, 6, 1, 1);
    GtkWidget *code_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_entry_set_width_chars(GTK_ENTRY(app->code), 5);
    gtk_entry_set_max_width_chars(GTK_ENTRY(app->code), 5);
    app->new_code = gtk_button_new_with_mnemonic("_Nieuw nummer");
    gtk_box_pack_start(GTK_BOX(code_row), app->code, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(code_row), app->new_code, TRUE, TRUE, 0);
    gtk_grid_attach(GTK_GRID(fields), code_row, 0, 7, 1, 1);
    app->aspect = gtk_combo_box_text_new(); app->resolution = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->aspect), "4:3");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->aspect), "16:9");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->aspect), 0);
    attach_field(fields, "_Beeldverhouding", app->aspect, 8);
    attach_field(fields, "Re_solutie", app->resolution, 10);
    app->band = gtk_combo_box_text_new();
    for (int i=0; bands[i]; ++i) {
        char *band = utf8(bands[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->band), band);
        g_free(band);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->band), 3);
    attach_field(fields, "_Frequentieband", app->band, 12);
    app->inverse = gtk_check_button_new_with_mnemonic("In_verse (kleuren omwisselen)");
    gtk_grid_attach(GTK_GRID(fields), app->inverse, 0, 14, 1, 1);
    app->show_sum = gtk_check_button_new_with_mnemonic("Cijfer_som in beeld");
    gtk_grid_attach(GTK_GRID(fields), app->show_sum, 0, 15, 1, 1);
    app->top_code = gtk_check_button_new_with_mnemonic("Code rechts_boven (DATV)");
    gtk_grid_attach(GTK_GRID(fields), app->top_code, 0, 16, 1, 1);
    app->blue_yellow = gtk_check_button_new_with_mnemonic("Blauw/_geel");
    gtk_grid_attach(GTK_GRID(fields), app->blue_yellow, 0, 17, 1, 1);
    GtkWidget *frame = gtk_frame_new("Voorbeeld");
    app->preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->preview, 480, 360);
    gtk_widget_set_hexpand(frame, TRUE); gtk_widget_set_vexpand(frame, TRUE);
    gtk_container_add(GTK_CONTAINER(frame), app->preview);
    gtk_grid_attach(GTK_GRID(grid), frame, 1, 1, 1, 1);
    GtkWidget *button = gtk_button_new_with_mnemonic("_Exporteer JPG");
    gtk_grid_attach(GTK_GRID(grid), button, 0, 2, 1, 1);
    app->status = gtk_label_new("Vul je gegevens in. JPG-bestanden worden naast het programma opgeslagen.");
    gtk_label_set_line_wrap(GTK_LABEL(app->status), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(app->status), 65);
    gtk_label_set_xalign(GTK_LABEL(app->status), 0);
    gtk_label_set_selectable(GTK_LABEL(app->status), TRUE);
    gtk_grid_attach(GTK_GRID(grid), app->status, 1, 2, 1, 1);
    aspect_changed(app->aspect, app); toggled(NULL, app);
    new_code(NULL, app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app->preview, "draw", G_CALLBACK(preview), app);
    g_signal_connect(app->aspect, "changed", G_CALLBACK(aspect_changed), app);
    g_signal_connect(app->mode, "changed", G_CALLBACK(toggled), app);
    g_signal_connect(app->automatic, "toggled", G_CALLBACK(toggled), app);
    g_signal_connect(app->show, "toggled", G_CALLBACK(toggled), app);
    g_signal_connect(app->blue_yellow, "toggled", G_CALLBACK(changed), app);
    g_signal_connect(app->inverse, "toggled", G_CALLBACK(changed), app);
    g_signal_connect(app->show_sum, "toggled", G_CALLBACK(changed), app);
    g_signal_connect(app->top_code, "toggled", G_CALLBACK(changed), app);
    g_signal_connect(app->new_code, "clicked", G_CALLBACK(new_code), app);
    g_signal_connect(button, "clicked", G_CALLBACK(generate), app);
    g_signal_connect(app->export_menu, "activate", G_CALLBACK(generate), app);
    g_signal_connect(app->export_as_menu, "activate", G_CALLBACK(generate), app);
    g_signal_connect_swapped(app->quit_menu, "activate", G_CALLBACK(gtk_widget_destroy), app->window);
    g_signal_connect(app->about_menu, "activate", G_CALLBACK(show_about), app);
    g_signal_connect(app->locator, "changed", G_CALLBACK(locator_changed), app);
    GtkWidget *inputs[] = {app->call, app->locator, app->code, app->band, app->resolution};
    for (size_t i=0; i<G_N_ELEMENTS(inputs); ++i) g_signal_connect(inputs[i], "changed", G_CALLBACK(changed), app);
}

static int smoke_test(const char *directory) {
    if (g_mkdir_with_parents(directory, 0755) != 0) return 1;
    for (int i=0; i<1000; ++i) {
        char code[5];
        if (!random_code(code) || !validate(code, valid_code) || !generated_code_valid((unsigned)atoi(code))) return 2;
    }
    for (int blue_yellow=0; blue_yellow<2; ++blue_yellow)
    for (int top_code=0; top_code<2; ++top_code)
    for (int show_sum=0; show_sum<2; ++show_sum)
    for (int inverse=0; inverse<2; ++inverse)
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int show=0; show<2; ++show) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        char *path = g_strdup_printf("%s/test-%dx%d-locator%d%s%s%s%s.jpg", directory, r.width, r.height, show, blue_yellow ? "-blauw-geel" : "", inverse ? "-inverse" : "", show_sum ? "-sum" : "", top_code ? "-top" : "");
        cairo_surface_t *surface = render(r, "PE1ITR/P", "1957", "JO21QK86DV", show, "436 MHz", inverse, show_sum, top_code, blue_yellow);
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 3; }
    }
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int text=0; text<2; ++text) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        char *path = g_strdup_printf("%s/pm-%dx%d-%d.jpg", directory, r.width, r.height, text);
        cairo_surface_t *surface = render_pm(r, text ? "PE1ITR/P" : "", text ? "JO21QK86DV" : "");
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 4; }
    }
    g_print("680 JPEGs generated; 1000 random codes checked.\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--smoke-test")) return smoke_test(argv[2]);
    if (argc != 1) { g_printerr("Gebruik: %s [--smoke-test uitvoermap]\n", argv[0]); return 1; }
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("Kan geen grafische sessie openen. Start vanuit je Linux-desktop.\n"); return 1;
    }
    GError *error = NULL;
    char *executable = g_file_read_link("/proc/self/exe", &error);
    if (!executable) { g_printerr("%s\n", error->message); g_error_free(error); return 1; }
    App app = {0};
    app.directory = g_path_get_dirname(executable);
    g_free(executable);
    create_ui(&app);
    gtk_widget_show_all(app.window);
    gtk_main();
    pm5544_cleanup();
    g_free(app.directory);
    return 0;
}
