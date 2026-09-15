#pragma once
#include "renderer.hpp"
#include "campaign.hpp"
#include "audio.hpp"
namespace neo {
class GameUi {
public:
    GameUi(Renderer&,Audio&,const std::filesystem::path&);
    bool campaign_event(Campaign&,const SDL_Event&);
    void campaign_draw(Campaign&);
    bool battle_event(Battle&,const SDL_Event&);
    void battle_draw(Battle&,bool paused);
    std::string battle_cursor(const Battle&,int,int,bool,bool) const;
    bool covers_battle(const Battle&,math3d::Vec3) const;
    bool dragging()const{return drag_unit>=0 && drag_moved;}
    bool valid_drop()const{return drag_valid;}
    void order_feedback(bool accepted){notice=accepted?"":"OUTSIDE DEPLOYMENT AREA";}
private:
    uint64_t mission_generation=~uint64_t(0),mission_voice_start=0;
    unsigned mission_head=0;bool mission_portrait=false;
    std::string mission_speaker;Pcm mission_speech;
    int hovered_command=-1;
    bool hovered_magic=false;
    void mission_dialogue(Battle&);
    std::string mission_clip(const Regiment&,unsigned);
    int drag_unit=-1,drag_start_x=0,drag_start_y=0;
    int facing_unit=-1;
    bool drag_moved=false,drag_valid=false;
    math3d::Vec3 drag_point{},drag_mouse{};
    void begin_drag(const Battle&,size_t,const SDL_Event&);
    void cancel_drag();
    void update_drag(const Battle&,int,int);
    void update_facing(Battle&,int,int);
    Renderer& renderer;Audio& audio;std::filesystem::path root;
    std::unique_ptr<Executable> executable;
    uint32_t shown_pc=0;size_t regiment=0,banner_page=0;
    bool book=false,spells=false,map=false;
    uint64_t speech_start=0;Pcm speech;
    std::string notice,last_map="Graphics/Maps/M1_ENG.BMP";
    std::vector<std::array<float,2>> travel;
    std::map<std::string,std::filesystem::path> paths;
    std::filesystem::path asset(const std::string&);
    std::string registry(unsigned);
    void image(const std::string&,unsigned,float,float,float=0,float=0);
    void button(float,float,float,const std::string&,bool=true);
    void wrapped(float,float,const std::string&,size_t,math3d::Vec3={.15f,.1f,.05f});
    void book_draw(const Campaign&);
    void map_draw(const Campaign&);
    std::vector<const Regiment*> roster(const Campaign&) const;
};
}
