#pragma once
#include "formats.hpp"
#include <SDL.h>
#include <map>
#include <random>
namespace neo {
// Main-thread mixer; only a short mixed PCM queue is submitted to SDL.
class Audio {
public:
    Audio(); ~Audio();
    Audio(const Audio&)=delete; Audio& operator=(const Audio&)=delete;
    void play(const Pcm&,int rate); // Replace speech only.
    void cue();
    void stop();
    bool speaking()const{return speech.cursor<speech.pcm.size();}
    void mute(bool enabled){muted=enabled;}
    void suspend(bool enabled);
    bool available() const {return device!=0;}
    void configure(const std::filesystem::path& data){root=data;}
    void effect(const std::string& name);
    void music(const std::string& script,const std::string& state);
    void update();
    void mix(int16_t* output,size_t samples); // Also used by deterministic mixer tests.
private:
    struct Voice {std::vector<int16_t> pcm;size_t cursor=0;};
    struct Pattern {std::vector<std::vector<std::string>> sequences;std::vector<std::map<std::string,std::string>> transitions;};
    SDL_AudioDeviceID device=0; bool muted=false,suspended=false;
    std::filesystem::path root;
    Voice speech,track;std::vector<Voice> effects;
    std::map<std::string,std::vector<int16_t>> cache;
    std::map<std::string,std::string> samples;
    std::map<std::string,Pattern> patterns;
    std::string script_name,state_name,pattern_name;
    std::vector<std::string> sequence;size_t sequence_index=0;
    std::mt19937 random{1};
    std::vector<int16_t> convert(const Pcm&,int);
    void next_track();
};
}
