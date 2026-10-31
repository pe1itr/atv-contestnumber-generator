#define main streambridge_main
#include "../src/main.cpp"
#undef main
#include <cassert>
int main(){
    gst_init(nullptr,nullptr);assert(gtk_init_check(nullptr,nullptr));
    UI ui;ui.path="/tmp/unused-streambridge-ui.ini";
    g_timeout_add(300,+[](gpointer data)->gboolean{
        auto u=static_cast<UI*>(data);
        assert(u->text("uri")=="srt://:4001?mode=listener");
        assert(audio_budget(u->read())==96);
        assert(!u->widgets.count("height"));
        assert(u->read().height()==240);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("aspect")),"169");
        assert(u->read().height()==180);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->widgets.at("width")),720);
        assert(u->read().height()==406);
        validate(u->read());
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("aspect")),"43");
        assert(u->read().height()==540);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->widgets.at("width")),1920);
        assert(u->read().width==1920 && u->read().height()==1440);validate(u->read());
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("aspect")),"169");
        assert(u->read().height()==1080);validate(u->read());
        auto invalid_size=u->read();invalid_size.width=1922;
        bool too_wide=false;try{validate(invalid_size);}catch(...){too_wide=true;}assert(too_wide);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->widgets.at("width")),320);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("aspect")),"43");
        gtk_entry_set_text(GTK_ENTRY(u->widgets.at("callsign")),"PE1TEST");assert(u->read().callsign=="PE1TEST");
        u->addresses={{"wifi-test","192.168.4.2"}};u->budget();
        std::string label=gtk_label_get_text(GTK_LABEL(u->network_label));
        assert(label.find("srt://192.168.4.2:4001?mode=caller")!=std::string::npos);
        gtk_entry_set_text(GTK_ENTRY(u->widgets.at("uri")),"srt://:5001?mode=listener");
        label=gtk_label_get_text(GTK_LABEL(u->network_label));assert(label.find(":5001?mode=caller")!=std::string::npos);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("input")),"1");
        assert(u->read().input==1);validate(u->read());
        label=gtk_label_get_text(GTK_LABEL(u->network_label));assert(label.find("rtsp://192.168.4.2:8554/live")!=std::string::npos);
        assert(!gtk_widget_get_sensitive(u->widgets.at("uri"))&&gtk_widget_get_sensitive(u->widgets.at("rtsp")));
        gtk_entry_set_text(GTK_ENTRY(u->widgets.at("rtsp")),"rtsp://0.0.0.0:8555/camera");
        label=gtk_label_get_text(GTK_LABEL(u->network_label));assert(label.find("rtsp://192.168.4.2:8555/camera")!=std::string::npos);
        gchar *tmp=nullptr;int fd=g_file_open_tmp("bridge-rtsp-XXXXXX.ini",&tmp,nullptr);assert(fd>=0);close(fd);
        save_settings(u->read(),tmp);auto restored=load_settings(tmp);assert(restored.input==1&&restored.rtsp_uri==u->text("rtsp")&&restored.uri==u->text("uri"));unlink(tmp);g_free(tmp);
        for(const char *bad:{"rtsp://remote.example:8554/live","rtsp://0.0.0.0:0/live","rtsp://0.0.0.0:8554/","rtsp://0.0.0.0:8554/live?x=1"}){
            auto s=u->read();s.rtsp_uri=bad;bool invalid=false;try{validate(s);}catch(...){invalid=true;}assert(invalid);
        }
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets.at("input")),"0");
        assert(u->text("uri")=="srt://:5001?mode=listener");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->widgets.at("audio")),32);
        bool invalid=false;try{validate(u->read());}catch(...){invalid=true;}assert(invalid);
        gtk_main_quit();return G_SOURCE_REMOVE;
    },&ui);
    ui.run();std::cout<<"GTK fields, dynamic telephone port and minimum audio bitrate OK\n";
}
