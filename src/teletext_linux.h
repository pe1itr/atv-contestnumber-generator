/* Native GTK view over the shared Level 1 editor. */
typedef struct {
    TeletextSettings book;
    TeletextEditor editor, undo;
    int selected, updating;
    GtkWidget *area, *color, *header, *pages, *status, *remove;
} TeletextDialog;
static void tt_linux_rect(void *ctx,int x,int y,int w,int h,unsigned rgb) {
    cairo_t *cr=ctx; cairo_set_source_rgb(cr,(rgb>>16)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);
    cairo_rectangle(cr,x,y,w,h); cairo_fill(cr);
}
static void tt_linux_glyph(void *ctx,int x,int y,int w,int h,char c,unsigned rgb) {
    cairo_t *cr=ctx; char text[2]={c,0};
    cairo_set_source_rgb(cr,(rgb>>16)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);
    cairo_select_font_face(cr,"monospace",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr,h*.78); cairo_text_extents_t ext; cairo_text_extents(cr,text,&ext);
    cairo_move_to(cr,x+(w-ext.width)/2-ext.x_bearing,y+(h-ext.height)/2-ext.y_bearing); cairo_show_text(cr,text);
}
static gboolean tt_linux_draw(GtkWidget *w,cairo_t *cr,gpointer data) {
    TeletextDialog *d=data;
    teletext_editor_plot(&d->editor,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w),tt_linux_rect,tt_linux_glyph,cr);
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
    teletext_editor_click(&d->editor,(int)ev->x,(int)ev->y,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w),ev->button==3);
    if (memcmp(before.raw,d->editor.raw,sizeof(before.raw))) d->undo=before;
    gtk_widget_grab_focus(w); tt_linux_refresh(d,NULL); return TRUE;
}
static gboolean tt_linux_key(GtkWidget *w,GdkEventKey *ev,gpointer data) {
    (void)w; TeletextDialog *d=data; int key=-1; const char *error=NULL;
    if ((ev->state&GDK_CONTROL_MASK) && (ev->keyval==GDK_KEY_z || ev->keyval==GDK_KEY_Z)) {
        TeletextEditor swap=d->editor; d->editor=d->undo; d->undo=swap; tt_linux_refresh(d,NULL); return TRUE;
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
    teletext_editor_load(&d->editor,&page); d->undo=d->editor;
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
    TeletextEditor before=d->editor; const char *error=NULL;
    if (action==29) error=teletext_editor_background(&d->editor,color);
    else if (action==102) teletext_editor_example(&d->editor);
    else if (action==103) { memset(d->editor.raw,' ',sizeof(d->editor.raw)); d->editor.cursor=0; }
    else if (action==104) d->editor.grid=!d->editor.grid;
    else teletext_editor_control(&d->editor,action<2?(unsigned)(color+action*16):(unsigned)action);
    if (!error && action!=104) d->undo=before;
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
static void edit_teletext(GtkWidget *widget,gpointer data) {
    (void)widget; App *app=data; TeletextDialog d={0}; d.book=app->teletext;
    GtkWidget *dialog=gtk_dialog_new_with_buttons("Teletext pages",GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,"_Cancel",GTK_RESPONSE_CANCEL,"_Save",GTK_RESPONSE_ACCEPT,NULL);
    GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(dialog)); gtk_container_set_border_width(GTK_CONTAINER(box),10);
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    d.pages=gtk_combo_box_text_new(); gtk_box_pack_start(GTK_BOX(row),d.pages,FALSE,FALSE,0);
    tt_linux_button(&d,row,"Add page",100); d.remove=tt_linux_button(&d,row,"Delete page",101);
    tt_linux_button(&d,row,"Antenna example",102); tt_linux_button(&d,row,"Clear page",103);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Header (32 characters):"),FALSE,FALSE,0);
    d.header=gtk_entry_new(); gtk_entry_set_max_length(GTK_ENTRY(d.header),32);
    gtk_entry_set_placeholder_text(GTK_ENTRY(d.header),"ATV TELETEXT 100"); gtk_box_pack_start(GTK_BOX(row),d.header,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("40 x 23 cells; each mosaic cell has 2 x 3 blocks. Click a cell and type, or toggle its blocks.\nInsert Text/Mosaic colour in a blank cell: it applies to the RIGHT, until another colour or end of row.\nBackground: selected colour, 3 empty cells; text colour stays.\nArrows: move. Delete: erase cell. Right-click: erase block. Ctrl+Z: undo. Ctrl+V: paste. T/M/B/C marks are not transmitted."),FALSE,FALSE,4);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,3);
    d.color=gtk_combo_box_text_new();
    for (int i=0;i<7;++i) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(d.color),teletext_color_names[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d.color),6); gtk_box_pack_start(GTK_BOX(row),d.color,FALSE,FALSE,0);
    tt_linux_button(&d,row,"Text colour",0); tt_linux_button(&d,row,"Mosaic colour",1);
    tt_linux_button(&d,row,"Colour background",29); tt_linux_button(&d,row,"Black background",28);
    tt_linux_button(&d,row,"Grid / preview",104);
    d.area=gtk_drawing_area_new(); gtk_widget_set_size_request(d.area,720,483); gtk_widget_set_can_focus(d.area,TRUE);
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
    tt_linux_pages(&d); gtk_widget_show_all(dialog);
    while (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
        const char *error=tt_linux_store(&d); if (error) { tt_linux_refresh(&d,error); continue; }
        TeletextSettings previous=app->teletext; app->teletext=d.book;
        AppConfig settings=capture_config(app); char *path=g_build_filename(app->directory,CONFIG_FILENAME,NULL);
        gboolean saved=config_save(path,&settings); g_free(path);
        if (!saved) { app->teletext=previous; tt_linux_refresh(&d,"Save failed. Check write permissions in the application directory."); continue; }
        gtk_label_set_text(GTK_LABEL(app->status),"Teletext pages saved. Restart UDP to transmit changes."); break;
    }
    gtk_widget_destroy(dialog);
}
