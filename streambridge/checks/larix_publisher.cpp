// Native test publisher using the SDP protocol spelling seen in Larix Android.
#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include <csignal>
#include <iostream>
static volatile sig_atomic_t stopped=0;
int main(int argc,char **argv){
    if(argc!=2)return 2;
    gst_init(nullptr,nullptr);signal(SIGINT,+[](int){stopped=1;});signal(SIGTERM,+[](int){stopped=1;});
    GError *error=nullptr;
    auto pipeline=gst_parse_launch("rtspclientsink name=sink protocols=tcp "
        "videotestsrc is-live=true pattern=snow ! video/x-raw,format=I420,width=640,height=480,framerate=25/1 ! "
        "x264enc tune=zerolatency bitrate=1800 key-int-max=25 ! video/x-h264,profile=high ! h264parse ! queue ! sink.sink_0 "
        "audiotestsrc is-live=true ! audio/x-raw,rate=44100,channels=1 ! fdkaacenc bitrate=64000 ! aacparse ! queue ! sink.sink_1",&error);
    if(error){std::cerr<<error->message<<'\n';g_error_free(error);if(pipeline)gst_object_unref(pipeline);return 1;}
    auto sink=gst_bin_get_by_name(GST_BIN(pipeline),"sink");g_object_set(sink,"location",argv[1],nullptr);
    g_signal_connect(sink,"update-sdp",G_CALLBACK(+[](GstElement*,GstSDPMessage *sdp,gpointer){
        for(guint i=0;i<gst_sdp_message_medias_len(sdp);++i){
            auto track=const_cast<GstSDPMedia*>(gst_sdp_message_get_media(sdp,i));
            gst_sdp_media_set_proto(track,"RTP/AVP/TCP");
        }
        auto text=gst_sdp_message_as_text(sdp);std::cout<<text<<std::flush;g_free(text);
    }),nullptr);gst_object_unref(sink);
    auto bus=gst_element_get_bus(pipeline);int result=0;
    if(gst_element_set_state(pipeline,GST_STATE_PLAYING)==GST_STATE_CHANGE_FAILURE)result=1;
    while(!stopped&&!result){
        auto msg=gst_bus_timed_pop_filtered(bus,100*GST_MSECOND,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));
        if(!msg)continue;
        if(GST_MESSAGE_TYPE(msg)==GST_MESSAGE_ERROR){GError *e=nullptr;gst_message_parse_error(msg,&e,nullptr);std::cerr<<e->message<<'\n';g_error_free(e);result=1;}
        gst_message_unref(msg);break;
    }
    gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(bus);gst_object_unref(pipeline);return result;
}
