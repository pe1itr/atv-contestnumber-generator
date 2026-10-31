#pragma once
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/sdp/gstsdpmessage.h>
#include <functional>
#include <string>
#include <stdexcept>
#include <chrono>

// Larix describes interleaved RTP as RTP/AVP/TCP in SDP. GStreamer's
// default media handler accepts RTP/AVP here and negotiates TCP in SETUP.
// Normalize only this equivalent spelling, on a copy; retain every codec,
// payload, control and fmtp field and leave other profiles to the base handler.
struct BridgeRtspMedia {GstRTSPMedia parent;};
struct BridgeRtspMediaClass {GstRTSPMediaClass parent;};
G_DEFINE_TYPE(BridgeRtspMedia,bridge_rtsp_media,GST_TYPE_RTSP_MEDIA)
static void bridge_rtsp_media_init(BridgeRtspMedia*){}
static void bridge_rtsp_media_class_init(BridgeRtspMediaClass *klass){
    GST_RTSP_MEDIA_CLASS(klass)->handle_sdp=+[](GstRTSPMedia *media,GstSDPMessage *sdp)->gboolean{
        GstSDPMessage *copy=nullptr;
        if(gst_sdp_message_copy(sdp,&copy)!=GST_SDP_OK)return FALSE;
        for(guint i=0;i<gst_sdp_message_medias_len(copy);++i){
            auto track=const_cast<GstSDPMedia*>(gst_sdp_message_get_media(copy,i));
            if(g_strcmp0(gst_sdp_media_get_proto(track),"RTP/AVP/TCP")==0)gst_sdp_media_set_proto(track,"RTP/AVP");
        }
        auto parent=GST_RTSP_MEDIA_CLASS(bridge_rtsp_media_parent_class);
        gboolean ok=parent->handle_sdp(media,copy);gst_sdp_message_free(copy);return ok;
    };
}

// Choose depayloaders from ANNOUNCE's SDP before the server prepares media.
// pre-announce-request is emitted after construction, so it is too late to
// alter a normal factory's launch description there.
struct BridgeRtspFactory {GstRTSPMediaFactory parent;GstElement *(*build)(gpointer);gpointer data;};
struct BridgeRtspFactoryClass {GstRTSPMediaFactoryClass parent;};
G_DEFINE_TYPE(BridgeRtspFactory,bridge_rtsp_factory,GST_TYPE_RTSP_MEDIA_FACTORY)
static void bridge_rtsp_factory_init(BridgeRtspFactory*){}
static void bridge_rtsp_factory_class_init(BridgeRtspFactoryClass *klass){
    GST_RTSP_MEDIA_FACTORY_CLASS(klass)->create_element=+[](GstRTSPMediaFactory *f,const GstRTSPUrl*)->GstElement*{
        auto p=reinterpret_cast<BridgeRtspFactory*>(f);return p->build(p->data);
    };
}

