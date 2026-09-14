#include "movie.hpp"
#include <SDL.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
namespace neo {
namespace {
void checked(int code,const char* action){if(code<0){char error[AV_ERROR_MAX_STRING_SIZE];av_strerror(code,error,sizeof(error));throw std::runtime_error(std::string(action)+": "+error);}}
}
struct Movie::Impl {
    AVFormatContext* file=nullptr;
    AVCodecContext *video=nullptr,*audio=nullptr;
    AVPacket* packet=nullptr;AVFrame* decoded=nullptr;
    SwsContext* scaler=nullptr;SwrContext* resampler=nullptr;
    SDL_AudioDeviceID device=0;
    int video_index=-1,audio_index=-1;
    bool eof=false,muted=false,started=false;
    double frame_duration=1./15,last_video=0,audio_end=0;
    uint64_t videos=0,samples=0;unsigned damaged_packets=0;
    struct TimedFrame {double pts;SpriteFrame image;};
    std::deque<TimedFrame> pending;
    SpriteFrame current;
    ~Impl(){if(device){SDL_ClearQueuedAudio(device);SDL_CloseAudioDevice(device);}swr_free(&resampler);sws_freeContext(scaler);av_frame_free(&decoded);av_packet_free(&packet);avcodec_free_context(&video);avcodec_free_context(&audio);avformat_close_input(&file);}
    AVCodecContext* codec(int index){
        auto* parameters=file->streams[index]->codecpar;const auto* decoder=avcodec_find_decoder(parameters->codec_id);
        if(!decoder)throw std::runtime_error("Movie codec unavailable");
        AVCodecContext* result=avcodec_alloc_context3(decoder);if(!result)throw std::bad_alloc();
        try{checked(avcodec_parameters_to_context(result,parameters),"Movie codec parameters");checked(avcodec_open2(result,decoder,nullptr),"Open movie codec");}catch(...){avcodec_free_context(&result);throw;}return result;
    }
    void queue_audio(const uint8_t* data,int count){
        samples+=unsigned(count);audio_end=double(samples)/44100;
        if(device && count){std::vector<uint8_t> silence;if(muted){silence.resize(size_t(count)*4);data=silence.data();}checked(SDL_QueueAudio(device,data,Uint32(count)*4),"Queue movie audio");}
    }
    void receive(AVCodecContext* codec,bool is_video){
        for(;;){int result=avcodec_receive_frame(codec,decoded);if(result==AVERROR(EAGAIN) || result==AVERROR_EOF)return;
            // ENG.TGQ contains one malformed EA audio packet; FFmpeg CLI also
            // discards it. Preserve the remaining movie instead of aborting startup.
            if(!is_video && result==AVERROR_INVALIDDATA && ++damaged_packets<=8){std::cerr<<"Skipping damaged movie audio packet\n";av_frame_unref(decoded);return;}
            checked(result,"Decode movie frame");
            if(is_video){
                if(decoded->width<=0 || decoded->height<=0 || decoded->width>4096 || decoded->height>4096)throw std::runtime_error("Invalid movie dimensions");
                SpriteFrame image{unsigned(decoded->width),unsigned(decoded->height),prj::Bytes(size_t(decoded->width)*decoded->height*4)};
                scaler=sws_getCachedContext(scaler,decoded->width,decoded->height,AVPixelFormat(decoded->format),decoded->width,decoded->height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
                if(!scaler)throw std::runtime_error("Movie color conversion unavailable");
                uint8_t* output[]={image.rgba.data()};int stride[]={decoded->width*4};
                checked(sws_scale(scaler,decoded->data,decoded->linesize,0,decoded->height,output,stride),"Convert movie video");
                double pts=decoded->best_effort_timestamp==AV_NOPTS_VALUE?videos*frame_duration:decoded->best_effort_timestamp*av_q2d(file->streams[video_index]->time_base);
                last_video=pts+frame_duration;pending.push_back({pts,std::move(image)});++videos;
            }else{
                if(!resampler){
                    AVChannelLayout input{};if(decoded->ch_layout.order==AV_CHANNEL_ORDER_UNSPEC)av_channel_layout_default(&input,decoded->ch_layout.nb_channels);else checked(av_channel_layout_copy(&input,&decoded->ch_layout),"Audio channel layout");
                    AVChannelLayout output=AV_CHANNEL_LAYOUT_STEREO;
                    int result=swr_alloc_set_opts2(&resampler,&output,AV_SAMPLE_FMT_S16,44100,&input,AVSampleFormat(decoded->format),decoded->sample_rate,0,nullptr);av_channel_layout_uninit(&input);checked(result,"Movie resampler");checked(swr_init(resampler),"Initialize movie resampler");
                }
                int capacity=swr_get_out_samples(resampler,decoded->nb_samples);checked(capacity,"Movie audio capacity");
                std::vector<uint8_t> output(size_t(capacity)*4);uint8_t* data=output.data();
                int count=swr_convert(resampler,&data,capacity,const_cast<const uint8_t**>(decoded->extended_data),decoded->nb_samples);checked(count,"Convert movie audio");queue_audio(data,count);
            }
            av_frame_unref(decoded);
        }
    }
    void send(AVCodecContext* codec,bool is_video,const AVPacket* data){int result=avcodec_send_packet(codec,data);if(result==AVERROR(EAGAIN)){receive(codec,is_video);result=avcodec_send_packet(codec,data);}checked(result,"Submit movie packet");receive(codec,is_video);}
    void read(){
        int result=av_read_frame(file,packet);
        if(result==AVERROR_EOF){eof=true;send(video,true,nullptr);if(audio)send(audio,false,nullptr);
            if(resampler){for(;;){uint8_t buffer[8192];uint8_t* p=buffer;int count=swr_convert(resampler,&p,2048,nullptr,0);checked(count,"Flush movie audio");queue_audio(p,count);if(!count)break;}}return;}
        checked(result,"Read movie packet");
        if(packet->stream_index==video_index)send(video,true,packet);else if(packet->stream_index==audio_index)send(audio,false,packet);
        av_packet_unref(packet);
    }
};
Movie::Movie(const std::filesystem::path& path,bool muted,bool output_audio):impl(std::make_unique<Impl>()){
    auto& m=*impl;m.muted=muted;checked(avformat_open_input(&m.file,path.c_str(),nullptr,nullptr),"Open movie");checked(avformat_find_stream_info(m.file,nullptr),"Read movie streams");
    m.video_index=av_find_best_stream(m.file,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);checked(m.video_index,"Find movie video");m.video=m.codec(m.video_index);
    auto rate=av_guess_frame_rate(m.file,m.file->streams[m.video_index],nullptr);if(rate.num>0 && rate.den>0)m.frame_duration=av_q2d(av_inv_q(rate));
    m.audio_index=av_find_best_stream(m.file,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
    if(m.audio_index>=0){m.audio=m.codec(m.audio_index);if(output_audio){SDL_AudioSpec desired{};desired.freq=44100;desired.format=AUDIO_S16SYS;desired.channels=2;desired.samples=512;m.device=SDL_OpenAudioDevice(nullptr,0,&desired,nullptr,0);if(!m.device)std::cerr<<"Movie audio unavailable: "<<SDL_GetError()<<'\n';}}
    m.packet=av_packet_alloc();m.decoded=av_frame_alloc();if(!m.packet || !m.decoded)throw std::bad_alloc();
}
Movie::~Movie()=default;
void Movie::mute(bool value){impl->muted=value;if(value && impl->device){prj::Bytes silence(SDL_GetQueuedAudioSize(impl->device));SDL_ClearQueuedAudio(impl->device);if(!silence.empty())checked(SDL_QueueAudio(impl->device,silence.data(),Uint32(silence.size())),"Mute movie audio");}}
bool Movie::update(double seconds){
    auto& m=*impl;
    // Demux only a short interval ahead of presentation; cap unusual streams too.
    for(unsigned packets=0;!m.eof && packets<256 && m.pending.size()<32 && (m.pending.empty() || m.pending.back().pts<seconds+.25);++packets)m.read();
    while(!m.pending.empty() && m.pending.front().pts<=seconds){m.current=std::move(m.pending.front().image);m.pending.pop_front();}
    if(!m.started){if(m.device)SDL_PauseAudioDevice(m.device,0);m.started=true;}
    return m.eof && m.pending.empty() && seconds>=std::max(m.last_video,m.audio_end) && (!m.device || SDL_GetQueuedAudioSize(m.device)==0);
}
const SpriteFrame& Movie::frame()const{return impl->current;}
uint64_t Movie::video_frames()const{return impl->videos;}
uint64_t Movie::audio_samples()const{return impl->samples;}
}
