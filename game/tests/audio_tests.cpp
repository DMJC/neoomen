#include "audio.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);require(SDL_Init(SDL_INIT_AUDIO)==0,"SDL init");
    {neo::Audio audio;require(audio.available(),"dummy device");int16_t output[4096];
    neo::Pcm tone{2,std::vector<int16_t>(8192,10000)};audio.play(tone,44100);audio.mix(output,4096);require(output[100]==10000,"speech PCM");
    audio.mute(true);audio.mix(output,4096);require(std::all_of(std::begin(output),std::end(output),[](auto x){return x==0;}),"mute output");
    audio.mute(false);audio.mix(output,4096);require(output[100]==0,"mute advances playback");
    audio.play(tone,44100);audio.stop();audio.mix(output,4096);require(output[100]==0,"speech cancellation");
    bool rejected=false;try{audio.play(tone,0);}catch(...){rejected=true;}require(rejected,"invalid rate");
    if(argc>1){audio.configure(argv[1]);for(auto script:{"EERIE9","EERIE11","BATTLE1","BATTLE2"}){
        audio.music(script,"sDeploy");bool heard=false;for(int i=0;i<1500;++i){if(i==500)audio.music(script,"sNormal");if(i==1000)audio.music(script,"sEnd");audio.mix(output,4096);heard|=std::any_of(std::begin(output),std::end(output),[](auto x){return x!=0;});}require(heard,"original FSM music silent");
    }
    audio.music("","");audio.effect("BUTTON01");audio.mix(output,4096);require(std::any_of(std::begin(output),std::end(output),[](auto x){return x!=0;}),"original WAV effect");
    audio.music("EERIE9","sNormal");audio.play(tone,44100);audio.stop();audio.mix(output,4096);require(std::any_of(std::begin(output),std::end(output),[](auto x){return x!=0;}),"speech stop preserves music");
    }audio.update();audio.suspend(true);audio.suspend(false);
    }SDL_Quit();std::cout<<"Audio mixing, mute, speech isolation and music sequencing passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
