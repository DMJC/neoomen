#include "audio.hpp"
#include "m3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
namespace neo {
Audio::Audio(){SDL_AudioSpec wanted{};wanted.freq=44100;wanted.format=AUDIO_S16SYS;wanted.channels=2;wanted.samples=1024;device=SDL_OpenAudioDevice(nullptr,0,&wanted,nullptr,0);if(device)SDL_PauseAudioDevice(device,0);else std::cerr<<"Audio unavailable: "<<SDL_GetError()<<'\n';}
Audio::~Audio(){if(device)SDL_CloseAudioDevice(device);}
std::vector<int16_t> Audio::convert(const Pcm& pcm,int rate){
    if(rate<4000 || rate>192000 || (pcm.channels!=1 && pcm.channels!=2) || pcm.samples.size()%pcm.channels)throw std::runtime_error("Invalid source audio format");
    if(pcm.samples.size()>size_t(INT32_MAX/2))throw std::runtime_error("Audio sample too large");
    std::unique_ptr<SDL_AudioStream,decltype(&SDL_FreeAudioStream)> stream(SDL_NewAudioStream(AUDIO_S16SYS,Uint8(pcm.channels),rate,AUDIO_S16SYS,2,44100),SDL_FreeAudioStream);
    if(!stream || SDL_AudioStreamPut(stream.get(),pcm.samples.data(),int(pcm.samples.size()*2))<0 || SDL_AudioStreamFlush(stream.get())<0)throw std::runtime_error(SDL_GetError());
    int n=SDL_AudioStreamAvailable(stream.get());if(n<0)throw std::runtime_error(SDL_GetError());std::vector<int16_t> result(size_t(n)/2);
    if(SDL_AudioStreamGet(stream.get(),result.data(),n)!=n)throw std::runtime_error(SDL_GetError());
    return result;
}
void Audio::play(const Pcm& pcm,int rate){speech={convert(pcm,rate),0};}
void Audio::stop(){speech={};}
void Audio::suspend(bool enabled){if(enabled==suspended)return;suspended=enabled;if(device){SDL_ClearQueuedAudio(device);SDL_PauseAudioDevice(device,enabled);}}
void Audio::effect(const std::string& name){
    if(root.empty() || effects.size()>=24)return;
    try{auto found=cache.find(name);if(found==cache.end()){
        auto path=m3d::resolve(root,"Sound/SOUND/"+name+".WAV");if(path.empty()){cache[name]={};return;}
        SDL_AudioSpec spec{};Uint8* bytes=nullptr;Uint32 length=0;if(!SDL_LoadWAV(path.c_str(),&spec,&bytes,&length))throw std::runtime_error(SDL_GetError());
        std::unique_ptr<Uint8,decltype(&SDL_FreeWAV)> owned(bytes,SDL_FreeWAV);
        SDL_AudioCVT cvt{};if(SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_S16SYS,2,44100)<0)throw std::runtime_error(SDL_GetError());
        std::vector<Uint8> buffer(size_t(length)*size_t(cvt.len_mult));std::copy(bytes,bytes+length,buffer.begin());cvt.buf=buffer.data();cvt.len=int(length);
        if(SDL_ConvertAudio(&cvt)<0)throw std::runtime_error(SDL_GetError());
        std::vector<int16_t> pcm(size_t(cvt.len_cvt)/2);std::memcpy(pcm.data(),buffer.data(),pcm.size()*2);found=cache.emplace(name,std::move(pcm)).first;
    }if(!found->second.empty())effects.push_back({found->second,0});}catch(const std::exception& e){cache[name]={};std::cerr<<"Sound "<<name<<": "<<e.what()<<'\n';}
}
void Audio::cue(){if(!root.empty()){effect("BUTTON01");return;}Pcm pcm{1,{}};for(int i=0;i<2205;++i)pcm.samples.push_back(int16_t(std::sin(i*6.2831853*660/44100)*1800*(1-i/2205.f)));if(effects.size()<24)effects.push_back({convert(pcm,44100),0});}
void Audio::music(const std::string& script,const std::string& state){
    state_name=state;if(script==script_name)return;script_name=script;track={};patterns.clear();samples.clear();sequence.clear();sequence_index=0;pattern_name.clear();
    if(script.empty() || root.empty())return;
    try{auto path=m3d::resolve(root,"Sound/Script/"+script+".FSM");auto bytes=read_file(path);std::istringstream lines(std::string(bytes.begin(),bytes.end()));std::string line,text;
        while(std::getline(lines,line)){line=line.substr(0,line.find('#'));for(char c:line){if(c=='{' || c=='}')text+=' ';text+=c;if(c=='{' || c=='}')text+=' ';}text+=' ';}
        std::istringstream input(text);std::string token;
        auto take=[&](){std::string value;if(!(input>>value))throw std::runtime_error("Truncated music FSM");return value;};
        auto expect=[&](const char* value){if(take()!=value)throw std::runtime_error("Invalid music FSM block");};
        while(input>>token){if(token=="state"){take();take();}else if(token=="start-state")take();else if(token=="start-pattern")pattern_name=take();else if(token=="sample"){auto file=take();samples[take()]=file;}else if(token=="pattern"){
            auto name=take();auto& p=patterns[name];expect("{");while((token=take())!="}"){auto kind=token;expect("{");if(kind=="sequence"){std::vector<std::string> s;while((token=take())!="}")s.push_back(token);p.sequences.push_back(std::move(s));}else if(kind=="state-table"){std::map<std::string,std::string> t;while((token=take())!="}")t[token]=take();p.transitions.push_back(std::move(t));}else throw std::runtime_error("Unknown music FSM block");}
        }else throw std::runtime_error("Unknown music FSM directive");}
    }catch(const std::exception& e){patterns.clear();std::cerr<<"Music "<<script<<": "<<e.what()<<'\n';}
}
void Audio::next_track(){
    for(int attempts=0;attempts<32;++attempts){
        if(sequence_index>=sequence.size()){
            if(!sequence.empty()){auto it=patterns.find(pattern_name);if(it==patterns.end() || it->second.transitions.empty())return;const auto& tables=it->second.transitions;const auto& table=tables[random()%tables.size()];auto next=table.find(state_name);if(next==table.end())next=table.find("default");if(next==table.end())return;pattern_name=next->second;}
            auto it=patterns.find(pattern_name);if(it==patterns.end() || it->second.sequences.empty()){patterns.clear();return;}const auto& choices=it->second.sequences;sequence=choices[random()%choices.size()];sequence_index=0;if(sequence.empty())return;
        }
        auto alias=sequence[sequence_index++];auto it=samples.find(alias);if(it==samples.end())continue;
        try{track={convert(decode_adpcm(read_file(m3d::resolve(root,"Sound/Music/"+it->second+".SAD")),2),22050),0};if(!track.pcm.empty())return;}catch(const std::exception& e){std::cerr<<"Music sample: "<<e.what()<<'\n';}
    }patterns.clear();
}
void Audio::mix(int16_t* output,size_t count){
    for(size_t i=0;i<count;++i){if(track.cursor>=track.pcm.size()) {track={};if(!patterns.empty())next_track();}
        const bool talking=speech.cursor<speech.pcm.size();int value=0;
        if(track.cursor<track.pcm.size())value+=int(track.pcm[track.cursor++]*(talking?.16f:.38f));
        if(talking)value+=speech.pcm[speech.cursor++];
        for(auto& v:effects)if(v.cursor<v.pcm.size())value+=int(v.pcm[v.cursor++]*.65f);
        output[i]=muted?0:int16_t(std::clamp(value,-32768,32767));
    }effects.erase(std::remove_if(effects.begin(),effects.end(),[](const Voice& v){return v.cursor==v.pcm.size();}),effects.end());
}
void Audio::update(){if(!device || suspended)return;while(SDL_GetQueuedAudioSize(device)<8192){int16_t buffer[2048];mix(buffer,2048);if(SDL_QueueAudio(device,buffer,sizeof buffer)<0)break;}}
}
