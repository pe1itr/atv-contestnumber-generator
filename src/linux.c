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
#include "datv.h"

typedef struct {
    GtkWidget *window, *call, *locator, *show, *automatic, *code;
    GtkWidget *aspect, *resolution, *band, *preview, *status, *inverse, *blue_yellow, *show_sum, *top_code, *ebu_top, *ebu_bottom, *mode, *manual, *new_code;
    GtkWidget *export_menu, *export_as_menu, *about_menu, *quit_menu;
    GtkWidget *ts_menu, *level1, *level2;
    gboolean restoring;
    DatvSettings datv;
    DvbSettings ts_dvb, udp_dvb;
    GtkWidget *udp_menu;
    DatvUdpSettings udp;
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
                      int right, int bottom, int size, PangoAlignment alignment, const ContestPalette *backdrop) {
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
    if (backdrop) {
        double x, y; cairo_get_current_point(cr, &x, &y);
        cairo_save(cr);
        cairo_set_source_rgb(cr, backdrop->background.red/255.0, backdrop->background.green/255.0, backdrop->background.blue/255.0);
        cairo_rectangle(cr, x, y, width, height); cairo_fill(cr);
        cairo_restore(cr); cairo_move_to(cr, x, y);
    }
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}

static cairo_surface_t *render(Resolution r, const char *call, const char *code,
                               const char *locator, gboolean show, const char *band, gboolean inverse, gboolean show_sum, gboolean top_code, gboolean blue_yellow, gboolean ebu_top, gboolean ebu_bottom) {
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, r.width, r.height);
    cairo_t *cr = cairo_create(surface);
    ContestPalette palette = contest_palette(blue_yellow, inverse);
    cairo_set_source_rgb(cr, palette.background.red/255.0, palette.background.green/255.0, palette.background.blue/255.0);
    cairo_paint(cr);
    for (int i=0; i<EBU_BAR_COUNT; ++i) {
        ContestColor c = ebu_colors[i];
        int left = r.width*i/EBU_BAR_COUNT, right = r.width*(i+1)/EBU_BAR_COUNT;
        cairo_set_source_rgb(cr, c.red/255.0, c.green/255.0, c.blue/255.0);
        if (ebu_top) cairo_rectangle(cr, left, 0, right-left, r.height*EBU_STRIP_PERCENT/100);
        if (ebu_bottom) {
            int y = r.height*(100-EBU_STRIP_PERCENT)/100;
            cairo_rectangle(cr, left, y, right-left, r.height-y);
        }
        cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, palette.foreground.red/255.0, palette.foreground.green/255.0, palette.foreground.blue/255.0);
    cairo_font_options_t *options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options(cr, options);
    cairo_font_options_destroy(options);
    int w = r.width, h = r.height, m = w/40;
    int small_size = MAX(7, h*6/100);
    draw_line(cr, call, m, (top_code || ebu_top) ? h*9/100 : h/50, w-m, h*22/100, h*17/100, PANGO_ALIGN_CENTER, NULL);
    draw_line(cr, code, m, h*22/100, w-m, h*(show ? 80 : 91)/100, h*70/100, PANGO_ALIGN_CENTER, NULL);
    if (show) draw_line(cr, locator, m, h*80/100, w-m, h*91/100, h*14/100, PANGO_ALIGN_CENTER, NULL);
    draw_line(cr, band, w/2, h*91/100, w-m, h*99/100, small_size, PANGO_ALIGN_RIGHT, ebu_bottom ? &palette : NULL);
    wchar_t digits[5] = {0};
    if (strlen(code) == 4) for (int i=0; i<4; ++i) digits[i] = (unsigned char)code[i];
    int sum = code_digit_sum(digits);
    if (top_code && sum >= 0)
        draw_line(cr, code, w/2, h/100, w-m, h*9/100, small_size, PANGO_ALIGN_RIGHT, ebu_top ? &palette : NULL);
    if (show_sum && sum >= 0) {
        char label[32];
        g_snprintf(label, sizeof(label), "Som=%d", sum);
        draw_line(cr, label, m, h*91/100, w/2, h*99/100, small_size, PANGO_ALIGN_LEFT, ebu_bottom ? &palette : NULL);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    return surface;
}