// RECORD receiver. All client callbacks run on the private context (thread pool
// max-threads=0); only GStreamer media/streaming work uses additional threads.
// One publisher at a time; media reaches NULL before raw input is reset/reused.
class RtspReceiver {
    GMainContext *context=nullptr;
    GstRTSPServer *server=nullptr;
    GstRTSPMediaFactory *factory=nullptr;
    GSource *source=nullptr;
    GstRTSPClient *owner=nullptr; // borrowed while connected
    GstRTSPMedia *media=nullptr; // owned until close/stop
    std::string path,video_chain,audio_chain;
    std::function<void(GstElement*)> configure;
    std::function<void()> reset;
    std::chrono::steady_clock::time_point connected,cleaned=std::chrono::steady_clock::now();
    static void close_client(GstRTSPClient *client){
        // Closing destroys the watch and emits callbacks which can drop the
        // server's last reference. Keep the client alive until close returns
        // and releases its internal watch mutex.
        g_object_ref(client);
        gst_rtsp_client_close(client);
        g_object_unref(client);
    }
    void clear_media(){
        if(media){
            gst_rtsp_media_unprepare(media);
            auto bin=gst_rtsp_media_get_element(media);
            gst_element_set_state(bin,GST_STATE_NULL);gst_object_unref(bin);
            g_object_unref(media);media=nullptr;
        }
        reset();
    }
    static void closed(GstRTSPClient *client,gpointer data){
        auto p=static_cast<RtspReceiver*>(data);
        if(p->owner==client){p->owner=nullptr;p->clear_media();}
    }
    static GstElement *construct(gpointer data){
        auto ctx=gst_rtsp_context_get_current();
        if(!ctx||!ctx->request||ctx->method!=GST_RTSP_ANNOUNCE)return nullptr;
        auto client=ctx->client;
        auto p=static_cast<RtspReceiver*>(data);
        if(p->owner)return nullptr;
        if(!ctx->uri||!ctx->uri->abspath||p->path!=ctx->uri->abspath)return nullptr;
        guint8 *body=nullptr;guint size=0;GstSDPMessage *sdp=nullptr;
        gst_rtsp_message_get_body(ctx->request,&body,&size);gst_sdp_message_new(&sdp);
        if(!body||size>65536||gst_sdp_message_parse_buffer(body,size,sdp)!=GST_SDP_OK){
            gst_sdp_message_free(sdp);return nullptr;
        }
        std::string launch="( ";bool video=false,audio=false,valid=true;
        const guint count=gst_sdp_message_medias_len(sdp);
        if(count<1||count>2)valid=false;
        for(guint i=0;valid&&i<count;++i){
            auto m=gst_sdp_message_get_media(sdp,i);
            const char *kind=gst_sdp_media_get_media(m);
            const char *format=gst_sdp_media_get_format(m,0);
            gchar *end=nullptr;guint64 payload=format?g_ascii_strtoull(format,&end,10):128;
            bool payload_ok=format&&*format&&end&&!*end&&payload<=127;
            GstCaps *caps=payload_ok?gst_sdp_media_get_caps_from_media(m,int(payload)):nullptr;
            const char *encoding=caps?gst_structure_get_string(gst_caps_get_structure(caps,0),"encoding-name"):nullptr;
            std::string depay;
            if(kind&&encoding&&std::string(kind)=="video"&&!video&&!g_ascii_strcasecmp(encoding,"H264")){
                video=true;depay="rtph264depay name=depay"+std::to_string(i)+" ! "+p->video_chain;
            }else if(kind&&encoding&&std::string(kind)=="audio"&&!audio&&(!g_ascii_strcasecmp(encoding,"MPEG4-GENERIC")||!g_ascii_strcasecmp(encoding,"MP4A-LATM"))){
                audio=true;depay=std::string(!g_ascii_strcasecmp(encoding,"MP4A-LATM")?"rtpmp4adepay":"rtpmp4gdepay")+" name=depay"+std::to_string(i)+" ! "+p->audio_chain;
            }else valid=false;
            if(caps)gst_caps_unref(caps);
            launch+=depay+" ";
        }
        gst_sdp_message_free(sdp);
        if(!valid||!video)return nullptr;
        launch+=")";GError *error=nullptr;
        auto bin=gst_parse_bin_from_description(launch.c_str(),FALSE,&error);
        if(error){g_error_free(error);if(bin)gst_object_unref(bin);return nullptr;}
        if(!bin)return nullptr;
        p->owner=client;p->connected=std::chrono::steady_clock::now();
        return bin;
    }
    static void connected_client(GstRTSPServer*,GstRTSPClient *client,gpointer data){
        g_signal_connect(client,"closed",G_CALLBACK(closed),data);
        // A completed publication releases its slot even if TCP stays open.
        g_signal_connect(client,"teardown-request",G_CALLBACK(+[](GstRTSPClient *c,GstRTSPContext*,gpointer d){closed(c,d);}),data);
    }
    static void configured(GstRTSPMediaFactory*,GstRTSPMedia *media,gpointer data){
        auto p=static_cast<RtspReceiver*>(data);
        p->clear_media();p->media=GST_RTSP_MEDIA(g_object_ref(media));
        gst_rtsp_media_set_stop_on_disconnect(media,TRUE);
        auto bin=gst_rtsp_media_get_element(media);p->configure(bin);gst_object_unref(bin);
    }
public:
    RtspReceiver(const std::string &uri,int latency,std::string video,std::string audio,
                 std::function<void(GstElement*)> setup,std::function<void()> clear)
        :video_chain(std::move(video)),audio_chain(std::move(audio)),configure(std::move(setup)),reset(std::move(clear)){
        GUri *u=g_uri_parse(uri.c_str(),G_URI_FLAGS_NONE,nullptr);
        path=g_uri_get_path(u);std::string host=g_uri_get_host(u),port=std::to_string(g_uri_get_port(u));g_uri_unref(u);
        context=g_main_context_new();g_main_context_push_thread_default(context);
        server=gst_rtsp_server_new();gst_rtsp_server_set_content_length_limit(server,65536);gst_rtsp_server_set_address(server,host.c_str());gst_rtsp_server_set_service(server,port.c_str());
        auto pool=gst_rtsp_server_get_thread_pool(server);gst_rtsp_thread_pool_set_max_threads(pool,0);g_object_unref(pool);
        auto custom=reinterpret_cast<BridgeRtspFactory*>(g_object_new(bridge_rtsp_factory_get_type(),nullptr));
        custom->build=construct;custom->data=this;factory=GST_RTSP_MEDIA_FACTORY(custom);gst_rtsp_media_factory_set_transport_mode(factory,GST_RTSP_TRANSPORT_MODE_RECORD);
        gst_rtsp_media_factory_set_media_gtype(factory,bridge_rtsp_media_get_type());
        gst_rtsp_media_factory_set_protocols(factory,GST_RTSP_LOWER_TRANS_TCP);
        gst_rtsp_media_factory_set_latency(factory,latency);
        gst_rtsp_media_factory_set_stop_on_disconnect(factory,TRUE);
        g_signal_connect(factory,"media-configure",G_CALLBACK(configured),this);
        auto mounts=gst_rtsp_server_get_mount_points(server);
        gst_rtsp_mount_points_add_factory(mounts,path.c_str(),GST_RTSP_MEDIA_FACTORY(g_object_ref(factory)));g_object_unref(mounts);
        g_signal_connect(server,"client-connected",G_CALLBACK(connected_client),this);
        GError *error=nullptr;source=gst_rtsp_server_create_source(server,nullptr,&error);
        if(!source){
            std::string reason=error?error->message:"RTSP-poort niet beschikbaar";g_clear_error(&error);
            g_object_unref(factory);g_object_unref(server);g_main_context_pop_thread_default(context);g_main_context_unref(context);
            throw std::runtime_error(reason);
        }
        g_source_attach(source,context);
    }
    ~RtspReceiver(){
        g_source_destroy(source);g_source_unref(source);
        gst_rtsp_server_client_filter(server,+[](GstRTSPServer*,GstRTSPClient *c,gpointer p){
            g_signal_handlers_disconnect_by_data(c,p);close_client(c);return GST_RTSP_FILTER_REMOVE;
        },this);
        owner=nullptr;clear_media();
        auto sessions=gst_rtsp_server_get_session_pool(server);
        gst_rtsp_session_pool_filter(sessions,+[](GstRTSPSessionPool*,GstRTSPSession*,gpointer){return GST_RTSP_FILTER_REMOVE;},nullptr);g_object_unref(sessions);
        g_signal_handlers_disconnect_by_data(factory,this);g_object_unref(factory);g_object_unref(server);
        g_main_context_pop_thread_default(context);g_main_context_unref(context);
    }
    void poll(){
        for(int i=0;i<32&&g_main_context_pending(context);++i)g_main_context_iteration(context,FALSE);
        auto now=std::chrono::steady_clock::now();
        if(now-cleaned>std::chrono::seconds(1)){
            auto sessions=gst_rtsp_server_get_session_pool(server);gst_rtsp_session_pool_cleanup(sessions);g_object_unref(sessions);cleaned=now;
        }
    }
    void expire_if_idle(bool idle){
        if(owner&&idle&&std::chrono::steady_clock::now()-connected>std::chrono::seconds(10))disconnect();
    }
    void disconnect(){if(owner)close_client(owner);}
};
