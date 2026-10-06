/* Linux-only capture/encoding. Included by datv.cpp; no subprocess or shell.
 * Pulse's nonblocking mainloop is serviced by the paced UDP worker. */
extern "C" {
#include <pulse/pulseaudio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}
class LinuxAudio {
    pa_mainloop *loop=nullptr;
    pa_context *context=nullptr;
    pa_stream *capture=nullptr;
    pa_operation *lookup=nullptr, *load=nullptr;
    bool source_checked=false, source_found=false, module_requested=false, module_finished=false, rechecked=false;
    int source_error=0;
    uint32_t loaded_module=PA_INVALID_INDEX;
    static void source_info(pa_context *ctx, const pa_source_info *info, int end, void *user) {
        auto *self=static_cast<LinuxAudio *>(user);
        if (info) self->source_found=true;
        if (end) { self->source_checked=true; self->source_error=end<0?pa_context_errno(ctx):0; }
    }
    static void module_loaded(pa_context *, uint32_t index, void *user) {
        auto *self=static_cast<LinuxAudio *>(user);
        self->loaded_module=index; self->module_finished=true;
    }
    static void release_operation(pa_operation *&op) {
        if (op) { pa_operation_cancel(op); pa_operation_unref(op); op=nullptr; }
    }
    bool ensure_source(const char *source) {
        if (!source_checked) {
            if (!lookup) {
                lookup=pa_context_get_source_info_by_name(context,source,source_info,this);
                if (!lookup) throw std::runtime_error("Cannot look up the PulseAudio source.");
            }
            return false;
        }
        release_operation(lookup);
        if (source_found) { release_operation(load); return true; }
        if (std::strcmp(source,"ffmix.monitor") || source_error!=PA_ERR_NOENTITY)
            throw std::runtime_error("Audio source not found or unavailable. Check the source name.");
        if (!module_requested) {
            module_requested=true;
            load=pa_context_load_module(context,"module-null-sink",
                "sink_name=ffmix rate=48000 channels=2 sink_properties=device.description=ATV-audiomix",
                module_loaded,this);
            if (!load) throw std::runtime_error("Cannot create the ffmix audio sink.");
            return false;
        }
        if (!module_finished) return false;
        release_operation(load);
        if (rechecked && loaded_module==PA_INVALID_INDEX) throw std::runtime_error("Cannot create ffmix. Check audio-server permissions and module-null-sink support.");
        // Also handles a concurrent creator: regardless of module-load result,
        // only an exact, successfully looked-up monitor is usable.
        rechecked=true; source_checked=false; source_error=0;
        return false;
    }
    AVCodecContext *codec=nullptr;
    AVFrame *frame=nullptr;
    AVPacket *packet=nullptr;
    std::deque<float> pcm;
    int channels=0;
    int64_t samples=0;
    bool recording=false;
public:
    ~LinuxAudio() {
        // Cancelling suppresses callbacks into this object; it cannot guarantee
        // cancellation of the server-side load. ffmix intentionally persists.
        release_operation(lookup); release_operation(load);
        if (capture) { pa_stream_disconnect(capture); pa_stream_unref(capture); }
        if (context) { pa_context_disconnect(context); pa_context_unref(context); }
        if (loop) pa_mainloop_free(loop);
        av_packet_free(&packet); av_frame_free(&frame); avcodec_free_context(&codec);
    }
    void open(const AudioSettings &s) {
        channels=s.bitrate==48000?1:2;
        loop=pa_mainloop_new();
        if (!loop) throw std::runtime_error("Cannot create the PulseAudio mainloop.");
        context=pa_context_new(pa_mainloop_get_api(loop),"ATV contest audio");
        if (!context || pa_context_connect(context,nullptr,PA_CONTEXT_NOAUTOSPAWN,nullptr)<0)
            throw std::runtime_error("Cannot connect to PulseAudio/PipeWire.");
        const AVCodec *encoder=avcodec_find_encoder_by_name("aac");
        codec=avcodec_alloc_context3(encoder);
        frame=av_frame_alloc(); packet=av_packet_alloc();
        if (!encoder || !codec || !frame || !packet) throw std::runtime_error("AAC encoder is unavailable.");
        codec->sample_rate=48000; codec->sample_fmt=AV_SAMPLE_FMT_FLTP;
        codec->bit_rate=s.bitrate; codec->profile=AV_PROFILE_AAC_LOW;
        codec->time_base={1,48000}; av_channel_layout_default(&codec->ch_layout,channels);
        if (avcodec_open2(codec,encoder,nullptr)<0 || codec->frame_size!=1024)
            throw std::runtime_error("Cannot initialise the AAC-LC encoder.");
        frame->format=codec->sample_fmt; frame->sample_rate=48000; frame->nb_samples=1024;
        av_channel_layout_copy(&frame->ch_layout,&codec->ch_layout);
        if (av_frame_get_buffer(frame,0)<0) throw std::runtime_error("Cannot allocate audio samples.");
    }
    bool poll(const char *source) {
        for (int i=0;i<32;++i) {
            int result=pa_mainloop_iterate(loop,0,nullptr);
            if (result<0) throw std::runtime_error("PulseAudio mainloop failed.");
            if (!result) break;
        }
        auto state=pa_context_get_state(context);
        if (!PA_CONTEXT_IS_GOOD(state)) throw std::runtime_error("PulseAudio/PipeWire disconnected.");
        if (state!=PA_CONTEXT_READY) return false;
        if (!capture && !ensure_source(source)) return false;
        if (!capture) {
            pa_sample_spec spec={PA_SAMPLE_FLOAT32NE,48000,static_cast<uint8_t>(channels)};
            capture=pa_stream_new(context,"DATV AAC input",&spec,nullptr);
            pa_buffer_attr attr{static_cast<uint32_t>(48000*channels*4/2),UINT32_MAX,UINT32_MAX,UINT32_MAX,
                                static_cast<uint32_t>(48000*channels*4/100)};
            if (!capture || pa_stream_connect_record(capture,source,&attr,
                    static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY|PA_STREAM_DONT_MOVE))<0)
                throw std::runtime_error("Cannot open the audio source. Check its name (e.g. ffmix.monitor).");
        }
        auto stream_state=pa_stream_get_state(capture);
        if (!PA_STREAM_IS_GOOD(stream_state)) throw std::runtime_error("Audio source disconnected or unavailable. Check its name (e.g. ffmix.monitor).");
        if (stream_state!=PA_STREAM_READY) return false;
        // PipeWire may silently choose the default source for an unknown name,
        // even with DONT_MOVE. Never capture a different device than requested.
        const char *device=pa_stream_get_device_name(capture);
        if (!device || std::strcmp(device,source))
            throw std::runtime_error("The requested audio source is unavailable; refusing a different input.");
        for (int i=0;i<64;++i) {
            size_t available=pa_stream_readable_size(capture);
            if (available==size_t(-1)) throw std::runtime_error("Cannot read the audio source.");
            if (!available) break;
            const void *data=nullptr; size_t bytes=0;
            if (pa_stream_peek(capture,&data,&bytes)<0) throw std::runtime_error("Cannot read PulseAudio samples.");
            if (!bytes) break;
            const float *input=static_cast<const float *>(data);
            for (size_t n=0;n<bytes/sizeof(float);++n) pcm.push_back(input?input[n]:0.0f);
            if (pa_stream_drop(capture)<0) throw std::runtime_error("Cannot release PulseAudio samples.");
            if (!recording) while (pcm.size()>size_t(4800*channels)) pcm.pop_front();
            if (pcm.size()>size_t(24000*channels)) throw std::runtime_error("Audio capture overflow. Restart the stream.");
        }
        return pcm.size()>=size_t(4800*channels);
    }
    Bytes next() {
        // Prime only when audio starts, not before a possibly 10-second video
        // lead-in: the AAC overlap must use the same live PCM as the first frame.
        if (!samples) (void)encode_frame();
        return encode_frame();
    }
    Bytes encode_frame() {
        recording=true;
        /* Correct small independent capture/TS clock drift by consuming at most
         * one extra/fewer sample per block, using linear interpolation. Keep
         * about 80ms queued; never let source clock drift grow latency forever. */
        int available=static_cast<int>(pcm.size()/channels);
        int take=available>4800?1025:available<2880?1023:1024;
        if (available<take) throw std::runtime_error("Audio capture underrun. Restart the stream.");
        if (av_frame_make_writable(frame)<0) throw std::runtime_error("Cannot prepare AAC samples.");
        for (int c=0;c<channels;++c) {
            float *out=reinterpret_cast<float *>(frame->data[c]);
            for (int i=0;i<1024;++i) {
                double pos=double(i)*(take-1)/1023;
                int a=static_cast<int>(pos), b=std::min(a+1,take-1);
                out[i]=pcm[a*channels+c]+float(pos-a)*(pcm[b*channels+c]-pcm[a*channels+c]);
            }
        }
        for (int i=0;i<take*channels;++i) pcm.pop_front();
        frame->pts=samples; samples+=1024;
        if (avcodec_send_frame(codec,frame)<0) throw std::runtime_error("AAC encoding failed.");
        int result=avcodec_receive_packet(codec,packet);
        if (result==AVERROR(EAGAIN)) return {};
        if (result<0) throw std::runtime_error("Cannot receive the AAC frame.");
        Bytes encoded(packet->data,packet->data+packet->size); av_packet_unref(packet);
        return encoded;
    }
};
