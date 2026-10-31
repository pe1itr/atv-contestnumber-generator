#define main camera_application_main
#include "../src/main.cpp"
#undef main
#include <cassert>
static int stage=0,ticks=0;
static gboolean exercise(gpointer data){
    auto u=static_cast<UI*>(data);assert(++ticks<250);
    if(stage==0){
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["input"]),"2");
        gtk_entry_set_text(GTK_ENTRY(u->widgets["callsign"]),"PE1ITR/P");
        for(int i=0;i<3;++i){
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["preset"]),std::to_string(i).c_str());
            auto s=u->read();assert(s.fps==2+2*i);assert(s.gop==5*s.fps);validate(s);
        }
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["system"]),"2");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["bw"]),"35");
        assert(u->choice("fec")!=0); // Below transport floor is filtered out.
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["bw"]),"40");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["fec"]),"0");
        assert(dvb_bitrate(u->read().dvb)==30160);
        u->start(0);assert(u->engine);stage=1;
    }else if(stage==1){
        auto status=u->engine->status();assert(!status.failed);
        if(u->serial&&status.datagrams>8){
            auto win=gtk_widget_get_window(u->window);auto pix=gdk_pixbuf_get_from_window(win,0,0,gdk_window_get_width(win),gdk_window_get_height(win));
            assert(pix);assert(gdk_pixbuf_save(pix,"build/ui-preview.png","png",nullptr,nullptr));g_object_unref(pix);
            u->engine->stop();stage=2;
        }
    }else if(stage==2&&!u->engine){
        auto saved=load_settings(u->path);assert(saved.callsign=="PE1ITR/P"&&saved.width==160&&saved.fps==6);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(u->widgets["aspect"]),"169");
        u->start(0);assert(u->engine);stage=3;
    }else if(stage==3){
        assert(!u->engine->status().failed);
        if(u->serial&&u->engine->status().datagrams>3){assert(u->engine->picture().height==90);gtk_widget_destroy(u->window);return G_SOURCE_REMOVE;}
    }
    return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv){
    gst_init(&argc,&argv);gtk_init(&argc,&argv);UI u;u.path="build/ui-test.ini";
    g_timeout_add(100,exercise,&u);u.run();assert(stage==3);std::cout<<"Camera GTK presets, FEC filtering, local preview, callsign settings, stop/restart and 16:9 OK\n";
}