static cairo_surface_t *render_pattern(Resolution r, const char *call, const char *locator, int mode) {
    wchar_t wc[25] = {0}, wl[LOCATOR_MAX_LENGTH+1] = {0};
    const char *p = call;
    for (int i=0; i<24 && *p; ++i, p=g_utf8_next_char(p)) wc[i] = (wchar_t)g_utf8_get_char(p);
    p = locator;
    for (int i=0; i<LOCATOR_MAX_LENGTH && *p; ++i, p=g_utf8_next_char(p)) wl[i] = (wchar_t)g_utf8_get_char(p);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, r.width, r.height);
    if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
        cairo_surface_flush(surface);
        if (!(mode==IMAGE_FUBK ? fubk_render : mode==IMAGE_PM5644 ? pm5644_render : pm5544_render)((uint32_t *)cairo_image_surface_get_data(surface), r.width, r.height, wc, wl)) {
            cairo_surface_destroy(surface);
            return cairo_image_surface_create(CAIRO_FORMAT_RGB24, -1, -1);
        }
        cairo_surface_mark_dirty(surface);
    }
    return surface;
}
static int selected_mode(App *app) {
    int row=gtk_combo_box_get_active(GTK_COMBO_BOX(app->mode));
    return row>=0 && row<IMAGE_MODE_COUNT ? image_mode_order[row] : IMAGE_CONTEST;
}
static gboolean pattern_mode(App *app) { return selected_mode(app)>IMAGE_CONTEST; }

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
    if (app->restoring) return;
    if (pattern_mode(app) || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic))) return;
    wchar_t locator[LOCATOR_MAX_LENGTH+1];
    read_locator(app, locator);
    if (locator_square_changed(app->contest_square, locator)) new_code(NULL, app);
}

static void toggled(GtkWidget *widget, gpointer data) {
    App *app = data;
    if (app->restoring) return;
    gtk_editable_set_editable(GTK_EDITABLE(app->code), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic)));
    gboolean pm = pattern_mode(app);
    locator_changed(NULL, app);
    GtkWidget *controls[] = {app->automatic, app->manual, app->code, app->inverse, app->blue_yellow,
                             app->band, app->show_sum, app->top_code, app->show, app->ebu_top, app->ebu_bottom};
    for (size_t i=0; i<G_N_ELEMENTS(controls); ++i) gtk_widget_set_sensitive(controls[i], !pm);
    gtk_widget_set_sensitive(app->new_code, !pm && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic)));
    if (widget == app->automatic && !pm && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic))) new_code(NULL, app);
    gtk_widget_set_sensitive(app->locator, pm || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show)));
    changed(widget, app);
}

static cairo_surface_t *current_image(App *app) {
    char *call = entry_text(app->call), *locator = entry_text(app->locator);
    char *band = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->band));
    cairo_surface_t *surface = pattern_mode(app) ? render_pattern(selected_resolution(app), call, locator, selected_mode(app)) : render(selected_resolution(app), call,
        gtk_entry_get_text(GTK_ENTRY(app->code)), locator,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show)), band ? band : "",
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->inverse)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show_sum)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->top_code)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->blue_yellow)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->ebu_top)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->ebu_bottom)));
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

typedef struct {
    App *app;
    GtkWidget *progress;
    cairo_surface_t *image;
    char *path, *call;
    gboolean overwrite;
    DatvSettings settings;
    DatvResult result;
} TsJob;

static void ts_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel) {
    (void)source; (void)cancel;
    TsJob *job=data;
    char error[256]="";
    char *temporary=g_strconcat(job->path,".XXXXXX",NULL);
    int fd=g_mkstemp(temporary);
    FILE *file=fd<0?NULL:fdopen(fd,"wb");
    if (!file && fd>=0) close(fd);
    gboolean ok=file && datv_write(file,(uint32_t *)cairo_image_surface_get_data(job->image),
        cairo_image_surface_get_width(job->image),cairo_image_surface_get_height(job->image),
        cairo_image_surface_get_stride(job->image),job->call,job->settings,&job->result,error);
    if (file) {
        if (ok && fsync(fileno(file))!=0) ok=FALSE;
        if (fclose(file)!=0) ok=FALSE;
    }
    if (ok && (job->overwrite?rename(temporary,job->path):link(temporary,job->path))!=0) ok=FALSE;
    unlink(temporary); g_free(temporary);
    if (ok) g_task_return_boolean(task,TRUE);
    else g_task_return_new_error(task,G_IO_ERROR,G_IO_ERROR_FAILED,"%s",
        error[0]?error:"TS opslaan is mislukt. Controleer pad, vrije ruimte en schrijfrechten.");
}
static void ts_done(GObject *source, GAsyncResult *result, gpointer data) {
    (void)source;
    TsJob *job=data;
    GError *error=NULL;
    gboolean ok=g_task_propagate_boolean(G_TASK(result),&error);
    gtk_widget_destroy(job->progress);
    if (!ok) { notify_error(job->app,error->message); g_error_free(error); }
    else {
        char *message=g_strdup_printf("TS opgeslagen: %s (QP %d, %d beelden, grootste IDR %d bytes)",
            job->path,job->result.qp,job->result.frames,job->result.largest_idr);
        gtk_label_set_text(GTK_LABEL(job->app->status),message); g_free(message);
    }
    cairo_surface_destroy(job->image); g_free(job->path); g_free(job->call); g_free(job);
}
static gboolean keep_progress(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data; return TRUE;
}

