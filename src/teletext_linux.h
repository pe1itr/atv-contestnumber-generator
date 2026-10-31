/* Native GTK view over the shared Level 1 editor. */
typedef struct {
    TeletextSettings book;
    TeletextEditor editor, undo;
    int selected, updating;
    GtkWidget *dialog, *area, *color, *header, *pages, *status, *remove, *attribute;
} TeletextDialog;
static void tt_linux_rect(void *ctx,int x,int y,int w,int h,unsigned rgb) {
    cairo_t *cr=ctx; cairo_set_source_rgb(cr,(rgb>>16)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);
    cairo_rectangle(cr,x,y,w,h); cairo_fill(cr);
}
static gboolean tt_linux_draw(GtkWidget *w,cairo_t *cr,gpointer data) {
    TeletextDialog *d=data;
    teletext_view_plot(&d->editor,gtk_entry_get_text(GTK_ENTRY(d->header)),100+d->selected,
        gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w),tt_linux_rect,cr);
    return TRUE;
}
static void tt_linux_refresh(TeletextDialog *d,const char *error) {
    char text[240];
    snprintf(text,sizeof(text),"Page %d | Row %d, column %d | %d pages: each repeats every %d seconds.",
        100+d->selected,d->editor.cursor/40+1,d->editor.cursor%40+1,
        teletext_page_count(&d->book),teletext_interval(&d->book)*teletext_page_count(&d->book));
    gtk_label_set_text(GTK_LABEL(d->status),error?error:text);
    gtk_widget_queue_draw(d->area);
}
static gboolean tt_linux_click(GtkWidget *w,GdkEventButton *ev,gpointer data) {
    TeletextDialog *d=data;
    if ((ev->button!=1 && ev->button!=3) || !d->editor.grid) return FALSE;
    TeletextEditor before=d->editor;
    teletext_view_click(&d->editor,(int)ev->x,(int)ev->y,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w),ev->button==3);
    if (memcmp(before.raw,d->editor.raw,sizeof(before.raw))) d->undo=before;
    gtk_widget_grab_focus(w); tt_linux_refresh(d,NULL); return TRUE;
}
static gboolean tt_linux_key(GtkWidget *w,GdkEventKey *ev,gpointer data) {
    (void)w; TeletextDialog *d=data; int key=-1; const char *error=NULL;
    if ((ev->state&GDK_CONTROL_MASK) && (ev->keyval==GDK_KEY_z || ev->keyval==GDK_KEY_Z)) {
        teletext_editor_undo(&d->editor,&d->undo); tt_linux_refresh(d,NULL); return TRUE;
    }
    if (d->editor.grid && (ev->state&GDK_CONTROL_MASK) && (ev->keyval==GDK_KEY_v || ev->keyval==GDK_KEY_V)) {
        char *text=gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
        if (text) { d->undo=d->editor; error=teletext_editor_paste(&d->editor,text); g_free(text); }
        tt_linux_refresh(d,error); return TRUE;
    }
    if (!d->editor.grid) return FALSE;
    switch (ev->keyval) {
    case GDK_KEY_Left: key=TT_LEFT; break; case GDK_KEY_Right: key=TT_RIGHT; break;
    case GDK_KEY_Up: key=TT_UP; break; case GDK_KEY_Down: key=TT_DOWN; break;
    case GDK_KEY_Home: key=TT_HOME; break; case GDK_KEY_End: key=TT_END; break;
    case GDK_KEY_Return: key=TT_ENTER; break; case GDK_KEY_BackSpace: key=TT_BACKSPACE; break;
    case GDK_KEY_Delete: key=TT_DELETE; break;
    }
    if (key>=0) { if (key==TT_DELETE || key==TT_BACKSPACE) d->undo=d->editor; teletext_editor_key(&d->editor,key); }
    else if (!(ev->state&(GDK_CONTROL_MASK|GDK_MOD1_MASK))) {
        gunichar c=gdk_keyval_to_unicode(ev->keyval);
        if (!c) return FALSE;
        d->undo=d->editor; error=teletext_editor_type(&d->editor,c);
    } else return FALSE;
    tt_linux_refresh(d,error); return TRUE;
}
static const char *tt_linux_store(TeletextDialog *d) {
    TeletextSettings page={0}; page.enabled=d->book.enabled;
    const char *header=gtk_entry_get_text(GTK_ENTRY(d->header));
    if (strlen(header)>32) return "Use up to 32 unaccented header characters.";
    strcpy(page.header,header); teletext_editor_save(&d->editor,&page);
    const char *error=teletext_validate(&page);
    if (!error) teletext_page_set(&d->book,d->selected,&page);
    return error;
}
static void tt_linux_load(TeletextDialog *d) {
    TeletextSettings page; teletext_page_get(&d->book,d->selected,&page);
    int reveal=d->editor.reveal,off=d->editor.flash_off;
    teletext_editor_load(&d->editor,&page); d->editor.reveal=reveal; d->editor.flash_off=off; d->undo=d->editor;
    gtk_entry_set_text(GTK_ENTRY(d->header),page.header);
    char fallback[33]; snprintf(fallback,sizeof(fallback),"ATV TELETEXT %d",100+d->selected);
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->header),fallback);
    gtk_widget_set_sensitive(d->remove,d->selected>0); tt_linux_refresh(d,NULL);
}
static void tt_linux_pages(TeletextDialog *d) {
    d->updating=1; gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(d->pages));
    for (int i=0;i<teletext_page_count(&d->book);++i) {
        char text[20]; snprintf(text,sizeof(text),"Page %d",100+i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d->pages),text);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->pages),d->selected); d->updating=0;
    tt_linux_load(d);
}
static void tt_linux_select(GtkComboBox *w,gpointer data) {
    TeletextDialog *d=data; if (d->updating) return;
    int selected=gtk_combo_box_get_active(w); if (selected<0) return;
    const char *error=tt_linux_store(d);
    if (error) {
        d->updating=1; gtk_combo_box_set_active(w,d->selected); d->updating=0; tt_linux_refresh(d,error); return;
    }
    d->selected=selected; tt_linux_load(d);
}
static int tt_linux_import_path(TeletextDialog *d,const char *path) {
    TeletextSettings page; teletext_page_get(&d->book,d->selected,&page);
    char error[256];
    if (!teletext_import_file(&page,path,error,sizeof(error))) { tt_linux_refresh(d,error); return 0; }
    TeletextEditor before=d->editor;
    int grid=d->editor.grid,reveal=d->editor.reveal,off=d->editor.flash_off;
    teletext_editor_load(&d->editor,&page);
    d->editor.grid=grid; d->editor.reveal=reveal; d->editor.flash_off=off; d->undo=before;
    tt_linux_refresh(d,"Page loaded into the selected page. Header and interval kept. Ctrl+Z: undo. Save to keep it.");
    return 1;
}
static gboolean tt_linux_page_filter(const GtkFileFilterInfo *info,gpointer unused) {
    (void)unused; const char *ext=strrchr(info->display_name,'.');
    return ext && (!g_ascii_strcasecmp(ext,".tti") || !g_ascii_strcasecmp(ext,".txt"));
}
static void tt_linux_import(TeletextDialog *d) {
    GtkWidget *picker=gtk_file_chooser_dialog_new("Load teletext page",GTK_WINDOW(d->dialog),
        GTK_FILE_CHOOSER_ACTION_OPEN,"_Cancel",GTK_RESPONSE_CANCEL,"_Open",GTK_RESPONSE_ACCEPT,NULL);
    GtkFileFilter *filter=gtk_file_filter_new(); gtk_file_filter_set_name(filter,"Teletext pages (*.tti, *.txt)");
    gtk_file_filter_add_custom(filter,GTK_FILE_FILTER_DISPLAY_NAME,tt_linux_page_filter,NULL,NULL);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(picker),filter);
    if (gtk_dialog_run(GTK_DIALOG(picker))==GTK_RESPONSE_ACCEPT) {
        char *path=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(picker));
        if (path) { tt_linux_import_path(d,path); g_free(path); }
    }
    gtk_widget_destroy(picker); gtk_widget_grab_focus(d->area);
}
static void tt_linux_action(GtkWidget *w,gpointer data) {
    TeletextDialog *d=data; int action=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"action"));
    int color=gtk_combo_box_get_active(GTK_COMBO_BOX(d->color))+1;
    if (action==100 || action==101) {
        const char *error=tt_linux_store(d); if (error) { tt_linux_refresh(d,error); return; }
        if (action==100) {
            if (!teletext_page_add(&d->book)) { tt_linux_refresh(d,"Up to four pages (100-103)."); return; }
            d->selected=d->book.extra_count;
        } else { teletext_page_delete(&d->book,d->selected); d->selected=0; }
        tt_linux_pages(d); return;
    }
    if (action==102) { tt_linux_import(d); return; }
    TeletextEditor before=d->editor; const char *error=NULL;
    if (action==29) error=teletext_editor_background(&d->editor,color);
    else if (action==103) { memset(d->editor.raw,' ',sizeof(d->editor.raw)); d->editor.cursor=0; }
    else if (action==104) d->editor.grid=!d->editor.grid;
    else if (action==105) {
        int selected=gtk_combo_box_get_active(GTK_COMBO_BOX(d->attribute));
        if (selected>=0 && selected<TELETEXT_ATTRIBUTE_COUNT)
            error=teletext_editor_control(&d->editor,teletext_attributes[selected].code);
    } else if (action==106) d->editor.reveal=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
    else error=teletext_editor_control(&d->editor,action<2?(unsigned)(color+action*16):(unsigned)action);
    if (!error && action!=104 && action!=106) d->undo=before;
    tt_linux_refresh(d,error?error:action==29?"Selected background applied. Three control cells inserted; continue at the cursor. Colour extends to the right.":NULL);
    gtk_widget_grab_focus(d->area);
}
static GtkWidget *tt_linux_button(TeletextDialog *d,GtkWidget *box,const char *label,int action) {
    GtkWidget *button=gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);
    g_object_set_data(G_OBJECT(button),"action",GINT_TO_POINTER(action));
    g_signal_connect(button,"clicked",G_CALLBACK(tt_linux_action),d); return button;
}
static void tt_linux_interval(GtkComboBox *w,gpointer data) {
    TeletextDialog *d=data; const int values[]={2,5,10}; int selected=gtk_combo_box_get_active(w);
    if (selected>=0 && selected<3) d->book.interval=values[selected];
    tt_linux_refresh(d,NULL);
}
static gboolean tt_linux_tick(gpointer data) {
    TeletextDialog *d=data;
    int off=(g_get_monotonic_time()/1000)%1000>=750;
    if (off!=d->editor.flash_off) { d->editor.flash_off=off; gtk_widget_queue_draw(d->area); }
    return G_SOURCE_CONTINUE;
}
static void tt_linux_header_changed(GtkEditable *entry,gpointer data) {
    (void)entry; TeletextDialog *d=data; gtk_widget_queue_draw(d->area);
}
static void edit_teletext(GtkWidget *widget,gpointer data) {
    (void)widget; App *app=data; TeletextDialog d={0}; d.book=app->teletext;
    GtkWidget *dialog=gtk_dialog_new_with_buttons("Teletext pages",GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,"_Cancel",GTK_RESPONSE_CANCEL,"_Save",GTK_RESPONSE_ACCEPT,NULL);
    d.dialog=dialog;
    GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(dialog)); gtk_container_set_border_width(GTK_CONTAINER(box),10);
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    d.pages=gtk_combo_box_text_new(); gtk_box_pack_start(GTK_BOX(row),d.pages,FALSE,FALSE,0);
    tt_linux_button(&d,row,"Add page",100); d.remove=tt_linux_button(&d,row,"Delete page",101);
    tt_linux_button(&d,row,"Load page",102); tt_linux_button(&d,row,"Clear page",103);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Header (32 characters):"),FALSE,FALSE,0);
    d.header=gtk_entry_new(); gtk_entry_set_max_length(GTK_ENTRY(d.header),32);
    gtk_entry_set_placeholder_text(GTK_ENTRY(d.header),"ATV TELETEXT 100"); gtk_box_pack_start(GTK_BOX(row),d.header,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("40 x 23 cells; each mosaic cell has 2 x 3 blocks. Click a cell and type, or toggle its blocks.\nInsert Text/Mosaic colour in a blank cell: it applies to the RIGHT, until another colour or end of row.\nBackground: selected colour, 3 empty cells; text colour stays.\nArrows: move. Delete: erase cell. Right-click: erase block. Ctrl+Z: undo. Ctrl+V: paste. Attribute marks are editor guides; Reveal shows concealed text."),FALSE,FALSE,4);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    d.color=gtk_combo_box_text_new();
    for (int i=0;i<7;++i) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d.color),teletext_color_names[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d.color),6); gtk_box_pack_start(GTK_BOX(row),d.color,FALSE,FALSE,0);
    tt_linux_button(&d,row,"Text colour",0); tt_linux_button(&d,row,"Mosaic colour",1);
    tt_linux_button(&d,row,"Colour background",29); tt_linux_button(&d,row,"Black background",28);
    tt_linux_button(&d,row,"Grid / preview",104);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    d.attribute=gtk_combo_box_text_new();
    for (int i=0;i<TELETEXT_ATTRIBUTE_COUNT;++i)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d.attribute),teletext_attributes[i].name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d.attribute),0); gtk_box_pack_start(GTK_BOX(row),d.attribute,FALSE,FALSE,0);
    tt_linux_button(&d,row,"Insert attribute",105);
    GtkWidget *reveal=gtk_check_button_new_with_label("Reveal");
    gtk_box_pack_start(GTK_BOX(row),reveal,FALSE,FALSE,0);
    g_object_set_data(G_OBJECT(reveal),"action",GINT_TO_POINTER(106));
    g_signal_connect(reveal,"toggled",G_CALLBACK(tt_linux_action),&d);
    d.area=gtk_drawing_area_new(); gtk_widget_set_size_request(d.area,720,504); gtk_widget_set_can_focus(d.area,TRUE);
    gtk_widget_add_events(d.area,GDK_BUTTON_PRESS_MASK|GDK_KEY_PRESS_MASK);
    gtk_box_pack_start(GTK_BOX(box),d.area,TRUE,TRUE,4);
    d.status=gtk_label_new(""); gtk_label_set_line_wrap(GTK_LABEL(d.status),TRUE); gtk_box_pack_start(GTK_BOX(box),d.status,FALSE,FALSE,3);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Next page every:"),FALSE,FALSE,0);
    GtkWidget *interval=gtk_combo_box_text_new();
    const char *intervals[]={"2 seconds","5 seconds","10 seconds"};
    for (int i=0;i<3;++i) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval),intervals[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(interval),teletext_interval(&d.book)==10?2:teletext_interval(&d.book)==5?1:0);
    gtk_box_pack_start(GTK_BOX(row),interval,FALSE,FALSE,0);
    g_signal_connect(interval,"changed",G_CALLBACK(tt_linux_interval),&d);
    g_signal_connect(d.area,"draw",G_CALLBACK(tt_linux_draw),&d);
    g_signal_connect(d.area,"button-press-event",G_CALLBACK(tt_linux_click),&d);
    g_signal_connect(d.area,"key-press-event",G_CALLBACK(tt_linux_key),&d);
    g_signal_connect(d.pages,"changed",G_CALLBACK(tt_linux_select),&d);
    g_object_set_data(G_OBJECT(dialog),"teletext-editor",&d);
    g_signal_connect(d.header,"changed",G_CALLBACK(tt_linux_header_changed),&d);
    tt_linux_pages(&d); gtk_widget_show_all(dialog);
    guint timer=g_timeout_add(50,tt_linux_tick,&d);
    while (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
        const char *error=tt_linux_store(&d); if (error) { tt_linux_refresh(&d,error); continue; }
        TeletextSettings previous=app->teletext; app->teletext=d.book;
        AppConfig settings=capture_config(app); char *path=g_build_filename(app->directory,CONFIG_FILENAME,NULL);
        gboolean saved=config_save(path,&settings); g_free(path);
        if (!saved) { app->teletext=previous; tt_linux_refresh(&d,"Save failed. Check write permissions in the application directory."); continue; }
        gtk_label_set_text(GTK_LABEL(app->status),"Teletext pages saved. Restart UDP to transmit changes."); break;
    }
    g_source_remove(timer); gtk_widget_destroy(dialog);
}
