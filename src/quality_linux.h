/* Local before/after comparison, at equal pixel scale with linked scrolling. */
typedef struct {
    GtkWidget *dialog, *status, *areas[2], *timeline_area;
    DatvTimeline timeline;
    cairo_surface_t *images[2];
    DatvStream *job;
    Resolution size;
    DatvSettings settings;
    int zoom, done;
} QualityDialog;
static void quality_rect(void *context,int x,int y,int w,int h,unsigned rgb) {
    cairo_t *cr=context;
    cairo_set_source_rgb(cr,((rgb>>16)&255)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);
    cairo_rectangle(cr,x,y,w,h); cairo_fill(cr);
}
static void quality_text(void *context,int x,int y,const char *text) {
    cairo_t *cr=context;
    PangoLayout *layout=pango_cairo_create_layout(cr);
    PangoFontDescription *font=pango_font_description_from_string("Sans 9");
    pango_layout_set_font_description(layout,font); pango_font_description_free(font);
    pango_layout_set_text(layout,text,-1);
    cairo_set_source_rgb(cr,0.1,0.1,0.1); cairo_move_to(cr,x,y);
    pango_cairo_show_layout(cr,layout); g_object_unref(layout);
}
static gboolean quality_timeline_draw(GtkWidget *widget,cairo_t *cr,gpointer data) {
    QualityDialog *d=data;
    datv_timeline_plot(&d->timeline,d->settings,gtk_widget_get_allocated_width(widget),
        gtk_widget_get_allocated_height(widget),quality_rect,quality_text,cr);
    return TRUE;
}
static gboolean quality_draw(GtkWidget *widget,cairo_t *cr,gpointer data) {
    QualityDialog *d=data;
    int index=widget==d->areas[1];
    cairo_set_source_rgb(cr,0.2,0.2,0.2); cairo_paint(cr);
    if (d->images[index]) {
        cairo_scale(cr,d->zoom,d->zoom);
        cairo_set_source_surface(cr,d->images[index],0,0);
        cairo_pattern_set_filter(cairo_get_source(cr),CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
    }
    return TRUE;
}
static void quality_zoom(GtkToggleButton *button,gpointer data) {
    QualityDialog *d=data;
    d->zoom=gtk_toggle_button_get_active(button)?2:1;
    for (int i=0;i<2;++i) {
        gtk_widget_set_size_request(d->areas[i],d->size.width*d->zoom,d->size.height*d->zoom);
        gtk_widget_queue_draw(d->areas[i]);
    }
}
static void quality_scroll(GtkAdjustment *source,gpointer target) {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(target),gtk_adjustment_get_value(source));
}
static gboolean quality_poll(gpointer data) {
    QualityDialog *d=data; DatvUdpStatus status;
    if (d->done) return G_SOURCE_CONTINUE;
    datv_udp_status(d->job,&status);
    if (status.state==DATV_PREPARING) return G_SOURCE_CONTINUE;
    d->done=1;
    int traced=datv_preview_timeline(d->job,&d->timeline);
    if (traced) gtk_widget_queue_draw(d->timeline_area);
    if (status.state==DATV_FAILED) {
        char text[1800], detail[1300];
        if (traced) { datv_timeline_text(&d->timeline,d->settings,detail,sizeof(detail));
            g_snprintf(text,sizeof(text),"%s\n%s",status.error,detail); }
        else g_snprintf(text,sizeof(text),"%s",status.error);
        gtk_label_set_text(GTK_LABEL(d->status),text);
    }
    else {
        cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_RGB24,d->size.width,d->size.height);
        cairo_surface_flush(image);
        if (cairo_surface_status(image)==CAIRO_STATUS_SUCCESS &&
            datv_preview_image(d->job,(uint32_t *)cairo_image_surface_get_data(image),d->size.width,d->size.height,cairo_image_surface_get_stride(image))) {
            cairo_surface_mark_dirty(image); d->images[1]=image;
            char text[1300]; datv_timeline_text(&d->timeline,d->settings,text,sizeof(text));
            gtk_label_set_text(GTK_LABEL(d->status),text);
            gtk_widget_queue_draw(d->areas[1]);
        } else {
            cairo_surface_destroy(image);
            gtk_label_set_text(GTK_LABEL(d->status),"Kan het gecomprimeerde voorbeeld niet weergeven.");
        }
    }
    return G_SOURCE_CONTINUE;
}
static void quality_compare(GtkWindow *parent,cairo_surface_t *original,Resolution size,const char *call,DatvSettings settings) {
    QualityDialog d={0}; d.size=size; d.settings=settings; d.zoom=size.width<=240?2:1;
    d.images[0]=original;
    char error[256];
    d.job=datv_preview_start((uint32_t *)cairo_image_surface_get_data(original),size.width,size.height,
        cairo_image_surface_get_stride(original),call,settings,error);
    d.dialog=gtk_dialog_new_with_buttons("DATV: Beeld controleren",parent,GTK_DIALOG_MODAL,"_Sluiten",GTK_RESPONSE_CLOSE,NULL);
    gtk_window_set_default_size(GTK_WINDOW(d.dialog),1100,820);
    GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(d.dialog));
    GtkWidget *grid=gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(grid),12);
    gtk_grid_set_row_spacing(GTK_GRID(grid),8); gtk_container_set_border_width(GTK_CONTAINER(grid),12);
    gtk_box_pack_start(GTK_BOX(box),grid,TRUE,TRUE,0);
    GtkWidget *scrolls[2];
    for (int i=0;i<2;++i) {
        gtk_grid_attach(GTK_GRID(grid),gtk_label_new(i?"Na compressie":"Origineel"),i,0,1,1);
        scrolls[i]=gtk_scrolled_window_new(NULL,NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolls[i]),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
        gtk_widget_set_hexpand(scrolls[i],TRUE); gtk_widget_set_vexpand(scrolls[i],TRUE);
        gtk_widget_set_size_request(scrolls[i],240,200);
        d.areas[i]=gtk_drawing_area_new();
        gtk_widget_set_halign(d.areas[i],GTK_ALIGN_START); gtk_widget_set_valign(d.areas[i],GTK_ALIGN_START);
        gtk_widget_set_size_request(d.areas[i],size.width*d.zoom,size.height*d.zoom);
        g_signal_connect(d.areas[i],"draw",G_CALLBACK(quality_draw),&d);
        gtk_container_add(GTK_CONTAINER(scrolls[i]),d.areas[i]);
        gtk_grid_attach(GTK_GRID(grid),scrolls[i],i,1,1,1);
    }
    for (int i=0;i<2;++i) {
        GtkAdjustment *a=gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolls[i]));
        GtkAdjustment *b=gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolls[1-i]));
        g_signal_connect(a,"value-changed",G_CALLBACK(quality_scroll),b);
        a=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolls[i]));
        b=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolls[1-i]));
        g_signal_connect(a,"value-changed",G_CALLBACK(quality_scroll),b);
    }
    GtkWidget *zoom=gtk_check_button_new_with_label("2× vergroten (beide beelden)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(zoom),d.zoom==2);
    g_signal_connect(zoom,"toggled",G_CALLBACK(quality_zoom),&d);
    gtk_grid_attach(GTK_GRID(grid),zoom,0,2,2,1);
    d.status=gtk_label_new(d.job?"Beeld voorbereiden en decoderen… Er wordt niets uitgezonden.":error);
    gtk_label_set_line_wrap(GTK_LABEL(d.status),TRUE);
    gtk_grid_attach(GTK_GRID(grid),d.status,0,3,2,1);
    d.timeline_area=gtk_drawing_area_new();
    gtk_widget_set_size_request(d.timeline_area,900,194);
    g_signal_connect(d.timeline_area,"draw",G_CALLBACK(quality_timeline_draw),&d);
    gtk_grid_attach(GTK_GRID(grid),d.timeline_area,0,4,2,1);
    GtkWidget *note=gtk_label_new("Eerste volledige videobeeld na H.264-compressie. Andere videobeelden kunnen verschillen.\nZonder ontvangstverliezen of beeldbewerking van je ontvanger. Scroll om details te bekijken.");
    gtk_label_set_line_wrap(GTK_LABEL(note),TRUE); gtk_grid_attach(GTK_GRID(grid),note,0,5,2,1);
    gtk_widget_show_all(d.dialog);
    guint timer=d.job?g_timeout_add(100,quality_poll,&d):0;
    gtk_dialog_run(GTK_DIALOG(d.dialog));
    if (timer) g_source_remove(timer);
    datv_udp_destroy(d.job);
    gtk_widget_destroy(d.dialog);
    if (d.images[1]) cairo_surface_destroy(d.images[1]);
}
