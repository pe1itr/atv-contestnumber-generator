#define main atv_application_main
#include "../src/linux.c"
#undef main
#include <assert.h>
static int stage, qp, test_fps;
static gint64 deadline;
static gboolean drive(gpointer unused) {
    (void)unused; assert(g_get_monotonic_time()<deadline);
    GList *windows=gtk_window_list_toplevels();
    for (GList *p=windows;p;p=p->next) {
        GtkWidget *w=p->data;
        if (!gtk_widget_get_visible(w)) continue;
        const char *title=gtk_window_get_title(GTK_WINDOW(w));
        if (g_strcmp0(title,"DATV: UDP output") && g_strcmp0(title,"DATV: Check image")) continue;
        GList *children=gtk_container_get_children(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(w))));
        GtkGrid *grid=GTK_GRID(children->data); g_list_free(children);
        if (!strcmp(title,"DATV: UDP output")) {
            if (stage==0) {
                gtk_entry_set_text(GTK_ENTRY(gtk_grid_get_child_at(grid,1,0)),"invalid");
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,2)),115196);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,3)),test_fps);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,4)),2);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_grid_get_child_at(grid,1,10)),test_fps==4?3000:1000);
                stage=1; gtk_dialog_response(GTK_DIALOG(w),4);
            } else if (stage==2) { stage=3; gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_CLOSE); }
        } else {
            const char *text=gtk_label_get_text(GTK_LABEL(gtk_grid_get_child_at(grid,0,3)));
            const char *value=strstr(text,"QP ");
            if (value) {
                qp=atoi(value+3); assert(qp>=24 && qp<=48);
                assert(GTK_IS_DRAWING_AREA(gtk_grid_get_child_at(grid,0,4)));
                assert(strstr(text,test_fps==4?"presentation time: 3000 ms":"presentation time: 1000 ms"));
                assert(strstr(text,"Frame period:") && strstr(text,"without TS overhead"));
                const char *timing=strstr(text,"First complete image: ");
                double ms=0; assert(timing && sscanf(timing,"First complete image: %lf ms",&ms)==1);
                assert(ms>0 && ms<=1000);
                GtkToggleButton *zoom=GTK_TOGGLE_BUTTON(gtk_grid_get_child_at(grid,0,2));
                gtk_toggle_button_set_active(zoom,FALSE); gtk_toggle_button_set_active(zoom,TRUE);
                GtkScrolledWindow *a=GTK_SCROLLED_WINDOW(gtk_grid_get_child_at(grid,0,1));
                GtkScrolledWindow *b=GTK_SCROLLED_WINDOW(gtk_grid_get_child_at(grid,1,1));
                gtk_adjustment_set_value(gtk_scrolled_window_get_hadjustment(a),24);
                assert(gtk_adjustment_get_value(gtk_scrolled_window_get_hadjustment(a))==gtk_adjustment_get_value(gtk_scrolled_window_get_hadjustment(b)));
                const char *snapshot=g_getenv("ATV_QUALITY_SCREENSHOT");
                if (snapshot && test_fps==10) {
                    while (gtk_events_pending()) gtk_main_iteration();
                    GdkPixbuf *pixels=gdk_pixbuf_get_from_window(gtk_widget_get_window(w),0,0,
                        gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w));
                    assert(pixels && gdk_pixbuf_save(pixels,snapshot,"png",NULL,NULL)); g_object_unref(pixels);
                }
                stage=2; gtk_dialog_response(GTK_DIALOG(w),GTK_RESPONSE_CLOSE);
            }
        }
    }
    g_list_free(windows); return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv) {
    gtk_init(&argc,&argv);
    App app={0}; app.directory="/tmp"; create_ui(&app); gtk_widget_show_all(app.window);
    AppConfig wanted=config_defaults(); strcpy(wanted.call,"PE1ITR"); strcpy(wanted.locator,"JO21QK"); strcpy(wanted.code,"1957");
    wanted.automatic=0; wanted.resolution=2; wanted.genius=2; wanted.show_sum=1; wanted.top_code=1;
    apply_config(&app,&wanted); int high=48;
    for (int i=0;i<2;++i) {
        stage=0; qp=0; test_fps=i?4:10; deadline=g_get_monotonic_time()+60000000;
        guint timer=g_timeout_add(50,drive,NULL); output_udp(NULL,&app); g_source_remove(timer);
        assert(stage==3 && qp); if (!i) high=qp; else assert(qp<=high);
        AppConfig actual=capture_config(&app); assert(!memcmp(&actual,&wanted,sizeof(wanted)));
        printf("Linux quality UI: 240px, 115196 bit/s, %d fps, GOP2 -> QP%d; no settings mutation, invalid destination allowed, zoom/scroll OK.\n",test_fps,qp);
    }
    g_signal_handlers_disconnect_by_func(app.window,gtk_main_quit,NULL); gtk_widget_destroy(app.window); pm5544_cleanup();
    return 0;
}
