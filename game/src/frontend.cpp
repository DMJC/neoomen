#include "frontend.hpp"
#include <cstring>
#include <iostream>
namespace neo {
Frontend::Frontend(SDL_Window* w,Renderer& r,const std::filesystem::path& data,const std::filesystem::path& checkpoint,bool muted):window(w),renderer(r),root(data),save(checkpoint),mute(muted){
    auto file=m3d::resolve(root,"Graphics/Pictures/MAINMENU.BMP");
    if(!file.empty()){
        std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> source(SDL_LoadBMP(file.c_str()),SDL_FreeSurface);
        if(!source)throw std::runtime_error(SDL_GetError());
        std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> rgba(SDL_ConvertSurfaceFormat(source.get(),SDL_PIXELFORMAT_RGBA32,0),SDL_FreeSurface);
        if(!rgba)throw std::runtime_error(SDL_GetError());
        background={unsigned(rgba->w),unsigned(rgba->h),prj::Bytes(size_t(rgba->w)*rgba->h*4)};
        for(int y=0;y<rgba->h;++y)std::memcpy(background.rgba.data()+size_t(y)*rgba->w*4,static_cast<uint8_t*>(rgba->pixels)+y*rgba->pitch,size_t(rgba->w)*4);
    }
}
void Frontend::menu(bool resume){movie.reset();mode=Mode::Menu;can_continue=resume;can_load=std::filesystem::is_regular_file(save);selected=resume?6:0;}
void Frontend::open_movie(const std::filesystem::path& file,int sequence){movie.reset();movie=std::make_unique<Movie>(file,mute);movie_number=sequence;movie_start=SDL_GetPerformanceCounter();mode=Mode::Movie;std::cout<<"Playing "<<file.filename()<<'\n';}
void Frontend::intro(){message.clear();try{open_movie(m3d::resolve(root,"Movies/ENG.TGQ"),1);}catch(const std::exception& e){message="INTRO UNAVAILABLE - CONTINUE FROM MENU";std::cerr<<e.what()<<'\n';menu(can_continue);}}
void Frontend::campaign_movie(const std::filesystem::path& file){open_movie(file,3);}
bool Frontend::enabled(int index)const {return index==6?can_continue:index==0?!root.empty():index==1?can_load:index==4 || index==5;}
FrontAction Frontend::finish_movie(bool skipped){
    movie.reset();if(movie_number==3){mode=Mode::Game;return FrontAction::MovieDone;}
    if(movie_number==1 && !skipped){try{open_movie(m3d::resolve(root,"Movies/INTRO.TGQ"),2);return FrontAction::None;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';message="INTRO UNAVAILABLE - CONTINUE FROM MENU";}}
    menu(can_continue);return FrontAction::None;
}
FrontAction Frontend::choose(int index){
    if(!enabled(index))return FrontAction::None;
    message.clear();if(index==4){mode=Mode::Options;return FrontAction::None;}
    if(index==5)return FrontAction::Quit;
    mode=Mode::Game;
    return index==0?FrontAction::NewCampaign:index==1?FrontAction::LoadCampaign:FrontAction::Continue;
}
FrontAction Frontend::event(const SDL_Event& e){
    if(e.type==SDL_QUIT)return FrontAction::Quit;
    bool key=e.type==SDL_KEYDOWN && !e.key.repeat;
    if(mode==Mode::Movie){if(key && (e.key.keysym.sym==SDLK_ESCAPE || e.key.keysym.sym==SDLK_SPACE || e.key.keysym.sym==SDLK_RETURN))return finish_movie(true);if(key && e.key.keysym.sym==SDLK_m){mute=!mute;movie->mute(mute);}return FrontAction::None;}
    if(mode==Mode::Options){
        if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
            int w,h;SDL_GetWindowSize(window,&w,&h);float scale=std::min(w/640.f,h/480.f);
            if(scale>0){float x=(e.button.x-(w-640*scale)/2)/scale,y=(e.button.y-(h-480*scale)/2)/scale;
                if(x>=140 && x<=500){if(y>=240 && y<320)mute=!mute;else if(y>=320 && y<345)colour_cursors=!colour_cursors;else if(y>=345 && y<380)intro();else if(y>=400 && y<440)menu(can_continue);}}
        }
        if(key){if(e.key.keysym.sym==SDLK_m || e.key.keysym.sym==SDLK_RETURN)mute=!mute;else if(e.key.keysym.sym==SDLK_c)colour_cursors=!colour_cursors;else if(e.key.keysym.sym==SDLK_i)intro();else if(e.key.keysym.sym==SDLK_ESCAPE)menu(can_continue);}return FrontAction::None;}
    if(mode!=Mode::Menu)return FrontAction::None;
    if(key){auto k=e.key.keysym.sym;if(k==SDLK_ESCAPE){if(can_continue)return choose(6);return FrontAction::Quit;}
        if(k==SDLK_UP || k==SDLK_DOWN){do{selected=(selected+(k==SDLK_UP?6:1))%7;}while(!enabled(selected));}
        if(k==SDLK_RETURN || k==SDLK_SPACE)return choose(selected);
        if(k==SDLK_n)return choose(0);
        if(k==SDLK_l)return choose(1);
    }
    if(e.type==SDL_MOUSEMOTION || (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT)){
        int w,h;SDL_GetWindowSize(window,&w,&h);float scale=std::min(w/640.f,h/480.f);if(scale<=0)return FrontAction::None;
        float x=((e.type==SDL_MOUSEMOTION?e.motion.x:e.button.x)-(w-640*scale)/2)/scale;
        float y=((e.type==SDL_MOUSEMOTION?e.motion.y:e.button.y)-(h-480*scale)/2)/scale;
        for(int i=0;i<7;++i){float top=i==6?210.f:250.f+i*40;if(x>=160 && x<=480 && y>=top && y<top+28 && enabled(i)){selected=i;if(e.type==SDL_MOUSEBUTTONDOWN)return choose(i);}}
    }
    return FrontAction::None;
}
FrontAction Frontend::draw(){
    if(mode==Mode::Movie){try{bool ended=movie->update(double(SDL_GetPerformanceCounter()-movie_start)/SDL_GetPerformanceFrequency());renderer.screen_image(movie->frame());if(ended)return finish_movie(false);}catch(const std::exception& e){std::cerr<<"Movie playback: "<<e.what()<<'\n';return finish_movie(true);}return FrontAction::None;}
    renderer.screen_image(background);
    auto menu_font=[&](bool selected){return m3d::resolve(root,selected?"Graphics/Fonts/F_MENBGR.FNT":"Graphics/Fonts/F_MENBG.FNT");};
    if(mode==Mode::Options){renderer.screen_font_label(menu_font(true),250,mute?"SOUND: OFF":"SOUND: ON");renderer.screen_font_label(menu_font(false),300,"ENTER / M TO TOGGLE SOUND");renderer.screen_font_label(menu_font(false),330,colour_cursors?"C CURSORS: ORIGINAL COLOUR":"C CURSORS: SYSTEM");renderer.screen_font_label(menu_font(false),365,"I REPLAY INTRO");renderer.screen_font_label(menu_font(false),410,"ESC BACK");}
    else {static const char* labels[]={"NEW CAMPAIGN","LOAD CAMPAIGN","MULTIPLAYER","TUTORIAL","OPTIONS","QUIT","CONTINUE"};
        for(int i=0;i<7;++i){if(i==6 && !can_continue)continue;renderer.screen_font_label(menu_font(i==selected),i==6?210.f:250.f+i*40,labels[i]);}
        if(!message.empty())renderer.screen_label(185,message.substr(0,65),{1,.35f,.2f});
        else if(root.empty())renderer.screen_label(185,"START WITH --DATA GAME_ROOT TO PLAY",{1,.6f,.2f});
    }
    return FrontAction::None;
}
}
