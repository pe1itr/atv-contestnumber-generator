#pragma once
#include "engine.hpp"
#include "rtsp.hpp"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <sstream>

// The receiver never owns the output clock. Decoded source timestamps map onto
// a fresh local epoch for each connection; the output encoder runs continuously.
class LiveInput {
    using Clock=std::chrono::steady_clock;
    struct Video {int64_t time;std::vector<uint8_t> data;};
    struct Audio {int64_t sample;std::vector<uint8_t> data;};
    Settings s;Clock::time_point epoch;std::atomic<bool> stopping{false};std::thread worker;
    std::mutex lock;std::deque<Video> video;std::deque<Audio> audio;
    Video current{};std::vector<uint8_t> blue;
    int64_t origin=-1,play_origin=0,last_pts[2]={-1,-1};
    Clock::time_point last_video{};bool linked[2]={},retry=false;
    std::string message="Wachten op SRT";
    GstElement *pipeline=nullptr;
    struct Context {LiveInput *self;int media;} contexts[2]={{this,0},{this,1}};
    int64_t now90()const{return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-epoch).count()*9/100;}
    void reset(const std::string &reason){
        std::lock_guard<std::mutex> guard(lock);
        video.clear();audio.clear();current={};origin=-1;last_pts[0]=last_pts[1]=-1;last_video={};message=reason;retry=false;
    }
    static GstFlowReturn decoded(GstAppSink *sink,gpointer data){
        auto c=static_cast<Context*>(data);auto p=c->self;int a=c->media;
        GstSample *sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        GstBuffer *buffer=gst_sample_get_buffer(sample);
        GstClockTime time=gst_segment_to_running_time(gst_sample_get_segment(sample),GST_FORMAT_TIME,GST_BUFFER_PTS(buffer));
        GstMapInfo map{};
        if(time==GST_CLOCK_TIME_NONE||!gst_buffer_map(buffer,&map,GST_MAP_READ)){
            gst_sample_unref(sample);std::lock_guard<std::mutex> guard(p->lock);p->retry=true;p->message="Bron zonder tijdstempels";return GST_FLOW_ERROR;
        }
        std::vector<uint8_t> bytes(map.data,map.data+map.size);
        gst_buffer_unmap(buffer,&map);gst_sample_unref(sample);
        int64_t pts=gst_util_uint64_scale(time,90000,GST_SECOND);
        std::lock_guard<std::mutex> guard(p->lock);
        if(p->stopping)return GST_FLOW_FLUSHING;
        if((a==0&&bytes.size()!=size_t(p->s.width*p->s.height()*4))||(a==1&&bytes.size()%4)){
            p->retry=true;p->message="Onverwachte ruwe beeld-/geluidsindeling";return GST_FLOW_ERROR;
        }
        if(p->last_pts[a]>=0&&pts<p->last_pts[a]){
            p->retry=true;p->message="Tijdsprong in de bron; opnieuw verbinden";return GST_FLOW_ERROR;
        }
        p->last_pts[a]=pts;
        if(p->origin<0){p->origin=pts;p->play_origin=p->now90()+27000;} // 300 ms jitter buffer
        int64_t mapped=p->play_origin+pts-p->origin;
        if(mapped>p->now90()+180000||mapped<p->now90()-180000){
            p->retry=true;p->message="Bronklok loopt buiten de invoerbuffer";return GST_FLOW_ERROR;
        }
        if(a==0){
            p->video.push_back({mapped,std::move(bytes)});p->last_video=Clock::now();
            while(p->video.size()>64)p->video.pop_front();
        }else{
            p->audio.push_back({mapped*48000/90000,std::move(bytes)});
            size_t size=0;for(const auto &b:p->audio)size+=b.data.size();
            if(size>48000*4*2){p->retry=true;p->message="Audiobuffer van bron vol";return GST_FLOW_ERROR;}
        }
        return GST_FLOW_OK;
    }
    static void pad(GstElement*,GstPad *src,gpointer data){
        auto p=static_cast<LiveInput*>(data);GstCaps *caps=gst_pad_get_current_caps(src);if(!caps)return;
        auto st=gst_caps_get_structure(caps,0);const char *type=gst_structure_get_name(st);
        int a=g_str_has_prefix(type,"video/")?0:g_str_has_prefix(type,"audio/")?1:-1,version=0;
        gst_structure_get_int(st,"mpegversion",&version);
        bool supported=a==0?std::strcmp(type,"video/x-h264")==0:a==1&&std::strcmp(type,"audio/mpeg")==0&&version==4;
        gst_caps_unref(caps);
        if(a<0||(a==1&&!p->s.audio_kbps)||p->linked[a])return;
        if(!supported){std::lock_guard<std::mutex> guard(p->lock);p->retry=true;p->message="Bron vereist H.264 8-bit 4:2:0 en AAC";return;}
        GstElement *q=gst_bin_get_by_name(GST_BIN(p->pipeline),a?"audioq":"videoq");
        GstPad *dst=gst_element_get_static_pad(q,"sink");p->linked[a]=gst_pad_link(src,dst)==GST_PAD_LINK_OK;
        gst_object_unref(dst);gst_object_unref(q);
    }
    std::string video_output()const{
        std::ostringstream text;
        text<<"videoconvert ! videoscale add-borders=true ! videorate skip-to-first=true ! video/x-raw,format=BGRx,width="<<s.width<<",height="<<s.height()<<",framerate="<<s.fps<<"/1,pixel-aspect-ratio="<<s.pixel_aspect()<<" ! appsink name=video sync=false emit-signals=true max-buffers=2 drop=true ";
        return text.str();
    }
    static std::string audio_output(){return "audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved ! appsink name=audio sync=false emit-signals=true max-buffers=8";}
    void connect_sinks(GstElement *bin){
        for(int a=0;a<(s.audio_kbps?2:1);++a){
            auto sink=gst_bin_get_by_name(GST_BIN(bin),a?"audio":"video");
            if(sink){
                // RTP's jitterbuffer can release the startup backlog at once.
                // Pace decoded RTSP samples on their common pipeline clock so
                // this valid backlog cannot overflow the raw playout buffer.
                if(s.input==1)g_object_set(sink,"async",FALSE,"sync",TRUE,nullptr);
                g_signal_connect(sink,"new-sample",G_CALLBACK(decoded),&contexts[a]);gst_object_unref(sink);
            }
        }
    }
    void receive_rtsp(){
        while(!stopping){
            try{
                reset("Wachten op RTSP-publicatie");
                const std::string queue="queue max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! ";
                RtspReceiver receiver(s.rtsp_uri,s.latency,
                    queue+"h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! openh264dec ! "+video_output(),
                    s.audio_kbps?queue+"aacparse ! fdkaacdec ! "+audio_output():"fakesink sync=false async=false",
                    [this](GstElement *bin){connect_sinks(bin);},[this]{reset("Wachten op RTSP-publicatie");});
                while(!stopping){
                    receiver.poll();bool failed,idle;
                    {std::lock_guard<std::mutex> guard(lock);failed=retry;idle=Clock::now()-last_video>std::chrono::seconds(10);
                        if(failed)GST_WARNING("RTSP input reset: %s (audio PTS=%" G_GINT64_FORMAT ", video PTS=%" G_GINT64_FORMAT ")",message.c_str(),last_pts[1],last_pts[0]);}
                    receiver.expire_if_idle(idle);
                    if(failed)receiver.disconnect();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }catch(const std::exception &e){reset(std::string("RTSP: ")+e.what());}
            for(int i=0;i<25&&!stopping;++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    void receive(){
        if(s.input==1&&!s.test_source){receive_rtsp();return;}
        while(!stopping){
            GstBus *bus=nullptr;
            try{
                linked[0]=linked[1]=false;
                std::ostringstream text;
                if(s.test_source)text<<"videotestsrc is-live=true pattern=ball ! ";
                else text<<"srtsrc name=source ! tsdemux name=demux ignore-pcr=true queue name=videoq max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! openh264dec ! ";
                text<<video_output();
                if(s.audio_kbps)text<<(s.test_source?"audiotestsrc is-live=true ! ":"queue name=audioq max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! aacparse ! fdkaacdec ! ")<<audio_output();
                GError *error=nullptr;pipeline=gst_parse_launch(text.str().c_str(),&error);
                if(error){std::string reason=error->message;g_error_free(error);throw std::runtime_error(reason);}
                if(!pipeline)throw std::runtime_error("Kan invoer niet maken");
                if(!s.test_source){
                    auto src=gst_bin_get_by_name(GST_BIN(pipeline),"source");
                    g_object_set(src,"uri",s.uri.c_str(),"latency",s.latency,"auto-reconnect",FALSE,nullptr);gst_object_unref(src);
                    auto demux=gst_bin_get_by_name(GST_BIN(pipeline),"demux");g_signal_connect(demux,"pad-added",G_CALLBACK(pad),this);gst_object_unref(demux);
                }
                connect_sinks(pipeline);
                bus=gst_element_get_bus(pipeline);
                if(gst_element_set_state(pipeline,GST_STATE_PLAYING)==GST_STATE_CHANGE_FAILURE)throw std::runtime_error("Kan SRT-invoer niet starten");
                while(!stopping){
                    {std::lock_guard<std::mutex> guard(lock);if(retry)throw std::runtime_error(message);}
                    GstMessage *m=gst_bus_timed_pop_filtered(bus,20*GST_MSECOND,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));
                    if(!m)continue;
                    std::string reason="SRT-stream beëindigd";
                    if(GST_MESSAGE_TYPE(m)==GST_MESSAGE_ERROR){GError *e=nullptr;gchar *details=nullptr;gst_message_parse_error(m,&e,&details);reason=e->message;g_error_free(e);g_free(details);}
                    gst_message_unref(m);throw std::runtime_error(reason);
                }
            }catch(const std::exception &e){reset(e.what());}
            if(pipeline){gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(pipeline);pipeline=nullptr;}
            if(bus)gst_object_unref(bus);
            // Clear after NULL as well: a final decoded callback may race teardown.
            {std::lock_guard<std::mutex> guard(lock);video.clear();audio.clear();current={};origin=-1;last_pts[0]=last_pts[1]=-1;last_video={};retry=false;}
            for(int i=0;i<25&&!stopping;++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    bool present(int64_t time)const{
        return !current.data.empty()&&current.time<=time&&time-current.time<=90000&&Clock::now()-last_video<=std::chrono::seconds(1);
    }
public:
    LiveInput(Settings settings,Clock::time_point clock):s(std::move(settings)),epoch(clock){
        blue.resize(s.width*s.height()*4);
        auto surface=cairo_image_surface_create_for_data(blue.data(),CAIRO_FORMAT_RGB24,s.width,s.height(),s.width*4);
        auto cr=cairo_create(surface);cairo_set_source_rgb(cr,0,0,1);cairo_paint(cr);
        auto layout=pango_cairo_create_layout(cr);auto font=pango_font_description_new();
        pango_font_description_set_family(font,"Sans");pango_font_description_set_weight(font,PANGO_WEIGHT_BOLD);
        pango_font_description_set_absolute_size(font,std::max(12,s.height()/9)*PANGO_SCALE);
        pango_layout_set_font_description(layout,font);
        pango_layout_set_width(layout,(s.width-16)*PANGO_SCALE);pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
        pango_layout_set_single_paragraph_mode(layout,TRUE);
        const std::string lines[2]={"No Signal",s.callsign};PangoRectangle ink[2];
        for(int i=0;i<2;++i){
            pango_layout_set_text(layout,lines[i].c_str(),-1);pango_layout_get_pixel_extents(layout,&ink[i],nullptr);
        }
        const int gap=std::max(4,s.height()/30);
        double y=(s.height()-ink[0].height-ink[1].height-gap)/2.0;
        cairo_set_source_rgb(cr,1,1,1);
        for(int i=0;i<2;++i){
            pango_layout_set_text(layout,lines[i].c_str(),-1);
            cairo_move_to(cr,(s.width-ink[i].width)/2.0-ink[i].x,y-ink[i].y);
            pango_cairo_show_layout(cr,layout);y+=ink[i].height+gap;
        }
        pango_font_description_free(font);g_object_unref(layout);cairo_destroy(cr);cairo_surface_flush(surface);cairo_surface_destroy(surface);
        worker=std::thread([this]{receive();});
    }
    ~LiveInput(){stopping=true;if(worker.joinable())worker.join();}
    std::vector<uint8_t> picture(int64_t time){
        std::lock_guard<std::mutex> guard(lock);
        while(!video.empty()&&video.front().time<=time){current=std::move(video.front());video.pop_front();}
        return present(time)?current.data:blue;
    }
    std::vector<uint8_t> sound(int64_t start){
        std::lock_guard<std::mutex> guard(lock);std::vector<uint8_t> result(1024*4,0);
        bool live=present(start*90000/48000);
        while(!audio.empty()&&audio.front().sample+int64_t(audio.front().data.size()/4)<=start)audio.pop_front();
        if(live)for(const auto &b:audio){
            int64_t begin=std::max(start,b.sample),end=std::min(start+1024,b.sample+int64_t(b.data.size()/4));
            if(begin<end)std::memcpy(result.data()+(begin-start)*4,b.data.data()+(begin-b.sample)*4,(end-begin)*4);
        }
        return result;
    }
    std::string status(int64_t time){std::lock_guard<std::mutex> guard(lock);return present(time)?(s.input==1?"RTSP-beeld actief":"SRT-beeld actief"):"No Signal — "+message;}
};
