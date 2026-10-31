#include "engine.hpp"
#include "network.hpp"
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <unistd.h>
#include <signal.h>
#include <map>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <climits>
#include <vector>
static volatile sig_atomic_t interrupted=0;
static void interrupt(int){interrupted=1;}
static std::string utf8(const wchar_t *s){
    gchar *v=g_ucs4_to_utf8(reinterpret_cast<const gunichar*>(s),-1,nullptr,nullptr,nullptr);
    std::string r=v?v:"";g_free(v);return r;
}
struct UI {
    Settings settings;std::string path;std::unique_ptr<Engine> engine;
    GtkWidget *window,*grid,*status_label,*budget_label,*start_button,*stop_button,*network_label;
    unsigned ticks=0;
    std::vector<LocalAddress> addresses;
    std::map<std::string,GtkWidget*> widgets;int row=0;
    void label(const char *name,GtkWidget *w){
        GtkWidget *l=gtk_label_new(name);gtk_widget_set_halign(l,GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid),l,0,row,1,1);gtk_grid_attach(GTK_GRID(grid),w,1,row++,1,1);
        gtk_widget_set_hexpand(w,TRUE);
    }
    void entry(const char *name,const char *key,const std::string &v){
        auto w=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(w),v.c_str());widgets[key]=w;label(name,w);
    }
    void spin(const char *name,const char *key,int value,int lo,int hi){
        auto w=gtk_spin_button_new_with_range(lo,hi,1);gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),value);widgets[key]=w;label(name,w);
    }
    void combo(const char *name,const char *key,const std::vector<std::pair<int,std::string>> &values,int value){
        auto w=gtk_combo_box_text_new();for(auto &v:values)gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w),std::to_string(v.first).c_str(),v.second.c_str());
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(w),std::to_string(value).c_str());widgets[key]=w;label(name,w);
    }
    int number(const char *key){return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.at(key)));}
    int choice(const char *key){const char *v=gtk_combo_box_get_active_id(GTK_COMBO_BOX(widgets.at(key)));return v?std::stoi(v):-1;}
    std::string text(const char *key){return gtk_entry_get_text(GTK_ENTRY(widgets.at(key)));}
    Settings read(){
        Settings s=settings;s.input=choice("input");s.rtsp_uri=text("rtsp");s.uri=text("uri");s.ip=text("ip");s.callsign=text("callsign");s.port=number("port");s.latency=number("latency");
        s.dvb.system=choice("system");s.dvb.symbol_rate=choice("sr");s.dvb.fec=choice("fec");s.dvb.guard=choice("guard");
        s.dvb.bandwidth_khz=choice("bw");s.dvb.pilots=choice("pilots");s.width=number("width");s.aspect=choice("aspect");
        s.fps=number("fps");s.gop=number("gop");s.video_kbps=number("video");s.audio_kbps=number("audio");return s;
    }
    void budget(){
        auto s=read();
        gtk_label_set_text(GTK_LABEL(network_label),(s.input==1?rtsp_phone_addresses(s.rtsp_uri,addresses):phone_addresses(s.uri,addresses)).c_str());std::string t="Beeld: "+std::to_string(s.width)+" × "+std::to_string(s.height())+" · "+(s.aspect==169?"16:9":"4:3")+"\nQPSK · S2 normale frames · T 2K\nTS: "+std::to_string(dvb_bitrate(s.dvb))+" bit/s · video: "+std::to_string(video_budget(s))+" kbit/s · AAC: "+std::to_string(audio_budget(s))+" kbit/s";
        try{validate(s);}catch(const std::exception &e){t+="\n"+std::string(e.what());}
        gtk_label_set_text(GTK_LABEL(budget_label),t.c_str());
        gtk_widget_set_sensitive(widgets["uri"],s.input==0);
        gtk_widget_set_sensitive(widgets["rtsp"],s.input==1);
        gtk_widget_set_sensitive(widgets["sr"],s.dvb.system!=DVB_T);
        gtk_widget_set_sensitive(widgets["bw"],s.dvb.system==DVB_T);
        gtk_widget_set_sensitive(widgets["guard"],s.dvb.system==DVB_T);
        gtk_widget_set_sensitive(widgets["pilots"],s.dvb.system==DVB_S2);
    }
    void start(){
        try{
            Settings s=read();validate(s);save_settings(s,path);settings=s;engine=std::make_unique<Engine>(s);
            gtk_widget_set_sensitive(grid,FALSE);gtk_widget_set_sensitive(start_button,FALSE);gtk_widget_set_sensitive(stop_button,TRUE);
        }catch(const std::exception &e){gtk_label_set_text(GTK_LABEL(status_label),e.what());}
    }
    static gboolean tick(gpointer data){
        auto u=static_cast<UI*>(data);
        if(++u->ticks%50==0){u->addresses=local_addresses();gtk_label_set_text(GTK_LABEL(u->network_label),(u->choice("input")==1?rtsp_phone_addresses(u->text("rtsp"),u->addresses):phone_addresses(u->text("uri"),u->addresses)).c_str());}
        if(!u->engine)return G_SOURCE_CONTINUE;
        auto s=u->engine->status();std::string t=s.text+" · "+std::to_string(s.datagrams)+" UDP-blokken (1316 bytes)";
        gtk_label_set_text(GTK_LABEL(u->status_label),t.c_str());
        if(s.done){u->engine.reset();gtk_widget_set_sensitive(u->grid,TRUE);gtk_widget_set_sensitive(u->start_button,TRUE);gtk_widget_set_sensitive(u->stop_button,FALSE);u->budget();}
        return G_SOURCE_CONTINUE;
    }
    void run(){
        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),"DATV Streambridge — SRT / RTSP naar MPEG-TS");gtk_window_set_default_size(GTK_WINDOW(window),650,780);
        auto box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(box),16);gtk_container_add(GTK_CONTAINER(window),box);
        auto intro=gtk_label_new("SRT / RTSP ontvangen → H.264 + AAC → CBR MPEG-TS over UDP\nStel de zender afzonderlijk in op hetzelfde DVB-profiel.");gtk_box_pack_start(GTK_BOX(box),intro,FALSE,FALSE,0);
        addresses=local_addresses();network_label=gtk_label_new("");gtk_label_set_selectable(GTK_LABEL(network_label),TRUE);gtk_label_set_line_wrap(GTK_LABEL(network_label),TRUE);gtk_label_set_xalign(GTK_LABEL(network_label),0);gtk_box_pack_start(GTK_BOX(box),network_label,FALSE,FALSE,0);
        auto scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_box_pack_start(GTK_BOX(box),scroll,TRUE,TRUE,0);
        grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),6);gtk_grid_set_column_spacing(GTK_GRID(grid),16);gtk_container_add(GTK_CONTAINER(scroll),grid);
        combo("Ingang","input",{{0,"SRT"},{1,"RTSP — ontvangst van Larix"}},settings.input);
        entry("RTSP-luisteradres","rtsp",settings.rtsp_uri);
        entry("SRT-URL (caller of listener)","uri",settings.uri);spin("Invoerlatency (ms)","latency",settings.latency,20,8000);
        entry("UDP-bestemming (IPv4)","ip",settings.ip);spin("UDP-poort","port",settings.port,1,65535);entry("Callsign (service en provider)","callsign",settings.callsign);
        std::vector<std::pair<int,std::string>> v;for(int i=0;i<DVB_SYSTEM_COUNT;++i)v.emplace_back(i,utf8(dvb_system_names[i]));combo("DVB-systeem","system",v,settings.dvb.system);
        v.clear();for(int i:dvb_symbol_rate_order)v.emplace_back(i,std::to_string(dvb_symbol_rates[i])+" ksym/s");combo("Symbolrate","sr",v,settings.dvb.symbol_rate);
        v.clear();for(int i:dvb_bandwidths)v.emplace_back(i,std::to_string(i)+" kHz");combo("DVB-T-bandbreedte","bw",v,settings.dvb.bandwidth_khz);
        v.clear();for(int i=0;i<DVB_FEC_COUNT;++i)v.emplace_back(i,utf8(dvb_fec_names[i]));combo("FEC","fec",v,settings.dvb.fec);
        combo("DVB-S2-pilots","pilots",{{0,"Uit"},{1,"Aan"}},settings.dvb.pilots);
        v.clear();for(int i=0;i<DVB_GUARD_COUNT;++i)v.emplace_back(i,utf8(dvb_guard_names[i]));combo("DVB-T guard interval","guard",v,settings.dvb.guard);
        spin("Breedte (even)","width",settings.width,160,Settings::max_width);combo("Beeldverhouding","aspect",{{43,"4:3"},{169,"16:9"}},settings.aspect);
        spin("Beelden per seconde","fps",settings.fps,2,30);spin("GOP (beelden)","gop",settings.gop,1,60);
        spin("Video kbit/s (0 = automatisch)","video",settings.video_kbps,0,2000);spin("AAC kbit/s (-1 = auto, 0 = uit)","audio",settings.audio_kbps,-1,512);
        budget_label=gtk_label_new("");gtk_label_set_line_wrap(GTK_LABEL(budget_label),TRUE);gtk_box_pack_start(GTK_BOX(box),budget_label,FALSE,FALSE,0);
        auto buttons=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);start_button=gtk_button_new_with_label("Opslaan en starten");stop_button=gtk_button_new_with_label("Stoppen");
        gtk_box_pack_start(GTK_BOX(buttons),start_button,TRUE,TRUE,0);gtk_box_pack_start(GTK_BOX(buttons),stop_button,TRUE,TRUE,0);gtk_box_pack_start(GTK_BOX(box),buttons,FALSE,FALSE,0);
        status_label=gtk_label_new("Gereed");gtk_label_set_line_wrap(GTK_LABEL(status_label),TRUE);gtk_box_pack_start(GTK_BOX(box),status_label,FALSE,FALSE,0);
        for(auto &v:widgets){const char *signal=GTK_IS_SPIN_BUTTON(v.second)?"value-changed":"changed";g_signal_connect_swapped(v.second,signal,G_CALLBACK(+[](UI *u){u->budget();}),this);}
        g_signal_connect_swapped(start_button,"clicked",G_CALLBACK(+[](UI *u){u->start();}),this);
        g_signal_connect_swapped(stop_button,"clicked",G_CALLBACK(+[](UI *u){if(u->engine)u->engine->stop();}),this);
        g_signal_connect(window,"destroy",G_CALLBACK(+[](GtkWidget*,gpointer){gtk_main_quit();}),nullptr);
        gtk_widget_set_sensitive(stop_button,FALSE);budget();gtk_widget_show_all(window);guint timer=g_timeout_add(100,tick,this);gtk_main();g_source_remove(timer);engine.reset();
    }
};
static int integer(const std::string &s){size_t n=0;int v=std::stoi(s,&n);if(n!=s.size())throw std::runtime_error("Ongeldig getal: "+s);return v;}
int main(int argc,char **argv){
    try{
        bool headless=false,test=false,probe=false,noaudio=false;int seconds=0;std::string capture,uri,rtsp,defaults;
        std::string path=std::string(g_get_user_config_dir())+"/datv-streambridge.ini";bool explicit_config=false;
        for(int i=1;i<argc;++i){std::string a=argv[i];auto val=[&](){if(++i>=argc)throw std::runtime_error("Waarde ontbreekt voor "+a);return std::string(argv[i]);};
            if(a=="--headless")headless=true;else if(a=="--test-source")test=true;else if(a=="--probe")probe=true;
            else if(a=="--config"){path=val();explicit_config=true;}else if(a=="--seconds")seconds=integer(val());
            else if(a=="--capture")capture=val();else if(a=="--srt")uri=val();else if(a=="--rtsp")rtsp=val();else if(a=="--no-audio")noaudio=true;
            else if(a=="--write-defaults")defaults=val();
            else if(a=="--help"){std::cout<<"DATV Streambridge (Linux)\nZonder opties: grafisch venster. Eigen configuratie: "<<path<<"\n\n--config PAD       eigen INI-bestand\n--headless         zonder venster, SIGINT/SIGTERM stopt\n--srt URL          SRT caller/listener URL\n--rtsp URL         RTSP RECORD luisteradres (bv. rtsp://0.0.0.0:8554/live)\n--no-audio         alleen beeld\n--seconds N        automatisch stoppen na N seconden TS-uitvoer\n--capture PAD      lokale TS-opname IN PLAATS VAN UDP; weigert overschrijven\n--test-source      bewegend testbeeld + toon IN PLAATS VAN SRT\n--probe            controleer GStreamer-componenten\n--write-defaults P schrijf voorbeeldconfiguratie\n";return 0;}
            else throw std::runtime_error("Onbekende optie: "+a);
        }
        gst_init(nullptr,nullptr);
        char exe[PATH_MAX];ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
        if(n>0){exe[n]=0;std::string p=exe;p=p.substr(0,p.find_last_of('/'))+"/plugins";gst_registry_scan_path(gst_registry_get(),p.c_str());}
        if(probe){bool ok=true;for(const char *name:{"srtsrc","tsdemux","h264parse","openh264dec","x264enc","aacparse","fdkaacenc","fdkaacdec","appsrc","appsink","rtph264depay","rtpmp4gdepay","rtpmp4adepay","rtpbin"}){
            GstElementFactory *f=gst_element_factory_find(name);std::cout<<name<<": "<<(f?"OK":"ONTBREEKT")<<"\n";ok&=f!=nullptr;if(f)gst_object_unref(f);
        }return ok?0:1;}
        Settings s;
        if(!defaults.empty()){if(g_file_test(defaults.c_str(),G_FILE_TEST_EXISTS))throw std::runtime_error("Configuratie bestaat al.");save_settings(s,defaults);return 0;}
        if(explicit_config||g_file_test(path.c_str(),G_FILE_TEST_EXISTS))s=load_settings(path);
        s.test_source=test;s.seconds=seconds;s.capture=capture;if(!uri.empty()&&!rtsp.empty())throw std::runtime_error("Kies --srt of --rtsp.");if(!uri.empty()){s.uri=uri;s.input=0;}if(!rtsp.empty()){s.rtsp_uri=rtsp;s.input=1;}if(noaudio)s.audio_kbps=0;
        validate(s);
        if(headless){
            signal(SIGINT,interrupt);signal(SIGTERM,interrupt);Engine engine(s);std::string previous;
            while(true){auto status=engine.status();if(status.text!=previous){std::cerr<<status.text<<"\n";previous=status.text;}
                if(status.done)return status.text.rfind("Fout: ",0)==0?1:0;
                if(interrupted)engine.stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        if(test||seconds||!capture.empty())throw std::runtime_error("Testbron, capture en duur vereisen --headless.");
        if(!gtk_init_check(nullptr,nullptr))throw std::runtime_error("Geen grafisch scherm beschikbaar; gebruik --headless.");
        UI ui;ui.settings=s;ui.path=path;ui.run();return 0;
    }catch(const std::exception &e){std::cerr<<"Fout: "<<e.what()<<"\n";return 1;}
}
