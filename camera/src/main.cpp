#include "engine.hpp"
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <signal.h>
#include <map>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cstring>
using namespace camera;
static volatile sig_atomic_t interrupted=0;
static void interrupt(int){interrupted=1;}
static std::string utf8(const wchar_t *s){char *p=g_ucs4_to_utf8(reinterpret_cast<const gunichar*>(s),-1,nullptr,nullptr,nullptr);std::string r=p?p:"";g_free(p);return r;}
struct UI {
    Settings settings;std::string path;std::unique_ptr<Engine> engine;
    GtkWidget *window,*grid,*budget_label,*status_label,*preview_image,*preview_button,*send_button,*record_button,*stop_button;
    std::map<std::string,GtkWidget*> widgets;int row=0;bool updating=false,closed=false;uint64_t serial=0;
    void field(const char *label,const char *key,GtkWidget *w){
        widgets[key]=w;auto l=gtk_label_new(label);gtk_label_set_xalign(GTK_LABEL(l),0);gtk_widget_set_hexpand(w,TRUE);
        gtk_grid_attach(GTK_GRID(grid),l,0,row,1,1);gtk_grid_attach(GTK_GRID(grid),w,1,row++,1,1);
    }
    void entry(const char *label,const char *key,const std::string &value){auto w=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(w),value.c_str());field(label,key,w);}
    void spin(const char *label,const char *key,int value,int lo,int hi){auto w=gtk_spin_button_new_with_range(lo,hi,1);gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),value);field(label,key,w);}
    void combo(const char *label,const char *key,const std::vector<std::pair<int,std::string>> &values,int value){
        auto w=gtk_combo_box_text_new();for(const auto &v:values)gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w),std::to_string(v.first).c_str(),v.second.c_str());
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(w),std::to_string(value).c_str());field(label,key,w);
    }
    int number(const char *key){return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.at(key)));}
    int choice(const char *key){const char *v=gtk_combo_box_get_active_id(GTK_COMBO_BOX(widgets.at(key)));return v?std::stoi(v):-1;}
    std::string text(const char *key){return gtk_entry_get_text(GTK_ENTRY(widgets.at(key)));}
    Settings read(){
        Settings s=settings;s.device=gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widgets.at("device")))));
        s.callsign=text("callsign");s.ip=text("ip");s.port=number("port");
        s.test_source=choice("input")!=0;s.pattern=std::max(0,choice("input")-1);
        s.width=number("width");s.aspect=choice("aspect");s.fps=number("fps");s.gop=number("gop");s.video_kbps=number("video");
        s.denoise=choice("denoise");s.monochrome=choice("mono");
        s.dvb={choice("system"),choice("sr"),choice("fec"),choice("pilots"),choice("guard"),choice("bw")};
        s.capture.clear();s.preview_only=false;s.seconds=0;return s;
    }
    void changed(){
        if(updating)return;
        updating=true;Settings s=read();int ids[DVB_FEC_COUNT];int count=dvb_fec_choices(s.dvb,ids),selected=-1;
        auto fec=GTK_COMBO_BOX_TEXT(widgets.at("fec"));gtk_combo_box_text_remove_all(fec);
        for(int i=0;i<count;++i){gtk_combo_box_text_append(fec,std::to_string(ids[i]).c_str(),utf8(dvb_fec_names[ids[i]]).c_str());if(ids[i]==s.dvb.fec)selected=ids[i];}
        if(selected<0&&count)selected=ids[0];
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(fec),std::to_string(selected).c_str());s=read();
        gtk_widget_set_sensitive(widgets["sr"],s.dvb.system!=DVB_T);gtk_widget_set_sensitive(widgets["bw"],s.dvb.system==DVB_T);
        gtk_widget_set_sensitive(widgets["guard"],s.dvb.system==DVB_T);gtk_widget_set_sensitive(widgets["pilots"],s.dvb.system==DVB_S2);
        gtk_widget_set_sensitive(widgets["device"],!s.test_source);
        std::string msg="TS: "+std::to_string(dvb_bitrate(s.dvb))+" bit/s · video: "+std::to_string(video_budget(s))+" kbit/s · geen audio\n"+
            std::to_string(s.width)+" × "+std::to_string(s.height())+" · "+std::to_string(s.fps)+" beelden/s · sleutelbeeld elke "+std::to_string(double(s.gop)/s.fps).substr(0,4)+" s";
        bool valid=true;try{validate(s);}catch(const std::exception &e){msg+="\n"+std::string(e.what());valid=false;}
        gtk_label_set_text(GTK_LABEL(budget_label),msg.c_str());
        for(auto w:{preview_button,send_button,record_button})gtk_widget_set_sensitive(w,valid&&!engine);
        updating=false;
    }
    void apply_preset(){
        if(updating)return;
        int index=choice("preset");if(index<0||index>2)return;
        Settings s=read();preset(s,index);updating=true;
        for(auto pair:{std::make_pair("width",s.width),{"fps",s.fps},{"gop",s.gop},{"video",s.video_kbps}})gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets.at(pair.first)),pair.second);
        updating=false;changed();
    }
    void start(int mode){
        try{
            Settings s=read();validate(s);
            if(mode==2){
                auto dialog=gtk_file_chooser_dialog_new("Lokale MPEG-TS-opname",GTK_WINDOW(window),GTK_FILE_CHOOSER_ACTION_SAVE,"Annuleren",GTK_RESPONSE_CANCEL,"Opnemen",GTK_RESPONSE_ACCEPT,nullptr);
                gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),"camera-proef.ts");
                if(gtk_dialog_run(GTK_DIALOG(dialog))!=GTK_RESPONSE_ACCEPT){gtk_widget_destroy(dialog);return;}
                char *name=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));s.capture=name?name:"";g_free(name);gtk_widget_destroy(dialog);
                if(s.capture.empty())return;
                if(g_file_test(s.capture.c_str(),G_FILE_TEST_EXISTS))throw std::runtime_error("Bestand bestaat al; kies een nieuwe naam.");
            }
            save_settings(s,path);s.preview_only=mode==0;settings=s;serial=0;engine=std::make_unique<Engine>(s);
            gtk_widget_set_sensitive(grid,FALSE);for(auto w:{preview_button,send_button,record_button})gtk_widget_set_sensitive(w,FALSE);
            gtk_widget_set_sensitive(stop_button,TRUE);
        }catch(const std::exception &e){gtk_label_set_text(GTK_LABEL(status_label),e.what());}
    }
    static gboolean tick(gpointer data){
        auto u=static_cast<UI*>(data);if(u->closed||!u->engine)return G_SOURCE_CONTINUE;
        auto s=u->engine->status();auto p=u->engine->picture();
        if(p.serial&&p.serial!=u->serial){
            auto pix=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,p.width,p.height);auto dst=gdk_pixbuf_get_pixels(pix);int stride=gdk_pixbuf_get_rowstride(pix);
            for(int y=0;y<p.height;++y)std::memcpy(dst+y*stride,p.rgb.data()+y*p.width*3,p.width*3);
            int w=480,h=u->settings.aspect==169?270:360;auto scaled=gdk_pixbuf_scale_simple(pix,w,h,GDK_INTERP_NEAREST);
            gtk_image_set_from_pixbuf(GTK_IMAGE(u->preview_image),scaled);g_object_unref(scaled);g_object_unref(pix);u->serial=p.serial;
        }
        std::string msg=s.text+"\n"+std::to_string(s.frames)+" gecodeerde beelden · "+std::to_string(s.datagrams)+" TS-blokken";
        gtk_label_set_text(GTK_LABEL(u->status_label),msg.c_str());
        if(s.done){u->engine.reset();gtk_widget_set_sensitive(u->grid,TRUE);gtk_widget_set_sensitive(u->stop_button,FALSE);u->changed();}
        return G_SOURCE_CONTINUE;
    }
    void run(){
        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),"DATV Camera — lage bitrate experimenten");gtk_window_set_default_size(GTK_WINDOW(window),1050,820);
        auto box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(box),14);gtk_container_add(GTK_CONTAINER(window),box);
        auto intro=gtk_label_new("Camera → H.264 met roepnaam → MPEG-TS / UDP\nStel de zender apart in op hetzelfde DVB-profiel. Begin met een lokale preview.");gtk_box_pack_start(GTK_BOX(box),intro,FALSE,FALSE,0);
        auto columns=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,18);gtk_box_pack_start(GTK_BOX(box),columns,TRUE,TRUE,0);
        auto scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_widget_set_size_request(scroll,490,-1);gtk_box_pack_start(GTK_BOX(columns),scroll,TRUE,TRUE,0);
        grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),7);gtk_grid_set_column_spacing(GTK_GRID(grid),12);gtk_container_add(GTK_CONTAINER(scroll),grid);
        combo("Beeldbron","input",{{0,"Aangesloten / interne camera"},{1,"Testbeeld — bewegende bal"},{2,"Testbeeld — detail en beweging"},{3,"Testbeeld — ruis (zware belasting)"}},settings.test_source?settings.pattern+1:0);
        auto camera=gtk_combo_box_text_new_with_entry();for(const auto &d:devices())gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(camera),d.name.c_str(),d.path.c_str());
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(camera))),settings.device.c_str());
        std::string tip="Beschikbare camera's:";for(const auto &d:devices())tip+="\n"+d.path+" — "+d.name;
        if(devices().empty())tip+="\nGeen camera gevonden. Sluit een camera aan en open het programma opnieuw.";
        gtk_widget_set_tooltip_text(camera,tip.c_str());field("Camera (/dev/videoN)","device",camera);
        entry("Roepnaam in beeld","callsign",settings.callsign);entry("Zender IPv4","ip",settings.ip);spin("UDP-poort","port",settings.port,1,65535);
        std::vector<std::pair<int,std::string>> v;for(int i=0;i<DVB_SYSTEM_COUNT;++i)v.emplace_back(i,utf8(dvb_system_names[i]));combo("DVB-systeem (QPSK)","system",v,settings.dvb.system);
        v.clear();for(int i:dvb_symbol_rate_order)v.emplace_back(i,std::to_string(dvb_symbol_rates[i])+" ksym/s");combo("Symbolrate S / S2","sr",v,settings.dvb.symbol_rate);
        v.clear();for(int i:dvb_bandwidths)v.emplace_back(i,std::to_string(i)+" kHz");combo("Bandbreedte T (2K)","bw",v,settings.dvb.bandwidth_khz);
        v.clear();for(int i=0;i<DVB_FEC_COUNT;++i)v.emplace_back(i,utf8(dvb_fec_names[i]));combo("FEC","fec",v,settings.dvb.fec);
        combo("S2-pilots (normale frames)","pilots",{{0,"Uit"},{1,"Aan"}},settings.dvb.pilots);
        v.clear();for(int i=0;i<DVB_GUARD_COUNT;++i)v.emplace_back(i,utf8(dvb_guard_names[i]));combo("Guard interval T","guard",v,settings.dvb.guard);
        combo("Beeldprofiel toepassen","preset",{{-1,"Eigen instellingen / opgeslagen"},{0,"Detail — 320 breed, 2 beelden/s"},{1,"Balans — 240 breed, 4 beelden/s"},{2,"Beweging — 160 breed, 6 beelden/s"}},-1);
        spin("Breedte (even)","width",settings.width,160,640);combo("Beeldverhouding","aspect",{{43,"4:3"},{169,"16:9"}},settings.aspect);
        spin("Beelden per seconde","fps",settings.fps,2,12);spin("Sleutelbeeld elke N beelden","gop",settings.gop,1,180);
        spin("Video kbit/s (0 = automatisch)","video",settings.video_kbps,0,1000);
        combo("Ruisfilter","denoise",{{0,"Uit — maximale scherpte"},{1,"Licht"},{2,"Sterk — zachter beeld"}},settings.denoise);
        combo("Kleur","mono",{{0,"Kleur"},{1,"Zwart-wit"}},settings.monochrome);
        auto right=gtk_box_new(GTK_ORIENTATION_VERTICAL,12);gtk_box_pack_start(GTK_BOX(columns),right,FALSE,FALSE,0);
        auto caption=gtk_label_new("Gedecodeerde compressiepreview\nVergroot zonder extra verscherping; geen ontvangstmeting.");gtk_box_pack_start(GTK_BOX(right),caption,FALSE,FALSE,0);
        preview_image=gtk_image_new();gtk_widget_set_size_request(preview_image,480,360);gtk_box_pack_start(GTK_BOX(right),preview_image,FALSE,FALSE,0);
        auto hint=gtk_label_new("Detail: minder vloeiend, meer ruimte per beeld.\nBeweging: kleiner beeld, vaker een nieuw frame.\n\nGebruik daglicht en houd de camera stil.\nEen langer sleutelbeeldinterval spaart bits,\nmaar vertraagt instappen en herstel bij verlies.\n\nTransportbuffer: 2 s, plus camera/ontvanger.\nExperimentele DATV-rasters en beeldfrequenties.");gtk_label_set_xalign(GTK_LABEL(hint),0);gtk_box_pack_start(GTK_BOX(right),hint,FALSE,FALSE,0);
        budget_label=gtk_label_new("");gtk_label_set_line_wrap(GTK_LABEL(budget_label),TRUE);gtk_box_pack_start(GTK_BOX(box),budget_label,FALSE,FALSE,0);
        auto buttons=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),buttons,FALSE,FALSE,0);
        preview_button=gtk_button_new_with_label("Lokale preview");send_button=gtk_button_new_with_label("Start UDP naar zender");record_button=gtk_button_new_with_label("Lokale TS-opname…");stop_button=gtk_button_new_with_label("Stoppen");
        for(auto w:{preview_button,send_button,record_button,stop_button})gtk_box_pack_start(GTK_BOX(buttons),w,TRUE,TRUE,0);
        status_label=gtk_label_new("Gereed — kies camera of testbeeld");gtk_label_set_line_wrap(GTK_LABEL(status_label),TRUE);gtk_label_set_selectable(GTK_LABEL(status_label),TRUE);gtk_box_pack_start(GTK_BOX(box),status_label,FALSE,FALSE,0);
        for(auto &v:widgets){if(v.first=="preset")continue;g_signal_connect_swapped(v.second,GTK_IS_SPIN_BUTTON(v.second)?"value-changed":"changed",G_CALLBACK(+[](UI *u){u->changed();}),this);}
        g_signal_connect_swapped(widgets["preset"],"changed",G_CALLBACK(+[](UI *u){u->apply_preset();}),this);
        g_signal_connect_swapped(preview_button,"clicked",G_CALLBACK(+[](UI *u){u->start(0);}),this);
        g_signal_connect_swapped(send_button,"clicked",G_CALLBACK(+[](UI *u){u->start(1);}),this);
        g_signal_connect_swapped(record_button,"clicked",G_CALLBACK(+[](UI *u){u->start(2);}),this);
        g_signal_connect_swapped(stop_button,"clicked",G_CALLBACK(+[](UI *u){if(u->engine)u->engine->stop();}),this);
        g_signal_connect(window,"destroy",G_CALLBACK(+[](GtkWidget*,gpointer data){static_cast<UI*>(data)->closed=true;gtk_main_quit();}),this);
        changed();gtk_widget_set_sensitive(stop_button,FALSE);gtk_widget_show_all(window);
        guint timer=g_timeout_add(100,tick,this);gtk_main();g_source_remove(timer);engine.reset();
    }
};
static int integer(const std::string &s){size_t n=0;int v=std::stoi(s,&n);if(n!=s.size())throw std::runtime_error("Ongeldig getal: "+s);return v;}
int main(int argc,char **argv){
    try{
        Settings s;bool headless=false,probe=false,list=false,explicit_config=false;std::string path=std::string(g_get_user_config_dir())+"/datv-camera.ini",defaults;
        for(int i=1;i<argc;++i)if(std::string(argv[i])=="--config"){if(++i>=argc)throw std::runtime_error("Waarde ontbreekt voor --config");path=argv[i];explicit_config=true;}
        if(explicit_config||g_file_test(path.c_str(),G_FILE_TEST_EXISTS))s=load_settings(path);
        for(int i=1;i<argc;++i){std::string a=argv[i];auto val=[&](){if(++i>=argc)throw std::runtime_error("Waarde ontbreekt voor "+a);return std::string(argv[i]);};
            if(a=="--config"){val();}else if(a=="--headless")headless=true;else if(a=="--test-source")s.test_source=true;
            else if(a=="--preview")s.preview_only=true;else if(a=="--capture")s.capture=val();else if(a=="--seconds")s.seconds=integer(val());
            else if(a=="--pattern")s.pattern=integer(val());else if(a=="--device")s.device=val();else if(a=="--callsign")s.callsign=val();
            else if(a=="--ip")s.ip=val();else if(a=="--port")s.port=integer(val());else if(a=="--preset")preset(s,integer(val()));
            else if(a=="--probe")probe=true;else if(a=="--list-cameras")list=true;else if(a=="--write-defaults")defaults=val();
            else if(a=="--help"){
                std::cout<<"DATV Camera (Linux) — zonder opties: grafisch venster\nConfiguratie: "<<path<<"\n"
                    <<"--config PAD       eigen INI-bestand\n--headless         zonder venster (SIGINT/SIGTERM stopt)\n--device PAD       /dev/video0 enz.\n--callsign CALL    roepnaam in beeld en service\n--ip IP --port N   UDP-zender\n--preset N         0 detail, 1 balans, 2 beweging\n--preview          lokale compressietest, geen UDP\n--capture PAD      TS-opname IN PLAATS VAN UDP, weigert overschrijven\n--seconds N        duur TS-uitvoer; rondt lopend PES af\n--test-source      testbeeld in plaats van camera\n--pattern N        0 bal, 1 detail/beweging, 2 ruis\n--probe            controleer benodigde plugins\n--list-cameras     beschikbare V4L2-camera's\n--write-defaults P schrijf voorbeeldconfiguratie\n";return 0;
            }else throw std::runtime_error("Onbekende optie: "+a);
        }
        gst_init(nullptr,nullptr);
        if(probe){bool ok=true;for(const char *name:{"v4l2src","decodebin","videoconvert","videoscale","videorate","videotestsrc","x264enc","h264parse","avdec_h264","appsrc","appsink"}){
            auto f=gst_element_factory_find(name);std::cout<<name<<": "<<(f?"OK":"ONTBREEKT")<<"\n";ok&=f!=nullptr;if(f)gst_object_unref(f);
        }return ok?0:1;}
        if(list){for(const auto &d:devices())std::cout<<d.path<<" — "<<d.name<<"\n";return 0;}
        if(!defaults.empty()){if(g_file_test(defaults.c_str(),G_FILE_TEST_EXISTS))throw std::runtime_error("Configuratie bestaat al.");save_settings(Settings{},defaults);return 0;}
        validate(s);
        if(headless){
            signal(SIGINT,interrupt);signal(SIGTERM,interrupt);Engine engine(s);std::string previous;
            std::cerr<<"TS "<<dvb_bitrate(s.dvb)<<" bit/s; video "<<video_budget(s)<<" kbit/s; "<<s.width<<"x"<<s.height()<<" @ "<<s.fps<<"\n";
            while(true){auto status=engine.status();if(status.text!=previous){std::cerr<<status.text<<"\n";previous=status.text;}
                if(status.done)return status.failed?1:0;
                if(interrupted)engine.stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        if(s.seconds||!s.capture.empty()||s.preview_only)throw std::runtime_error("--seconds, --capture en --preview vereisen --headless; gebruik anders de vensterknoppen.");
        if(!gtk_init_check(nullptr,nullptr))throw std::runtime_error("Geen grafisch scherm beschikbaar; gebruik --headless.");
        UI ui;ui.settings=s;ui.path=path;ui.run();return 0;
    }catch(const std::exception &e){std::cerr<<"Fout: "<<e.what()<<"\n";return 1;}
}
