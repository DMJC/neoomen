#pragma once
#include "movie.hpp"
#include "renderer.hpp"
namespace neo {
enum class FrontAction {None,NewCampaign,LoadCampaign,Continue,Quit,MovieDone};
class Frontend {
public:
    Frontend(SDL_Window*,Renderer&,const std::filesystem::path&,const std::filesystem::path&,bool mute);
    void menu(bool can_continue=false);
    void intro();
    void campaign_movie(const std::filesystem::path&);
    FrontAction event(const SDL_Event&);
    FrontAction draw();
    bool active()const{return mode!=Mode::Game;}
    bool colour_cursors=true;
    bool playing_movie()const{return mode==Mode::Movie;}
    bool muted()const{return mute;}
    void set_mute(bool value){mute=value;if(movie)movie->mute(value);}
    void error(const std::string& text){message=text;menu(can_continue);}
private:
    enum class Mode {Game,Movie,Menu,Options};
    Mode mode=Mode::Game;
    SDL_Window* window;Renderer& renderer;std::filesystem::path root,save;
    SpriteFrame background;std::unique_ptr<Movie> movie;
    uint64_t movie_start=0;
    int movie_number=0,selected=0;
    bool mute=false,can_continue=false,can_load=false;
    std::string message;
    void open_movie(const std::filesystem::path&,int sequence);
    FrontAction finish_movie(bool skipped);
    bool enabled(int)const;
    FrontAction choose(int);
};
}