#include "dvb_linux.h"
typedef struct {
    App *app;
    GtkWidget *dialog, *fields[5], *status;
    DvbControls dvb;
    DatvStream *stream;
} UdpDialog;
static gboolean udp_poll(gpointer data) {
    UdpDialog *d=data;
    if (!d->stream) return G_SOURCE_CONTINUE;
    DatvUdpStatus status; datv_udp_status(d->stream,&status);
    char text[512]; datv_udp_status_text(d->app->udp,status,text,sizeof(text));
    gtk_label_set_text(GTK_LABEL(d->status),text);
    gboolean busy=status.state==DATV_PREPARING || status.state==DATV_RUNNING;
    for (int i=0;i<5;++i) gtk_widget_set_sensitive(d->fields[i],i!=2 && !busy);
    gtk_widget_set_sensitive(d->dvb.box,!busy);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d->dialog),4,!busy);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d->dialog),1,!busy);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d->dialog),3,!busy);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d->dialog),2,busy);
    return G_SOURCE_CONTINUE;
}
#include "quality_linux.h"

static void output_udp(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app=data;
    char *call=entry_text(app->call), *locator=entry_text(app->locator);
    gboolean show=pattern_mode(app)||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show));
    if (!validate(call,valid_call) || ((show||*locator) && !validate(locator,selected_mode(app)==IMAGE_FUBK?valid_fubk_locator:valid_locator)) ||
        (!pattern_mode(app) && !validate(gtk_entry_get_text(GTK_ENTRY(app->code)),valid_code))) {
        notify_error(app,"Vul eerst een geldige roepnaam en locator in (FUBK: minimaal 6 locatortekens). De contestcode moet vier cijfers bevatten, niet alle vier gelijk en geen oplopende of aflopende reeks.");
        g_free(call); g_free(locator); return;
    }
    g_free(locator);
    Resolution r=selected_resolution(app);
    const char *invalid=datv_validate(app->udp.video,r.width,r.height);
    if (invalid) { notify_error(app,invalid); g_free(call); return; }
    cairo_surface_t *im=current_image(app);
    if (cairo_surface_status(im)!=CAIRO_STATUS_SUCCESS) {
        notify_error(app,"Kan het beeld niet maken."); g_free(call); cairo_surface_destroy(im); return;
    }
    UdpDialog d={0}; d.app=app;
    d.dialog=gtk_dialog_new_with_buttons("DATV: UDP-uitvoer",GTK_WINDOW(app->window),GTK_DIALOG_MODAL,
        "Beeld _controleren",4,"_Start",1,"S_top",2,"_Toepassen en sluiten",3,"_Sluiten",GTK_RESPONSE_CLOSE,NULL);
    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),8); gtk_grid_set_column_spacing(GTK_GRID(grid),12);
    gtk_container_set_border_width(GTK_CONTAINER(grid),16);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d.dialog))),grid);
    const char *labels[]={"IP-adres (IPv4)","Poort","Berekende TS-bitrate (bit/s)","Beelden per seconde","GOP (beelden; 1 = alleen IDR)"};
    int values[]={0,app->udp.port,app->udp.video.bitrate,app->udp.video.fps,app->udp.video.gop};
    int mins[]={0,1,0,1,1}, maxs[]={0,65535,2000000,25,250};
    for (int i=0;i<5;++i) {
        GtkWidget *label=gtk_label_new(labels[i]); gtk_label_set_xalign(GTK_LABEL(label),0);
        d.fields[i]=i?gtk_spin_button_new_with_range(mins[i],maxs[i],i==2?1000:1):gtk_entry_new();
        if (i) gtk_spin_button_set_value(GTK_SPIN_BUTTON(d.fields[i]),values[i]);
        else {
            gtk_entry_set_max_length(GTK_ENTRY(d.fields[i]),15);
            gtk_entry_set_text(GTK_ENTRY(d.fields[i]),app->udp.ip);
            gtk_entry_set_placeholder_text(GTK_ENTRY(d.fields[i]),"192.168.1.50");
        }
        gtk_grid_attach(GTK_GRID(grid),label,0,i,1,1); gtk_grid_attach(GTK_GRID(grid),d.fields[i],1,i,1,1);
    }
    dvb_controls_init(&d.dvb,d.fields[2],app->udp_dvb);
    gtk_grid_attach(GTK_GRID(grid),d.dvb.box,0,5,3,1);
    GtkWidget *note=gtk_label_new("Tip voor de contest: gebruik 4 fps en GOP 2.\n\nStart zendt het huidige beeld, zonder audio.\nStop en sluit dit venster om het beeld te wijzigen. Sluiten stopt ook de stream.");
    gtk_grid_attach(GTK_GRID(grid),note,0,6,3,1);
    d.status=gtk_label_new("Vul het IP-adres van Portsdown in en kies Start.");
    gtk_label_set_line_wrap(GTK_LABEL(d.status),TRUE); gtk_label_set_max_width_chars(GTK_LABEL(d.status),65);
    gtk_label_set_xalign(GTK_LABEL(d.status),0); gtk_grid_attach(GTK_GRID(grid),d.status,0,7,3,1);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d.dialog),2,FALSE);
    gtk_widget_show_all(d.dialog);
    guint timer=g_timeout_add(200,udp_poll,&d);
    for (;;) {
        int response=gtk_dialog_run(GTK_DIALOG(d.dialog));
        if (response==2) { datv_udp_stop(d.stream); continue; }
        if (response!=1 && response!=3 && response!=4) break;
        if (d.stream) {
            DatvUdpStatus status; datv_udp_status(d.stream,&status);
            if (status.state==DATV_PREPARING || status.state==DATV_RUNNING) continue;
            datv_udp_destroy(d.stream); d.stream=NULL;
        }
        DatvUdpSettings s=app->udp;
        g_strlcpy(s.ip,gtk_entry_get_text(GTK_ENTRY(d.fields[0])),sizeof(s.ip));
        for (int i=1;i<5;++i) if (i!=2) gtk_spin_button_update(GTK_SPIN_BUTTON(d.fields[i]));
        s.port=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d.fields[1]));
        DvbSettings radio=dvb_controls_read(&d.dvb);
        s.video.bitrate=dvb_bitrate(radio);
        s.video.fps=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d.fields[3]));
        s.video.gop=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d.fields[4]));
        DatvUdpSettings check=s;
        if (response==3 && !check.ip[0]) g_strlcpy(check.ip,"127.0.0.1",sizeof(check.ip));
        invalid=response==4 ? datv_validate(s.video,r.width,r.height) : datv_udp_validate(check,r.width,r.height);
        if (invalid) { gtk_label_set_text(GTK_LABEL(d.status),invalid); continue; }
        if (response==4) { quality_compare(GTK_WINDOW(d.dialog),im,r,call,s.video); continue; }
        app->udp=s; app->udp_dvb=radio;
        if (response==3) break;
        datv_udp_destroy(d.stream); d.stream=NULL;
        char error[256];
        d.stream=datv_udp_start((uint32_t *)cairo_image_surface_get_data(im),r.width,r.height,
            cairo_image_surface_get_stride(im),call,s,error);
        if (!d.stream) gtk_label_set_text(GTK_LABEL(d.status),error);
        else { app->udp=s; udp_poll(&d); }
    }
    g_source_remove(timer); datv_udp_destroy(d.stream);
    gtk_widget_destroy(d.dialog); cairo_surface_destroy(im); g_free(call);
}
static void export_ts(GtkWidget *widget, gpointer data) {
    (void)widget;
    App *app=data;
    char *call=entry_text(app->call), *locator=entry_text(app->locator);
    gboolean show=pattern_mode(app)||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show));
    if (!validate(call,valid_call) || ((show||*locator) && !validate(locator,selected_mode(app)==IMAGE_FUBK?valid_fubk_locator:valid_locator)) ||
        (!pattern_mode(app) && !validate(gtk_entry_get_text(GTK_ENTRY(app->code)),valid_code))) {
        notify_error(app,"Vul eerst een geldige roepnaam en locator in (FUBK: minimaal 6 locatortekens). De contestcode moet vier cijfers bevatten, niet alle vier gelijk en geen oplopende of aflopende reeks.");
        g_free(call); g_free(locator); return;
    }
    g_free(locator);
    Resolution r=selected_resolution(app);
    const char *invalid=datv_validate(app->datv,r.width,r.height);
    if (invalid) { notify_error(app,invalid); g_free(call); return; }
    GtkWidget *dialog=gtk_dialog_new_with_buttons("DATV: TS-proefbestand",GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,"_Annuleren",GTK_RESPONSE_CANCEL,
        "_Verder",GTK_RESPONSE_ACCEPT,NULL);
    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),8); gtk_grid_set_column_spacing(GTK_GRID(grid),12);
    gtk_container_set_border_width(GTK_CONTAINER(grid),16);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),grid);
    const char *labels[]={"Berekende TS-bitrate (bit/s)","Beeldduur (seconden)","Beelden per seconde","GOP (beelden; 1 = alleen IDR)"};
    int values[]={app->datv.bitrate,app->datv.seconds,app->datv.fps,app->datv.gop};
    int minimum[]={0,1,1,1}, maximum[]={2000000,60,25,250};
    GtkWidget *fields[4];
    for (int i=0; i<4; ++i) {
        GtkWidget *label=gtk_label_new(labels[i]); gtk_label_set_xalign(GTK_LABEL(label),0);
        fields[i]=gtk_spin_button_new_with_range(minimum[i],maximum[i],i==0?1000:1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields[i]),values[i]);
        gtk_grid_attach(GTK_GRID(grid),label,0,i,1,1); gtk_grid_attach(GTK_GRID(grid),fields[i],1,i,1,1);
    }
    DvbControls radio_controls;
    dvb_controls_init(&radio_controls,fields[0],app->ts_dvb);
    gtk_grid_attach(GTK_GRID(grid),radio_controls.box,0,4,3,1);
    GtkWidget *note=gtk_label_new("Huidig beeld, zonder audio. Service = roepnaam; ID = 1.\nHet bestand bevat ook 1 seconde aanloop voor de decoder.");
    gtk_grid_attach(GTK_GRID(grid),note,0,5,3,1);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog))!=GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog); g_free(call); return;
    }
    for (int i=1; i<4; ++i) gtk_spin_button_update(GTK_SPIN_BUTTON(fields[i]));
    app->ts_dvb=dvb_controls_read(&radio_controls);
    app->datv=(DatvSettings){dvb_bitrate(app->ts_dvb),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(fields[1])),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(fields[2])),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(fields[3]))};
    gtk_widget_destroy(dialog);
    GtkWidget *picker=gtk_file_chooser_dialog_new("TS opslaan",GTK_WINDOW(app->window),GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Annuleren",GTK_RESPONSE_CANCEL,"_Opslaan",GTK_RESPONSE_ACCEPT,NULL);
    GtkFileChooser *chooser=GTK_FILE_CHOOSER(picker);
    gtk_file_chooser_set_local_only(chooser,TRUE);
    gtk_file_chooser_set_current_folder(chooser,app->directory);
    char *safe=g_strdup(call); for (char *p=safe; *p; ++p) if (*p=='/') *p='_';
    char *mode_name=utf8(image_modes[selected_mode(app)]);
    char *name=g_strdup_printf("%s-%s-%dx%d-%dbps.ts",safe,pattern_mode(app)?mode_name:gtk_entry_get_text(GTK_ENTRY(app->code)),r.width,r.height,app->datv.bitrate);
    gtk_file_chooser_set_current_name(chooser,name); g_free(safe); g_free(name); g_free(mode_name);
    GtkFileFilter *filter=gtk_file_filter_new(); gtk_file_filter_set_name(filter,"MPEG-TS (*.ts)");
    gtk_file_filter_add_pattern(filter,"*.ts"); gtk_file_chooser_add_filter(chooser,filter);
    char *path=gtk_dialog_run(GTK_DIALOG(picker))==GTK_RESPONSE_ACCEPT?gtk_file_chooser_get_filename(chooser):NULL;
    gtk_widget_destroy(picker);
    if (!path) { g_free(call); return; }
    char *basename=g_path_get_basename(path);
    if (!strchr(basename,'.')) {
        char *extended=g_strconcat(path,".ts",NULL); g_free(path); path=extended;
    }
    g_free(basename);
    gboolean overwrite=g_file_test(path,G_FILE_TEST_EXISTS);
    if (overwrite) {
        GtkWidget *confirm=gtk_message_dialog_new(GTK_WINDOW(app->window),GTK_DIALOG_MODAL,
            GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"%s bestaat al. Wil je dit bestand vervangen?",path);
        gtk_dialog_set_default_response(GTK_DIALOG(confirm),GTK_RESPONSE_NO);
        int answer=gtk_dialog_run(GTK_DIALOG(confirm)); gtk_widget_destroy(confirm);
        if (answer!=GTK_RESPONSE_YES) { g_free(call); g_free(path); return; }
    }
    cairo_surface_t *image=current_image(app);
    if (cairo_surface_status(image)!=CAIRO_STATUS_SUCCESS) {
        notify_error(app,"Kan het beeld niet maken."); cairo_surface_destroy(image); g_free(call); g_free(path); return;
    }
    TsJob *job=g_new0(TsJob,1);
    job->app=app; job->call=call; job->path=path; job->image=image; job->settings=app->datv;
    job->overwrite=overwrite;
    job->progress=gtk_message_dialog_new(GTK_WINDOW(app->window),GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,GTK_BUTTONS_NONE,"TS maken en bitrate controleren...");
    g_signal_connect(job->progress,"delete-event",G_CALLBACK(keep_progress),NULL);
    gtk_widget_show(job->progress);
    GTask *task=g_task_new(NULL,NULL,ts_done,job); g_task_set_task_data(task,job,NULL);
    g_task_run_in_thread(task,ts_worker); g_object_unref(task);
}
static void genius_changed(GtkCheckMenuItem *item, gpointer data) {
    App *app=data;
    gtk_widget_set_visible(app->ts_menu,gtk_check_menu_item_get_active(item));
    gtk_widget_set_visible(app->udp_menu,gtk_check_menu_item_get_active(item));
}

