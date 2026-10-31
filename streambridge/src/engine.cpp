#include "engine.hpp"
#include "mux.hpp"
#include "avc.hpp"
#include "input.hpp"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib/gstdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>
#include <fstream>
#include <cerrno>
using namespace transport;
using Clock=std::chrono::steady_clock;
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int audio_budget(const Settings &s) {
    if(s.audio_kbps>=0)return s.audio_kbps;
    int rate=dvb_bitrate(s.dvb);
    return rate>=300000?96:rate>=220000?64:48;
}
static int audio_group(int kbps){return kbps>=192?1:kbps>=96?2:4;}
static int audio_overhead(const Settings &s){
    int kbps=audio_budget(s);
    // TS headers, ADTS/PES headers and worst-case final-packet stuffing per PES.
    return kbps?kbps*1000/46+74000/audio_group(kbps)+3000:0;
}
int video_budget(const Settings &s) {
    // Reserve worst-case separate PCR, PSI/SI and PES/TS overhead, plus 15% headroom.
    int available=(dvb_bitrate(s.dvb)-60000-audio_budget(s)*1000-audio_overhead(s))*85/100000;
    return s.video_kbps?s.video_kbps:std::max(0,available);
}
void validate(const Settings &s) {
    int rate=dvb_bitrate(s.dvb);
    require(rate>=DATV_MIN_BITRATE,"Ongeldig DVB-profiel of TS-bitrate te laag.");
    require(s.audio_kbps==-1||s.audio_kbps==0||(s.audio_kbps>=48&&s.audio_kbps<=512),"AAC moet -1 (automatisch), 0 (uit) of 48–512 kbit/s zijn.");
    require(s.video_kbps>=0 && s.video_kbps<=2000,"Video moet 0 (automatisch) of maximaal 2000 kbit/s zijn.");
    require(video_budget(s)>=10 && video_budget(s)<=(rate-60000-audio_budget(s)*1000-audio_overhead(s))*85/100000,
            "Onvoldoende TS-capaciteit. Verhoog SR/FEC, zet audio uit of verlaag de videobitrate (0 = automatisch).");
    require(s.aspect==43||s.aspect==169,"Beeldverhouding moet 4:3 of 16:9 zijn.");
    require(s.width>=160&&s.width<=Settings::max_width&&s.height()>=90&&s.height()<=Settings::max_height&&!(s.width%2)&&!(s.height()%2),"Resolutie: even waarden, 160–1920 pixels breed, hoogte automatisch uit 4:3 of 16:9.");
    require(s.fps>=2&&s.fps<=30&&s.gop>=1&&s.gop<=s.fps*2,"Framerate 2–30; GOP 1 tot maximaal twee seconden.");
    require(s.latency>=20&&s.latency<=8000,"Invoerlatency moet 20–8000 ms zijn.");
    require(s.seconds>=0&&s.seconds<=86400,"Testduur moet 0–86400 seconden zijn.");
    require(!s.callsign.empty()&&s.callsign.size()<=32,"Callsign: 1–32 ASCII-tekens.");
    for(unsigned char c:s.callsign)require(c>=32&&c<=126,"Gebruik ASCII-tekens in de callsign.");
    require(s.input==0||s.input==1,"Kies SRT of RTSP als ingang.");
    if(!s.test_source&&s.input==1){
        GUri *u=g_uri_parse(s.rtsp_uri.c_str(),G_URI_FLAGS_NONE,nullptr);
        require(u!=nullptr,"Ongeldig RTSP-luisteradres.");
        const char *scheme=g_uri_get_scheme(u),*host=g_uri_get_host(u),*path=g_uri_get_path(u);
        in_addr addr{};
        bool valid=scheme&&std::string(scheme)=="rtsp"&&host&&inet_pton(AF_INET,host,&addr)==1
            &&(ntohl(addr.s_addr)>>24)<224&&g_uri_get_port(u)>0&&g_uri_get_port(u)<=65535
            &&path&&path[0]=='/'&&path[1]&&!g_uri_get_query(u)&&!g_uri_get_fragment(u)&&!g_uri_get_userinfo(u);
        if(path)for(const unsigned char *c=reinterpret_cast<const unsigned char*>(path);*c;++c)
            valid=valid&&(*c=='/'||*c=='-'||*c=='_'||g_ascii_isalnum(*c));
        g_uri_unref(u);require(valid,"RTSP: gebruik rtsp://0.0.0.0:8554/live of een lokaal IPv4-adres, poort en pad (letters/cijfers/-/_).");
    }
    if(!s.test_source&&s.input==0){
        require(s.uri.rfind("srt://",0)==0,"De invoer moet een srt://-URL zijn.");
        GError *e=nullptr; GUri *u=g_uri_parse(s.uri.c_str(),G_URI_FLAGS_NONE,&e);
        if(e)g_error_free(e);
        require(u!=nullptr,"Ongeldige SRT-URL.");
        int port=g_uri_get_port(u);g_uri_unref(u);
        require(port>0&&port<=65535,"Geef een SRT-poort op (1–65535).");
    }
    if(s.capture.empty()){
        in_addr a{};require(inet_pton(AF_INET,s.ip.c_str(),&a)==1,"Uitvoer vereist een numeriek unicast IPv4-adres.");
        unsigned v=ntohl(a.s_addr);
        require((v>>24)>0&&(v>>24)<224&&v!=0xffffffff,"Gebruik een unicast IPv4-adres.");
        require(s.port>=1&&s.port<=65535,"UDP-poort moet 1–65535 zijn.");
    }
}
static void config_fields(GKeyFile *k,Settings &s,bool save) {
    auto number=[&](const char *key,int &v){
        if(save)g_key_file_set_integer(k,"streambridge",key,v);
        else if(g_key_file_has_key(k,"streambridge",key,nullptr)){
            GError *e=nullptr;int n=g_key_file_get_integer(k,"streambridge",key,&e);
            if(e){g_error_free(e);throw std::runtime_error(std::string("Ongeldige instelling: ")+key);}v=n;
        }
    };
    auto str=[&](const char *key,std::string &v){
        if(save)g_key_file_set_string(k,"streambridge",key,v.c_str());
        else if(g_key_file_has_key(k,"streambridge",key,nullptr)){gchar *t=g_key_file_get_string(k,"streambridge",key,nullptr);if(t){v=t;g_free(t);}}
    };
    number("input",s.input);str("rtsp",s.rtsp_uri);str("srt",s.uri);str("ip",s.ip);str("callsign",s.callsign);
    number("port",s.port);number("latency",s.latency);number("width",s.width);
    if(!save && !g_key_file_has_key(k,"streambridge","aspect",nullptr)){
        int legacy_height=s.height();number("height",legacy_height);
        // Migrate old independent dimensions to the closest supported display ratio.
        if(legacy_height>0 && double(s.width)/legacy_height>14.0/9.0)s.aspect=169;
    }
    number("aspect",s.aspect);
    number("fps",s.fps);number("gop",s.gop);number("video_kbps",s.video_kbps);number("audio_kbps",s.audio_kbps);
    number("system",s.dvb.system);number("symbol_rate_id",s.dvb.symbol_rate);number("fec_id",s.dvb.fec);
    number("pilots",s.dvb.pilots);number("guard_id",s.dvb.guard);number("bandwidth_khz",s.dvb.bandwidth_khz);
}
Settings load_settings(const std::string &path){
    Settings s;GKeyFile *k=g_key_file_new();GError *e=nullptr;
    if(!g_key_file_load_from_file(k,path.c_str(),G_KEY_FILE_NONE,&e)){
        std::string msg=e->message;g_error_free(e);g_key_file_unref(k);throw std::runtime_error(msg);
    }
    try{config_fields(k,s,false);validate(s);}catch(...){g_key_file_unref(k);throw;}
    g_key_file_unref(k);return s;
}
void save_settings(const Settings &source,const std::string &path){
    validate(source);Settings s=source;GKeyFile *k=g_key_file_new();config_fields(k,s,true);
    gsize size=0;gchar *data=g_key_file_to_data(k,&size,nullptr);GError *e=nullptr;
    bool ok=g_file_set_contents_full(path.c_str(),data,size,G_FILE_SET_CONTENTS_CONSISTENT,0600,&e);
    g_free(data);g_key_file_unref(k);
    if(!ok){std::string msg=e->message;g_error_free(e);throw std::runtime_error(msg);}
}
struct Engine::Impl {
    Settings s;std::mutex mutex;std::thread thread;std::atomic<bool> stop{false};Status info;std::string failure;
    GstElement *pipeline=nullptr;std::deque<Unit> queues[2];int64_t last_pts[2]={-1,-1};
    Unit audio_pending;int audio_frames=0;int64_t audio_origin=-1;uint64_t audio_samples=0;
    Clock::time_point last_input[2];size_t bytes[2]={};
    struct Callback {Impl *self;int media;} cb[2]={{this,0},{this,1}};
    explicit Impl(Settings settings):s(std::move(settings)){}
    void fail(const std::string &msg){std::lock_guard<std::mutex> l(mutex);if(failure.empty())failure="Fout: "+msg;info.text=failure;stop=true;}
    static GstFlowReturn sample(GstAppSink *sink,gpointer data){
        auto c=static_cast<Callback*>(data);auto p=c->self;int a=c->media;
        GstSample *sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        GstBuffer *b=gst_sample_get_buffer(sample);const GstSegment *seg=gst_sample_get_segment(sample);
        GstClockTime pts=gst_segment_to_running_time(seg,GST_FORMAT_TIME,GST_BUFFER_PTS(b));
        GstMapInfo map{};
        if(pts==GST_CLOCK_TIME_NONE||!gst_buffer_map(b,&map,GST_MAP_READ)){
            gst_sample_unref(sample);p->fail("Invoer zonder bruikbare tijdstempels.");return GST_FLOW_ERROR;
        }
        Unit u;u.pts=gst_util_uint64_scale(pts,90000,GST_SECOND);u.key=!GST_BUFFER_FLAG_IS_SET(b,GST_BUFFER_FLAG_DELTA_UNIT);
        u.bytes.assign(map.data,map.data+map.size);gst_buffer_unmap(b,&map);gst_sample_unref(sample);
        if(a==0){
            try{u.bytes=avc_for_transport(u.bytes);u.slice=first_slice(u.bytes);}
            catch(const std::exception &e){p->fail(e.what());return GST_FLOW_ERROR;}
        }
        std::lock_guard<std::mutex> l(p->mutex);
        if(p->stop)return GST_FLOW_FLUSHING;
        if(a==1){
            // AAC-LC at 48 kHz: each access unit advances exactly 1024 samples.
            // Smooth small demux/encoder timestamp corrections, never drop audio.
            if(p->audio_origin<0)p->audio_origin=u.pts;
            int64_t expected=p->audio_origin+int64_t(p->audio_samples*90000/48000);
            if(std::abs(u.pts-expected)>9000){
                p->failure=p->info.text="Fout: Audioklok wijkt meer dan 100 ms af ("+std::to_string(u.pts)+" versus "+std::to_string(expected)+"); herstart vereist.";p->stop=true;return GST_FLOW_ERROR;
            }
            u.pts=expected;p->audio_samples+=1024;
        }
        if(p->last_pts[a]>=0 && u.pts<=p->last_pts[a]){
            p->failure=p->info.text="Fout: "+std::string(a?"Audio":"Video")+"-tijdstempel springt achteruit ("+std::to_string(p->last_pts[a])+" naar "+std::to_string(u.pts)+"); herstart vereist.";p->stop=true;return GST_FLOW_ERROR;
        }
        if(p->queues[a].size()>300 || p->bytes[a]+u.bytes.size()>4*1024*1024 ||
           (!p->queues[a].empty()&&u.pts-p->queues[a].front().pts>270000)){
            p->failure=p->info.text="Fout: "+std::string(a?"Audio":"Video")+"-invoerbuffer vol; bronklok of encoder loopt te snel.";p->stop=true;return GST_FLOW_ERROR;
        }
        p->last_pts[a]=u.pts;p->last_input[a]=Clock::now();
        if(a==1){
            if(!p->audio_frames)p->audio_pending.pts=u.pts;
            p->audio_pending.bytes.insert(p->audio_pending.bytes.end(),u.bytes.begin(),u.bytes.end());
            if(++p->audio_frames<audio_group(p->s.audio_kbps))return GST_FLOW_OK;
            u=std::move(p->audio_pending);p->audio_pending=Unit{};p->audio_frames=0;
        }
        p->bytes[a]+=u.bytes.size();p->queues[a].push_back(std::move(u));return GST_FLOW_OK;
    }
    void bus_errors(GstBus *bus){
        while(GstMessage *m=gst_bus_pop_filtered(bus,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS))){
            if(GST_MESSAGE_TYPE(m)==GST_MESSAGE_EOS){gst_message_unref(m);throw std::runtime_error("SRT-invoer beëindigd; herstart vereist.");}
            GError *e=nullptr;gchar *debug=nullptr;gst_message_parse_error(m,&e,&debug);
            std::string msg=e?e->message:"GStreamer-fout";g_clear_error(&e);g_free(debug);gst_message_unref(m);throw std::runtime_error(msg);
        }
    }
    void run(){
        int fd=-1;FILE *file=nullptr;GstBus *bus=nullptr;
        std::unique_ptr<LiveInput> input;std::thread feeder;std::atomic<bool> feeder_stop{false};GstElement *sources[2]={};
        try{
            validate(s);
            s.audio_kbps=audio_budget(s);
            // A single continuous encoder owns the output clock, including fallback.
            std::ostringstream desc;
            desc<<"appsrc name=rawvideo is-live=true format=time ! video/x-raw,format=BGRx,width="<<s.width<<",height="<<s.height()<<",framerate="<<s.fps<<"/1,pixel-aspect-ratio="<<s.pixel_aspect()<<" ! videoconvert ! video/x-raw,format=I420 ! "
                <<"x264enc bitrate="<<video_budget(s)<<" key-int-max="<<s.gop<<" tune=zerolatency speed-preset=veryfast bframes=0 byte-stream=true aud=true vbv-buf-capacity=400 option-string=force-cfr=1 nal-hrd=none ! "
                <<"video/x-h264,profile=main,stream-format=byte-stream,alignment=au ! h264parse config-interval=-1 ! appsink name=vsink sync=false emit-signals=true max-buffers=8 ";
            if(s.audio_kbps){
                desc<<"appsrc name=rawaudio is-live=true format=time ! audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved ! audioconvert ! fdkaacenc perfect-timestamp=true bitrate="<<s.audio_kbps*1000
                    <<" ! aacparse ! audio/mpeg,mpegversion=4,stream-format=adts,profile=lc ! appsink name=asink sync=false emit-signals=true max-buffers=16";
            }
            GError *e=nullptr;pipeline=gst_parse_launch(desc.str().c_str(),&e);
            if(e){std::string msg=e->message;g_error_free(e);throw std::runtime_error(msg);}
            require(pipeline,"Kan GStreamer-pipeline niet maken.");
            for(int a=0;a<(s.audio_kbps?2:1);++a){
                GstElement *sink=gst_bin_get_by_name(GST_BIN(pipeline),a?"asink":"vsink");
                g_signal_connect(sink,"new-sample",G_CALLBACK(sample),&cb[a]);gst_object_unref(sink);
            }
            bus=gst_element_get_bus(pipeline);
            require(gst_element_set_state(pipeline,GST_STATE_PLAYING)!=GST_STATE_CHANGE_FAILURE,"Kan invoer niet starten.");
            auto epoch=Clock::now();input=std::make_unique<LiveInput>(s,epoch);
            for(int a=0;a<(s.audio_kbps?2:1);++a)sources[a]=gst_bin_get_by_name(GST_BIN(pipeline),a?"rawaudio":"rawvideo");
            feeder=std::thread([&]{
                try{
                    uint64_t video_frame=0,audio_sample=0;
                    while(!feeder_stop&&!stop){
                        uint64_t vt=video_frame*GST_SECOND/s.fps;
                        uint64_t at=s.audio_kbps?audio_sample*GST_SECOND/48000:UINT64_MAX;
                        bool sound=at<vt;uint64_t time=sound?at:vt;
                        std::this_thread::sleep_until(epoch+std::chrono::nanoseconds(time));
                        if(feeder_stop||stop)break;
                        auto data=sound?input->sound(audio_sample):input->picture(time*90000/GST_SECOND);
                        auto src=GST_APP_SRC(sources[sound?1:0]);
                        require(gst_app_src_get_current_level_buffers(src)<8,"Uitvoerencoder kan het tempo niet bijhouden.");
                        GstBuffer *b=gst_buffer_new_allocate(nullptr,data.size(),nullptr);require(b,"Kan uitvoerframe niet alloceren.");
                        gst_buffer_fill(b,0,data.data(),data.size());GST_BUFFER_PTS(b)=time;
                        GST_BUFFER_DURATION(b)=sound?GST_SECOND*1024/48000:(video_frame+1)*GST_SECOND/s.fps-time;
                        require(gst_app_src_push_buffer(src,b)==GST_FLOW_OK,"Uitvoerencoder accepteert geen nieuwe frames.");
                        if(sound)audio_sample+=1024;else ++video_frame;
                    }
                }catch(const std::exception &e){fail(e.what());}
            });
            auto waiting=Clock::now();auto ready=waiting;bool have=false;int64_t base=0;
            {std::lock_guard<std::mutex> l(mutex);info.text="Uitvoer voorbereiden (No Signal)";}
            while(!stop){
                bus_errors(bus);
                {std::lock_guard<std::mutex> l(mutex);
                    
                    if(!queues[0].empty()&&(!s.audio_kbps||!queues[1].empty())){
                        require(queues[0].front().key,"Eerste videoframe is geen IDR.");
                        if(!have){have=true;ready=Clock::now();base=queues[0].front().pts;if(s.audio_kbps)base=std::min(base,queues[1].front().pts);}
                    }
                }
                if(have&&Clock::now()-ready>=std::chrono::milliseconds(500))break;
                require(Clock::now()-waiting<std::chrono::seconds(30),"Uitvoerencoder start niet binnen 30 seconden.");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if(!stop){
                if(s.capture.empty()){
                    fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);require(fd>=0,"Kan UDP-socket niet openen.");
                    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(s.port);inet_pton(AF_INET,s.ip.c_str(),&addr.sin_addr);
                    require(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0,"Kan UDP-bestemming niet instellen.");
                }else{
                    int out=open(s.capture.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
                    require(out>=0,"Kan capture niet maken (bestand bestaat al of map is niet schrijfbaar).");
                    file=fdopen(out,"wb");if(!file){close(out);throw std::runtime_error("Kan capture niet openen.");}
                }
                static std::atomic<int> version{int(time(nullptr))&31};
                Mux mux(dvb_bitrate(s.dvb),s.audio_kbps,s.callsign,base,version++,s.width>720||s.height()>576);
                auto start=Clock::now();uint64_t count=0;bool finalizing=false;uint8_t data[1316];
                {std::lock_guard<std::mutex> l(mutex);info.running=true;info.text="Actief";}
                while(!stop){
                    bus_errors(bus);
                    auto due=start+std::chrono::nanoseconds(ticks(count*1316,1000000000,dvb_bitrate(s.dvb)));
                    // No catch-up bursts: stop when a complete datagram interval is missed.
                    std::this_thread::sleep_until(due);
                    auto tolerance=std::chrono::nanoseconds(std::max<uint64_t>(5000000,ticks(1316,1000000000,dvb_bitrate(s.dvb))));
                    require(Clock::now()-due<=tolerance,"UDP-timing gemist door systeembelasting; uitvoer gestopt.");
                    {std::lock_guard<std::mutex> l(mutex);
                        for(int a=0;a<(s.audio_kbps?2:1);++a)
                            require(Clock::now()-last_input[a]<std::chrono::seconds(2),"Uitvoerencoder stilgevallen; uitvoer gestopt.");
                        for(int i=0;i<7;++i){
                            if(s.seconds&&mux.time90()>=uint64_t(s.seconds)*90000){
                                finalizing=true;queues[0].clear();queues[1].clear();
                            }
                            mux.next(data+i*188,queues);
                        }
                        for(int a=0;a<2;++a){bytes[a]=0;for(const auto &u:queues[a])bytes[a]+=u.bytes.size();}
                    }
                    if(file)require(fwrite(data,1,sizeof(data),file)==sizeof(data),"Schrijven van capture mislukt.");
                    else require(send(fd,data,sizeof(data),0)==sizeof(data),"UDP-verzending mislukt (controleer ontvanger/netwerk).");
                    ++count;{auto state=input->status(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-epoch).count()*9/100);std::lock_guard<std::mutex> l(mutex);info.datagrams=count;info.text=state;}
                    if(finalizing&&!mux.pending())break;
                }
            }
        }catch(const std::exception &e){fail(e.what());}
        feeder_stop=true;if(feeder.joinable())feeder.join();input.reset();
        for(auto src:sources)if(src)gst_object_unref(src);
        if(pipeline){gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(pipeline);pipeline=nullptr;}
        if(bus)gst_object_unref(bus);
        if(fd>=0)close(fd);
        if(file&&fclose(file)!=0)fail("Sluiten van capture mislukt.");
        std::lock_guard<std::mutex> l(mutex);info.running=false;info.done=true;
        info.text=failure.empty()?"Gestopt":failure;
    }
};
Engine::Engine(Settings s):p(new Impl(std::move(s))){p->thread=std::thread([this]{p->run();});}
Engine::~Engine(){stop();if(p->thread.joinable())p->thread.join();}
void Engine::stop(){p->stop=true;}
Status Engine::status(){std::lock_guard<std::mutex> l(p->mutex);return p->info;}
