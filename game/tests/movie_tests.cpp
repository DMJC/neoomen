#include "movie.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: neoomen_movie_tests MOVIE");
    neo::Movie movie(argv[1],true,false);bool done=false;unsigned updates=0;
    for(;updates<30*600 && !done;++updates){done=movie.update(updates/30.);const auto& image=movie.frame();if(image.rgba.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid converted movie frame");}
    if(!done || movie.video_frames()<2 || !movie.audio_samples())throw std::runtime_error("Movie failed to finish with video and audio");
    std::cout<<movie.video_frames()<<" frames / "<<movie.audio_samples()<<" stereo PCM samples / "<<updates/30.<<" seconds\n";
    bool rejected=false;try{neo::Movie missing(std::string(argv[1])+".missing",true,false);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Missing movie accepted");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
