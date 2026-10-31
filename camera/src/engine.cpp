#include "engine.hpp"
#include "avc.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <cmath>
namespace camera {
using namespace transport;
using Clock=std::chrono::steady_clock;
static void require(bool ok,const char *msg){if(!ok)throw std::runtime_error(msg);}
struct Engine::Impl {
    Settings s;std::mutex mutex;std::thread worker;std::atomic<bool> stopped{false};
    Status info;Picture preview;Bytes raw;Clock::time_point last_raw{},last_encoded{};
    std::deque<Unit> queues[2];int64_t last_pts=-1;size_t queued_bytes=0;
    GstElement *input=nullptr,*encoder=nullptr;
    explicit Impl(Settings settings):s(std::move(settings)){}
    void fail(const std::string &msg){std::lock_guard<std::mutex> lock(mutex);if(!info.failed){info.failed=true;info.text="Fout: "+msg;}stopped=true;}
    bool until(Clock::time_point due){
        while(!stopped&&Clock::now()<due)std::this_thread::sleep_for(std::min(std::chrono::milliseconds(10),std::chrono::duration_cast<std::chrono::milliseconds>(due-Clock::now())+std::chrono::milliseconds(1)));
        return !stopped;
    }
    static GstFlowReturn picture_sample(GstAppSink *sink,gpointer data){
        auto *p=static_cast<Impl*>(data);GstSample *sample=gst_app_sink_pull_sample(sink);
        if(!sample)return GST_FLOW_EOS;
        GstVideoInfo vi;gst_video_info_init(&vi);GstVideoFrame f{};
        if(!gst_video_info_from_caps(&vi,gst_sample_get_caps(sample))||!gst_video_frame_map(&f,&vi,gst_sample_get_buffer(sample),GST_MAP_READ)){
            gst_sample_unref(sample);p->fail("Kan camerabeeld of preview niet lezen.");return GST_FLOW_ERROR;
        }
        bool decoded=GST_VIDEO_INFO_FORMAT(&vi)==GST_VIDEO_FORMAT_RGB;
        int w=GST_VIDEO_INFO_WIDTH(&vi),h=GST_VIDEO_INFO_HEIGHT(&vi),bpp=decoded?3:4;
        Bytes pixels(size_t(w)*h*bpp);auto *src=static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&f,0));
        for(int y=0;y<h;++y)std::memcpy(pixels.data()+size_t(y)*w*bpp,src+y*GST_VIDEO_FRAME_PLANE_STRIDE(&f,0),w*bpp);
        gst_video_frame_unmap(&f);gst_sample_unref(sample);
        {std::lock_guard<std::mutex> lock(p->mutex);
            if(decoded){p->preview.width=w;p->preview.height=h;p->preview.rgb=std::move(pixels);++p->preview.serial;}
            else{p->raw=std::move(pixels);p->last_raw=Clock::now();}
        }return GST_FLOW_OK;
    }
    static GstFlowReturn encoded_sample(GstAppSink *sink,gpointer data){
        auto *p=static_cast<Impl*>(data);GstSample *sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        GstBuffer *b=gst_sample_get_buffer(sample);GstMapInfo map{};
        GstClockTime pts=gst_segment_to_running_time(gst_sample_get_segment(sample),GST_FORMAT_TIME,GST_BUFFER_PTS(b));
        if(pts==GST_CLOCK_TIME_NONE||!gst_buffer_map(b,&map,GST_MAP_READ)){
            gst_sample_unref(sample);p->fail("Encoder levert geen geldige tijdstempel/data.");return GST_FLOW_ERROR;
        }
        Unit u;u.pts=gst_util_uint64_scale(pts,90000,GST_SECOND);u.key=!GST_BUFFER_FLAG_IS_SET(b,GST_BUFFER_FLAG_DELTA_UNIT);
        u.bytes.assign(map.data,map.data+map.size);gst_buffer_unmap(b,&map);gst_sample_unref(sample);
        try{
            u.bytes=avc_for_transport(u.bytes);u.slice=first_slice(u.bytes);
            std::lock_guard<std::mutex> lock(p->mutex);
            require(u.pts>p->last_pts,"Encodertijdstempel loopt achteruit.");
            require(p->queues[0].size()<100&&p->queued_bytes+u.bytes.size()<2*1024*1024,"Encoderbuffer vol; uitvoer gestopt.");
            p->last_pts=u.pts;p->last_encoded=Clock::now();++p->info.frames;
            p->queued_bytes+=u.bytes.size();p->queues[0].push_back(std::move(u));
        }catch(const std::exception &e){p->fail(e.what());return GST_FLOW_ERROR;}
        return GST_FLOW_OK;
    }
    static void errors(GstElement *pipeline){
        GstBus *bus=gst_element_get_bus(pipeline);
        GstMessage *m=gst_bus_pop_filtered(bus,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));gst_object_unref(bus);
        if(!m)return;
        std::string msg="Camera gestopt of losgekoppeld; controleer de camera en start opnieuw.";
        if(GST_MESSAGE_TYPE(m)==GST_MESSAGE_ERROR){
            GError *e=nullptr;gchar *debug=nullptr;gst_message_parse_error(m,&e,&debug);
            if(e)msg=e->message;
            g_clear_error(&e);g_free(debug);
        }gst_message_unref(m);throw std::runtime_error(msg);
    }
    static GstElement *pipeline(const std::string &desc){
        GError *error=nullptr;GstElement *result=gst_parse_launch(desc.c_str(),&error);
        if(error){std::string msg=error->message;g_error_free(error);if(result)gst_object_unref(result);throw std::runtime_error(msg);}
        require(result,"Kan GStreamer-pipeline niet maken.");return result;
    }
    void connect_sink(GstElement *pipe,const char *name,GCallback callback){
        GstElement *sink=gst_bin_get_by_name(GST_BIN(pipe),name);require(sink,"Uitvoer ontbreekt in pipeline.");
        g_signal_connect(sink,"new-sample",callback,this);gst_object_unref(sink);
    }
    void process(Bytes &pixels){
        const int w=s.width,h=s.height();
        if(s.denoise){
            Bytes original=pixels;
            for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x)for(int c=0;c<3;++c){
                int sum=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)sum+=original[((y+dy)*w+x+dx)*4+c];
                size_t n=(y*w+x)*4+c;
                pixels[n]=s.denoise==1?(original[n]+sum/9)/2:sum/9;
            }
        }
        if(s.monochrome)for(size_t i=0;i<pixels.size();i+=4){
            int y=(29*pixels[i]+150*pixels[i+1]+77*pixels[i+2])>>8;pixels[i]=pixels[i+1]=pixels[i+2]=y;
        }
        cairo_surface_t *surface=cairo_image_surface_create_for_data(pixels.data(),CAIRO_FORMAT_RGB24,w,h,w*4);
        cairo_t *cr=cairo_create(surface);PangoLayout *layout=pango_cairo_create_layout(cr);
        cairo_font_options_t *options=cairo_font_options_create();
        cairo_font_options_set_antialias(options,CAIRO_ANTIALIAS_NONE);
        pango_cairo_context_set_font_options(pango_layout_get_context(layout),options);cairo_font_options_destroy(options);
        PangoFontDescription *font=pango_font_description_from_string("DejaVu Sans Bold");
        int size=std::max(14,w/18);pango_font_description_set_absolute_size(font,size*PANGO_SCALE);
        pango_layout_set_font_description(layout,font);pango_layout_set_text(layout,s.callsign.c_str(),-1);
        int tw,th;pango_layout_get_pixel_size(layout,&tw,&th);
        if(tw>w-8){pango_font_description_set_absolute_size(font,double(size)*(w-8)/tw*PANGO_SCALE);pango_layout_set_font_description(layout,font);pango_layout_get_pixel_size(layout,&tw,&th);}
        // A static full-width bar and larger, un-antialiased lettering survive
        // the severe quantization better than small text over moving scenery.
        int top=(h-th-6)/16*16;
        cairo_set_source_rgb(cr,0,0,0);cairo_rectangle(cr,0,top,w,h-top);cairo_fill(cr);
        cairo_set_source_rgb(cr,1,1,1);cairo_move_to(cr,4,top+(h-top-th)/2);pango_cairo_show_layout(cr,layout);
        pango_font_description_free(font);g_object_unref(layout);cairo_destroy(cr);cairo_surface_destroy(surface);
    }
    void run(){
        int fd=-1;FILE *file=nullptr;GstElement *src=nullptr;std::thread feeder;std::atomic<bool> feed_stop{false};
        try{
            validate(s);
            std::ostringstream desc;
            if(s.test_source){const char *patterns[]={"ball","smpte","snow"};desc<<"videotestsrc is-live=true pattern="<<patterns[s.pattern];}
            else desc<<"v4l2src device="<<s.device<<" do-timestamp=true ! decodebin";
            desc<<" ! videoconvert ! videoscale add-borders=true ! videorate ! video/x-raw,format=BGRx,width="<<s.width<<",height="<<s.height()<<",framerate="<<s.fps<<"/1,pixel-aspect-ratio=1/1 ! appsink name=raw sync=false emit-signals=true max-buffers=1 drop=true";
            input=pipeline(desc.str());connect_sink(input,"raw",G_CALLBACK(picture_sample));
            require(gst_element_set_state(input,GST_STATE_PLAYING)!=GST_STATE_CHANGE_FAILURE,"Camera kan niet starten; controleer apparaat en rechten.");
            auto deadline=Clock::now()+std::chrono::seconds(8);bool have_raw=false;
            while(!stopped){
                errors(input);{std::lock_guard<std::mutex> lock(mutex);have_raw=!raw.empty();}
                if(have_raw)break;
                require(Clock::now()<deadline,"Geen camerabeelden ontvangen binnen acht seconden.");until(Clock::now()+std::chrono::milliseconds(10));
            }
            if(!stopped){
                desc.str("");desc.clear();
                desc<<"appsrc name=video is-live=true format=time ! video/x-raw,format=BGRx,width="<<s.width<<",height="<<s.height()<<",framerate="<<s.fps<<"/1,pixel-aspect-ratio="<<(s.aspect==169?16:4)*s.height()<<"/"<<(s.aspect==169?9:3)*s.width
                    <<" ! videoconvert ! video/x-raw,format=I420 ! x264enc bitrate="<<video_budget(s)<<" key-int-max="<<s.gop
                    <<" speed-preset=medium tune=zerolatency threads=1 sliced-threads=false bframes=0 byte-stream=true aud=true vbv-buf-capacity=600 "
                    <<"option-string=scenecut=0:force-cfr=1 nal-hrd=none ! video/x-h264,profile=main,stream-format=byte-stream,alignment=au ! h264parse config-interval=-1 ! tee name=encoded "
                    <<"encoded. ! queue ! appsink name=ts sync=false emit-signals=true max-buffers=8 "
                    <<"encoded. ! queue ! avdec_h264 ! videoconvert ! video/x-raw,format=RGB ! appsink name=decoded sync=false emit-signals=true max-buffers=1 drop=true";
                encoder=pipeline(desc.str());connect_sink(encoder,"ts",G_CALLBACK(encoded_sample));connect_sink(encoder,"decoded",G_CALLBACK(picture_sample));
                src=gst_bin_get_by_name(GST_BIN(encoder),"video");
                require(gst_element_set_state(encoder,GST_STATE_PLAYING)!=GST_STATE_CHANGE_FAILURE,"Encoder kan niet starten.");
                feeder=std::thread([&]{
                    try{
                        auto epoch=Clock::now();uint64_t n=0;
                        while(!feed_stop&&!stopped){
                            uint64_t t=n*GST_SECOND/s.fps;if(!until(epoch+std::chrono::nanoseconds(t))||feed_stop)break;
                            Bytes pixels;
                            {std::lock_guard<std::mutex> lock(mutex);
                                require(Clock::now()-last_raw<std::chrono::seconds(3),"Camera levert geen nieuwe beelden meer; uitvoer gestopt.");pixels=raw;
                            }
                            process(pixels);require(gst_app_src_get_current_level_buffers(GST_APP_SRC(src))<4,"Encoder houdt cameraframes niet bij.");
                            GstBuffer *buffer=gst_buffer_new_allocate(nullptr,pixels.size(),nullptr);require(buffer,"Onvoldoende geheugen voor camerabeeld.");
                            gst_buffer_fill(buffer,0,pixels.data(),pixels.size());GST_BUFFER_PTS(buffer)=t;GST_BUFFER_DURATION(buffer)=(n+1)*GST_SECOND/s.fps-t;
                            require(gst_app_src_push_buffer(GST_APP_SRC(src),buffer)==GST_FLOW_OK,"Encoder weigert camerabeeld.");++n;
                        }
                    }catch(const std::exception &e){fail(e.what());}
                });
                bool ready=false;int64_t base=0;auto wait_start=Clock::now(),first=wait_start;
                while(!stopped){
                    errors(input);errors(encoder);
                    {std::lock_guard<std::mutex> lock(mutex);if(!ready&&!queues[0].empty()){
                        require(queues[0].front().key,"Encoder start niet met een sleutelbeeld.");ready=true;base=queues[0].front().pts;first=Clock::now();
                    }}
                    if(ready&&Clock::now()-first>=std::chrono::milliseconds(750))break;
                    require(Clock::now()-wait_start<std::chrono::seconds(8),"Encoder levert geen beelden.");until(Clock::now()+std::chrono::milliseconds(10));
                }
                if(!stopped){
                    if(!s.capture.empty()){
                        int out=open(s.capture.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);require(out>=0,"Kan opname niet maken; bestand bestaat al of map is niet schrijfbaar.");
                        file=fdopen(out,"wb");if(!file){close(out);throw std::runtime_error("Kan opname niet openen.");}
                    }else if(!s.preview_only){
                        fd=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);require(fd>=0,"Kan UDP-socket niet maken.");
                        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(s.port);inet_pton(AF_INET,s.ip.c_str(),&addr.sin_addr);
                        require(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0,"Kan UDP-bestemming niet instellen.");
                    }
                    static std::atomic<int> version{int(time(nullptr))&31};
                    Mux mux(dvb_bitrate(s.dvb),0,s.callsign,base,version++,false,true);
                    auto start=Clock::now();uint64_t count=0;bool finishing=false;uint8_t data[1316];
                    {std::lock_guard<std::mutex> lock(mutex);info.text=s.preview_only?"Lokale compressiepreview — UDP uit":file?"Lokale TS-opname — UDP uit":"Video verzenden naar "+s.ip+":"+std::to_string(s.port);}
                    while(!stopped){
                        errors(input);errors(encoder);
                        auto due=start+std::chrono::nanoseconds(ticks(count*1316,1000000000,dvb_bitrate(s.dvb)));
                        if(!until(due))break;
                        auto tolerance=std::chrono::nanoseconds(std::max<uint64_t>(5000000,ticks(1316,1000000000,dvb_bitrate(s.dvb))));
                        require(Clock::now()-due<=tolerance,"UDP-planning gemist door systeembelasting; uitvoer gestopt.");
                        {std::lock_guard<std::mutex> lock(mutex);
                            require(Clock::now()-last_encoded<std::chrono::seconds(3),"Encoder stilgevallen; uitvoer gestopt.");
                            for(int i=0;i<7;++i){
                                if(s.seconds&&mux.time90()>=uint64_t(s.seconds)*90000){finishing=true;queues[0].clear();}
                                mux.next(data+i*188,queues);
                            }
                            queued_bytes=0;for(const auto &u:queues[0])queued_bytes+=u.bytes.size();
                        }
                        if(file)require(fwrite(data,1,sizeof(data),file)==sizeof(data),"Schrijven van TS-opname mislukt.");
                        else if(fd>=0)require(send(fd,data,sizeof(data),0)==sizeof(data),"UDP-verzending mislukt; controleer zender, IP-adres en poort.");
                        ++count;{std::lock_guard<std::mutex> lock(mutex);info.datagrams=count;}
                        if(finishing&&!mux.pending())break;
                    }
                }
            }
        }catch(const std::exception &e){fail(e.what());}
        stopped=true;feed_stop=true;if(feeder.joinable())feeder.join();
        if(src)gst_object_unref(src);
        for(auto pipe:{input,encoder})if(pipe){gst_element_set_state(pipe,GST_STATE_NULL);gst_object_unref(pipe);}
        input=encoder=nullptr;
        if(fd>=0)close(fd);
        if(file&&fclose(file)!=0)fail("Sluiten van TS-opname mislukt.");
        std::lock_guard<std::mutex> lock(mutex);info.done=true;if(!info.failed)info.text="Gestopt";
    }
};
Engine::Engine(Settings s):p(new Impl(std::move(s))){p->worker=std::thread([this]{p->run();});}
Engine::~Engine(){stop();if(p->worker.joinable())p->worker.join();}
void Engine::stop(){p->stopped=true;}
Status Engine::status(){std::lock_guard<std::mutex> lock(p->mutex);return p->info;}
Picture Engine::picture(){std::lock_guard<std::mutex> lock(p->mutex);return p->preview;}
}