static void generate(GtkWidget *widget, gpointer data) {
    App *app = data;
    char *call = entry_text(app->call), *locator = entry_text(app->locator);
    if (!validate(call, valid_call)) {
        notify_error(app, "Vul een roepnaam in met letters en cijfers, eventueel met / (3 tot 24 tekens).");
        goto cleanup;
    }
    gboolean pm = pattern_mode(app);
    if ((pm || locator[0] || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show))) && !validate(locator, selected_mode(app)==IMAGE_FUBK?valid_fubk_locator:valid_locator)) {
        notify_error(app, "Vul een geldige Maidenheadlocator in (4, 6, 8, 10 of 12 tekens), bijvoorbeeld JO21QK. FUBK vereist minimaal 6 tekens.");
        goto cleanup;
    }
    if (!pm && !validate(gtk_entry_get_text(GTK_ENTRY(app->code)), valid_code)) {
        notify_error(app, "Vul vier cijfers in; niet alle vier gelijk en geen oplopende of aflopende reeks (zoals 4567 of 5432)."); goto cleanup;
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
    char *mode_name = utf8(image_modes[selected_mode(app)]);
    char *name = pm ? g_strdup_printf("%s%s-%s-%dx%d.jpg", call, locator_suffix, mode_name, r.width, r.height) : g_strdup_printf("%s%s-%s-%s-%dx%d%s%s.jpg", call, locator_suffix, gtk_entry_get_text(GTK_ENTRY(app->code)), band_file, r.width, r.height, color_suffix, inverse ? "-inverse" : "");
    g_free(locator_suffix); g_free(mode_name);
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
static void show_codec_license(GtkWidget *widget, gpointer data) {
    (void)widget; App *app=data;
    GtkWidget *dialog=gtk_message_dialog_new(GTK_WINDOW(app->window),GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE,"%s",app_codec_license());
    gtk_window_set_title(GTK_WINDOW(dialog),"OpenH264-licentie");
    gtk_dialog_run(GTK_DIALOG(dialog)); gtk_widget_destroy(dialog);
}

static AppConfig capture_config(App *app) {
    AppConfig s=config_defaults();
    g_strlcpy(s.call,gtk_entry_get_text(GTK_ENTRY(app->call)),sizeof(s.call));
    g_strlcpy(s.locator,gtk_entry_get_text(GTK_ENTRY(app->locator)),sizeof(s.locator));
    g_strlcpy(s.code,gtk_entry_get_text(GTK_ENTRY(app->code)),sizeof(s.code));
    s.mode=selected_mode(app);
    s.aspect=gtk_combo_box_get_active(GTK_COMBO_BOX(app->aspect));
    s.resolution=gtk_combo_box_get_active(GTK_COMBO_BOX(app->resolution));
    s.band=gtk_combo_box_get_active(GTK_COMBO_BOX(app->band));
    s.automatic=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->automatic));
    s.show=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show));
    s.inverse=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->inverse));
    s.blue_yellow=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->blue_yellow));
    s.show_sum=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show_sum));
    s.top_code=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->top_code));
    s.ebu_top=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->ebu_top));
    s.ebu_bottom=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->ebu_bottom));
    s.genius=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(app->level2))?2:1;
    s.ts=app->datv; s.udp=app->udp; s.ts_dvb=app->ts_dvb; s.udp_dvb=app->udp_dvb; return s;
}
static void apply_config(App *app, const AppConfig *s) {
    app->restoring=TRUE;
    gtk_entry_set_text(GTK_ENTRY(app->call),s->call);
    gtk_entry_set_text(GTK_ENTRY(app->locator),s->locator);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->mode),image_mode_row(s->mode));
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->aspect),s->aspect);
    aspect_changed(app->aspect,app);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->resolution),s->resolution);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->band),s->band);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->automatic?app->automatic:app->manual),TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show),s->show);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->inverse),s->inverse);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->blue_yellow),s->blue_yellow);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show_sum),s->show_sum);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->top_code),s->top_code);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->ebu_top),s->ebu_top);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->ebu_bottom),s->ebu_bottom);
    gtk_entry_set_text(GTK_ENTRY(app->code),s->code);
    app->contest_square[0]=0;
    wchar_t locator[LOCATOR_MAX_LENGTH+1]; read_locator(app,locator);
    locator_square_changed(app->contest_square,locator);
    app->datv=s->ts; app->udp=s->udp; app->ts_dvb=s->ts_dvb; app->udp_dvb=s->udp_dvb;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(s->genius==2?app->level2:app->level1),TRUE);
    app->restoring=FALSE; toggled(NULL,app);
}
static void save_config(GtkWidget *widget, gpointer data) {
    (void)widget; App *app=data; AppConfig s=capture_config(app);
    char *path=g_build_filename(app->directory,CONFIG_FILENAME,NULL);
    if (config_save(path,&s)) {
        char *message=g_strdup_printf("Instellingen opgeslagen in %s",path);
        gtk_label_set_text(GTK_LABEL(app->status),message); g_free(message);
    } else notify_error(app,"Instellingen opslaan mislukt. Controleer de invoer en schrijfrechten naast het programma.");
    g_free(path);
}
static void load_config(App *app) {
    AppConfig s; char *path=g_build_filename(app->directory,CONFIG_FILENAME,NULL);
    int result=config_load(path,&s); g_free(path);
    if (result==1) {
        apply_config(app,&s);
        gtk_label_set_text(GTK_LABEL(app->status),"Opgeslagen instellingen geladen. UDP-uitvoer staat uit.");
    } else if (result<0) {
        notify_error(app,"Het configuratiebestand is ongeldig of onleesbaar. De standaardinstellingen worden gebruikt.");
    }
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
    app->datv=datv_defaults();
    app->udp=datv_udp_defaults();
    app->ts_dvb=app->udp_dvb=dvb_defaults();
    app->datv.bitrate=app->udp.video.bitrate=dvb_bitrate(app->ts_dvb);
    GtkWidget *config=gtk_menu_item_new_with_label("Config"), *config_menu=gtk_menu_new();
    GtkWidget *level1=gtk_radio_menu_item_new_with_label(NULL,"Genius level 1 (standaard)");
    GtkWidget *level2=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(level1),"Genius level 2");
    app->level1=level1; app->level2=level2;
    GtkWidget *save=gtk_menu_item_new_with_label("Huidige instellingen opslaan");
    g_signal_connect(save,"activate",G_CALLBACK(save_config),app);
    gtk_menu_shell_append(GTK_MENU_SHELL(config_menu),level1); gtk_menu_shell_append(GTK_MENU_SHELL(config_menu),level2);
    gtk_menu_shell_append(GTK_MENU_SHELL(config_menu),gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(config_menu),save);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(config),config_menu);
    app->ts_menu=gtk_menu_item_new_with_label("Exporteer TS-proefbestand...");
    gtk_widget_set_no_show_all(app->ts_menu,TRUE);
    app->udp_menu=gtk_menu_item_new_with_label("DATV UDP-uitvoer...");
    gtk_widget_set_no_show_all(app->udp_menu,TRUE);
    g_signal_connect(app->udp_menu,"activate",G_CALLBACK(output_udp),app);
    g_signal_connect(level2,"toggled",G_CALLBACK(genius_changed),app);
    g_signal_connect(app->ts_menu,"activate",G_CALLBACK(export_ts),app);
    app->export_menu = gtk_menu_item_new_with_mnemonic("_Exporteer JPG");
    app->export_as_menu = gtk_menu_item_new_with_mnemonic("Exporteren _naar...");
    app->quit_menu = gtk_menu_item_new_with_mnemonic("_Quit");
    app->about_menu = gtk_menu_item_new_with_mnemonic("_Over dit programma");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->export_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->export_as_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->ts_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->udp_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), app->quit_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(info_menu), app->about_menu);
    GtkWidget *license=gtk_menu_item_new_with_label("OpenH264-licentie");
    gtk_menu_shell_append(GTK_MENU_SHELL(info_menu),license);
    g_signal_connect(license,"activate",G_CALLBACK(show_codec_license),app);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), file_menu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(info), info_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), config);
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
    for (int i=0; i<IMAGE_MODE_COUNT; ++i) {
        char *name=utf8(image_modes[image_mode_order[i]]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->mode),name); g_free(name);
    }
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
    GtkWidget *ebu_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->ebu_top = gtk_check_button_new_with_mnemonic("EBU b_oven");
    app->ebu_bottom = gtk_check_button_new_with_mnemonic("EBU o_nder");
    gtk_box_pack_start(GTK_BOX(ebu_row), app->ebu_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ebu_row), app->ebu_bottom, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(fields), ebu_row, 0, 18, 2, 1);
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
    g_signal_connect(app->ebu_top, "toggled", G_CALLBACK(changed), app);
    g_signal_connect(app->ebu_bottom, "toggled", G_CALLBACK(changed), app);
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
        cairo_surface_t *surface = render(r, "PE1ITR/P", "1957", "JO21QK86DV", show, "436 MHz", inverse, show_sum, top_code, blue_yellow, FALSE, FALSE);
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 3; }
    }
    for (int flags=0; flags<4; ++flags)
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        char *path = g_strdup_printf("%s/ebu-%dx%d-%d.jpg", directory, r.width, r.height, flags);
        cairo_surface_t *surface = render(r, "PE1ITR/P", "1957", "JO21QK86DV", TRUE, "436 MHz", FALSE, TRUE, TRUE, FALSE, flags&1, flags&2);
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 3; }
    }
    for (int philips=0; philips<2; ++philips)
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int text=0; text<2; ++text) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        char *path = g_strdup_printf("%s/%s-%dx%d-%d.jpg", directory, philips ? "pm5644" : "pm", r.width, r.height, text);
        cairo_surface_t *surface = render_pattern(r, text ? "PE1ITR/P" : "", text ? "JO21QK86DV" : "", philips ? IMAGE_PM5644 : IMAGE_PM5544);
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 4; }
    }
    for (int aspect=0; aspect<2; ++aspect) for (int i=0; i<RESOLUTION_COUNT; ++i) for (int text=0; text<3; ++text) {
        Resolution r = (aspect ? resolutions169 : resolutions43)[i];
        char *path = g_strdup_printf("%s/fubk-%dx%d-%d.jpg", directory, r.width, r.height, text);
        cairo_surface_t *surface = render_pattern(r, text ? "PE1ITR/P" : "", text==2 ? "JO21QK" : text ? "JO21QK86DV" : "", IMAGE_FUBK);
        GError *error = NULL;
        gboolean ok = save_jpeg(surface, path, TRUE, &error);
        cairo_surface_destroy(surface); g_free(path);
        if (!ok) { g_printerr("%s\n", error->message); g_error_free(error); return 4; }
    }
    g_print("%d JPEGs generated; 1000 random codes checked.\n", 86*RESOLUTION_COUNT);
    return 0;
}

int main(int argc, char **argv) {
    if (argc>=3 && (!strcmp(argv[1],"--ts-test") || !strcmp(argv[1],"--ts-ebu-test") || !strcmp(argv[1],"--ts-fubk-test") || !strcmp(argv[1],"--ts-pm5644-test"))) {
        DatvSettings s; int w,h;
        if (!datv_test_options(argc-3,(const char *const *)(argv+3),&s,&w,&h)) {
            g_printerr("Ongeldige TS-testinstellingen.\n"); return 1;
        }
        cairo_surface_t *im=!strcmp(argv[1],"--ts-pm5644-test") ? render_pattern((Resolution){w,h},"PE1ITR","JO21QK86DV",IMAGE_PM5644) : !strcmp(argv[1],"--ts-fubk-test") ? render_pattern((Resolution){w,h},"PE1ITR","JO21QK86DV",IMAGE_FUBK) : render((Resolution){w,h},"PE1ITR","1957","JO21QK",TRUE,"436 MHz",FALSE,TRUE,TRUE,FALSE, !strcmp(argv[1],"--ts-ebu-test"), !strcmp(argv[1],"--ts-ebu-test"));
        FILE *f=fopen(argv[2],"wbx"); char error[256]="Kan geen nieuw TS-bestand maken (bestaat het al?).";
        DatvResult result;
        int ok=f && cairo_surface_status(im)==CAIRO_STATUS_SUCCESS && datv_write(f,
            (uint32_t *)cairo_image_surface_get_data(im),w,h,cairo_image_surface_get_stride(im),"PE1ITR",s,&result,error);
        if (f && fclose(f)!=0) ok=0;
        if (!ok && f) unlink(argv[2]);
        cairo_surface_destroy(im);
        if (!ok) { g_printerr("%s\n",error); return 1; }
        g_print("TS: %d bit/s, QP %d, %d beelden, grootste IDR %d bytes\n",s.bitrate,result.qp,result.frames,result.largest_idr);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--smoke-test")) return smoke_test(argv[2]);
    if (argc != 1) { g_printerr("Gebruik: %s [--smoke-test uitvoermap | --ts-test bestand.ts [bitrate [duur [fps [gop [breedte [hoogte]]]]]]]\n", argv[0]); return 1; }
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
    load_config(&app);
    gtk_main();
    pm5544_cleanup();
    g_free(app.directory);
    return 0;
}
