#include "game_ui.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace math3d;
namespace neo {
namespace {
const math3d::Vec3 ink{.15f,.09f,.035f},gold{1,.8f,.35f},dim{.35f,.35f,.35f};
bool hit(math3d::Vec3 p,float x,float y,float w,float h){return p.x>=x && p.x<x+w && p.y>=y && p.y<y+h;}
struct HudRect {float x,y,w,h;bool contains(Vec3 p)const{return hit(p,x,y,w,h);}};
// The original mage controls use a five-slot strip: four spells and one item.
constexpr HudRect magic_button{224,354,190,62};
constexpr HudRect command_button(unsigned i){return {510+float(i%2)*62,362+float(i/2)*57,58,54};}
}
GameUi::GameUi(Renderer& r,Audio& a,const std::filesystem::path& data):renderer(r),audio(a),root(data){if(!root.empty())executable=std::make_unique<Executable>(m3d::resolve(root,"PRG_ENG/DarkOmen.exe"));}
std::filesystem::path GameUi::asset(const std::string& name){auto found=paths.find(name);if(found!=paths.end())return found->second;std::string path=name;std::replace(path.begin(),path.end(),'\\','/');for(const auto& p:std::vector<std::pair<std::string,std::string>>{{"[PICTURES]","Graphics/Pictures"},{"[MAPS]","Graphics/Maps"},{"[SPRITES]","Graphics/Sprites"},{"[BOOKS]","Graphics/Books"},{"[GAMEFLOW]","GameData/GameFlow"}})if(path.rfind(p.first,0)==0)path=p.second+path.substr(p.first.size());return paths.emplace(name,root.empty()?std::filesystem::path{}:m3d::resolve(root,path)).first->second;}
std::string GameUi::registry(unsigned id){return executable && id>0 && id<253?executable->string(0x4ceb7c+(id-1)*44+4):"";}
void GameUi::image(const std::string& path,unsigned frame,float x,float y,float w,float h){auto file=asset(path);if(!file.empty())renderer.ui_image(file,frame,x,y,w,h);}
void GameUi::button(float x,float y,float w,const std::string& text,bool enabled){renderer.ui_rect(x,y,w,20,enabled?math3d::Vec3{.24f,.16f,.08f}:math3d::Vec3{.13f,.13f,.13f});renderer.ui_text(x+6,y+6,text,enabled?gold:dim);}
void GameUi::wrapped(float x,float y,const std::string& text,size_t columns,math3d::Vec3 color){std::string line=text;while(!line.empty()){size_t n=std::min(columns,line.size());if(n<line.size()){auto space=line.rfind(' ',n);if(space>0 && space!=std::string::npos)n=space;}renderer.ui_text(x,y,line.substr(0,n),color);y+=12;line.erase(0,n);while(!line.empty() && line[0]==' ')line.erase(0,1);}}
std::vector<const Regiment*> GameUi::roster(const Campaign& c)const{std::vector<const Regiment*> result;for(const auto& r:c.army.regiments)if(r.status&1)result.push_back(&r);return result;}
bool GameUi::campaign_event(Campaign& c,const SDL_Event& e){
    if(c.state==CampaignState::Mission || e.type==SDL_QUIT)return false;
    bool key=e.type==SDL_KEYDOWN && !e.key.repeat;bool click=e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT;auto p=click?renderer.ui_mouse(float(e.button.x),float(e.button.y)):math3d::Vec3{};
    if(key && (e.key.keysym.sym==SDLK_F5 || e.key.keysym.sym==SDLK_F9))return false;
    if(key && e.key.keysym.sym==SDLK_ESCAPE){if(book || map){book=map=false;return true;}return false;}
    if(key && e.key.keysym.sym==SDLK_b){audio.effect("PAPER1");book=!book;map=false;return true;}
    if(key && e.key.keysym.sym==SDLK_m){map=!map;book=false;return true;}
    bool showing_book=book || c.presentation.screen==CampaignScreen::Book;
    if(showing_book){auto units=roster(c);if((key && e.key.keysym.sym==SDLK_LEFT)||(click && hit(p,30,438,90,22))){audio.effect("PAPER1");if(!units.empty())regiment=(regiment+units.size()-1)%units.size();return true;}
        if((key && e.key.keysym.sym==SDLK_RIGHT)||(click && hit(p,125,438,90,22))){audio.effect("PAPER1");if(!units.empty())regiment=(regiment+1)%units.size();return true;}
        if((key && e.key.keysym.sym==SDLK_s)||(click && hit(p,340,438,125,22))){spells=!spells;return true;}
    }
    if(click && hit(p,20,450,110,20) && !showing_book){book=true;map=false;return true;}
    if(click && hit(p,145,450,110,20) && !showing_book){map=true;book=false;return true;}
    bool advance=(key && (e.key.keysym.sym==SDLK_RETURN || e.key.keysym.sym==SDLK_SPACE)) || (click && hit(p,490,438,125,32));
    if(advance && (book || map)){book=map=false;return true;}
    if(c.state==CampaignState::Choice){
        int choice=-1;if(key && e.key.keysym.sym==SDLK_1)choice=0;if(key && e.key.keysym.sym==SDLK_2)choice=1;
        if(click && hit(p,30,390,580,20))choice=0;
        if(click && hit(p,30,414,580,20))choice=1;
        if(choice>=0){audio.cue();audio.stop();c.answer(unsigned(choice));return true;}
        return key || click;
    }
    if(advance){audio.cue();audio.stop();c.answer();return true;}
    return key || click;
}
void GameUi::book_draw(const Campaign& c){
    image("Graphics/Books/BIGBLACK.BMP",0,0,0,640,480);auto units=roster(c);
    renderer.ui_text(45,35,spells?"MAGIC ITEMS":"TROOP ROSTER",ink,1.4f);renderer.ui_text(365,35,"COFFERS: "+std::to_string(c.army.gold)+" GOLD",ink);
    if(!units.empty()){
        regiment%=units.size();const auto& r=*units[regiment];wrapped(40,64,r.name.empty()?registry(r.sprite):r.name,38);
        std::string banner=registry(r.banner),troop=banner.size()>2?banner.substr(2):registry(r.sprite);
        image("Graphics/Books/Troops/T_"+troop+".BMP",0,48,105,240,180);
        image("Graphics/Banners/"+banner+".SPR",0,45,298,36,55);
        wrapped(90,300,r.leader.empty()?r.name:r.leader,33);renderer.ui_text(90,333,"STRENGTH "+std::to_string(r.alive)+" / "+std::to_string(r.maximum),ink);
        if(!spells){const char* labels[]={"MOVE","WEAPON SKILL","BALLISTIC SKILL","STRENGTH","TOUGHNESS","WOUNDS","INITIATIVE","ATTACKS","LEADERSHIP"};for(unsigned i=0;i<9;++i){renderer.ui_text(350,90+i*22,labels[i],ink);renderer.ui_text(565,90+i*22,std::to_string(r.stats[i]),ink);}renderer.ui_text(350,302,"ARMOUR "+std::to_string(r.armour),ink);renderer.ui_text(350,322,"EXPERIENCE "+std::to_string(r.experience),ink);}
        else {renderer.ui_text(350,90,r.wizard?"WIZARD EQUIPMENT":"REGIMENT EQUIPMENT",ink);renderer.ui_text(350,150,"EQUIPPED ITEMS",ink);
            for(unsigned i=0;i<r.items.size();++i)if(r.items[i]!=65535){image("Graphics/Sprites/MAG_ITEM.SPR",r.items[i],350+i*70,170,48,48);}
            renderer.ui_text(350,255,"SHARED INVENTORY",ink);unsigned slot=0;for(auto item:c.magic_items()){image("Graphics/Sprites/MAG_ITEM.SPR",item,350+(slot%5)*45,273+(slot/5)*40,32,32);if(++slot==15)break;}
        }
        renderer.ui_text(45,388,"REGIMENT "+std::to_string(regiment+1)+" OF "+std::to_string(units.size()),ink);
    }
    button(30,438,90,"PREVIOUS");button(125,438,90,"NEXT");button(340,438,125,spells?"REGIMENT STATS":"MAGIC ITEMS");button(490,438,125,book?"BACK":"CONTINUE");
}
void GameUi::map_draw(const Campaign& c){
    const auto& p=c.presentation;std::string background=p.screen==CampaignScreen::Map?p.background:last_map;
    image(background,0,0,0,640,480);
    if(p.screen==CampaignScreen::Map)for(const auto& m:p.markers)image(p.map_icons,unsigned(m[0]),float(m[1]),float(m[2]));
    float visible=float(SDL_GetTicks64()-speech_start)*.04f,walked=0;
    for(size_t i=1;i<travel.size();++i){auto a=travel[i-1],b=travel[i];float length=std::hypot(b[0]-a[0],b[1]-a[1]);for(float step=0;step<length;step+=6){float x=a[0]+(b[0]-a[0])*step/length,y=a[1]+(b[1]-a[1])*step/length;if(walked+step<=visible)renderer.ui_rect(x-1.5f,y-1.5f,3,3,{1,.8f,.15f});}walked+=length;}
    renderer.ui_rect(20,18,600,25,{.12f,.09f,.04f});renderer.ui_text(30,26,"CAMPAIGN MAP / "+std::to_string(c.completed)+" BATTLES COMPLETED",gold);
    button(20,450,110,"ARMY BOOK");button(490,438,125,map?"BACK":"CONTINUE");
}
void GameUi::campaign_draw(Campaign& c){
    if(shown_pc!=c.pc()){
        shown_pc=c.pc();speech={};audio.stop();speech_start=SDL_GetTicks64();
        if(last_map=="Graphics/Maps/M1_ENG.BMP" && !c.presentation.dot_file.empty()){auto name=c.presentation.dot_file;for(auto& ch:name)ch=char(std::toupper(static_cast<unsigned char>(ch)));auto at=name.find("CH");if(at!=std::string::npos && at+2<name.size() && name[at+2]>='1' && name[at+2]<='4')last_map="Graphics/Maps/M"+name.substr(at+2,1)+"_ENG.BMP";}
        if(c.presentation.screen==CampaignScreen::Map){last_map=c.presentation.background;travel.clear();auto dot=asset(c.presentation.dot_file);if(!dot.empty())travel=decode_travel_route(read_file(dot),c.presentation.route);}
        if(!c.presentation.speech.empty())try{auto file=asset("Sound/SP_ENG/"+c.presentation.speech+".mad");if(!file.empty()){speech=decode_adpcm(read_file(file),1);audio.play(speech,22050);}}catch(const std::exception& e){std::cerr<<"Dialogue audio: "<<e.what()<<'\n';}
    }
    renderer.ui_begin(true);
    if(book || c.presentation.screen==CampaignScreen::Book){book_draw(c);return;}
    if(map || c.presentation.screen==CampaignScreen::Map){map_draw(c);return;}
    auto background=c.presentation.background;if(background.empty() || background.find("MAPS")!=std::string::npos)background="Graphics/Pictures/M_EMPC.BMP";
    image(background,0,0,0,640,480);
    if(c.presentation.screen==CampaignScreen::TalkingHead){
        double seconds=(SDL_GetTicks64()-speech_start)/1000.;float mouth=0;size_t at=size_t(seconds*22050);
        for(size_t i=at;i<std::min(at+256,speech.samples.size());++i)mouth+=std::abs(float(speech.samples[i]));
        mouth=std::min(1.f,mouth/(256*6000.f));
        unsigned head=c.presentation.head;if(head>=5000)head-=5000;
        renderer.portrait(root,head,55,92,250,278,mouth,seconds);
        renderer.ui_rect(325,140,275,120,{.08f,.06f,.035f});
        std::string speaker="CONVERSATION";
        if(executable)for(const auto& r:c.army.regiments)if(r.head>0 && r.head<253 && executable->word(0x4ceb7c+(r.head-1)*44+40)==head && !r.leader.empty()){speaker=r.leader;break;}
        wrapped(338,160,speaker,40,gold);renderer.ui_text(338,205,"SPACE OR ENTER TO CONTINUE",gold);

    }else {renderer.ui_rect(30,95,580,46,{.12f,.09f,.05f});wrapped(45,110,c.presentation.screen==CampaignScreen::Debrief?"BATTLE DEBRIEFING":c.state==CampaignState::Finished?"CAMPAIGN COMPLETE":"THE COMPANY CAMP",88,gold);}
    if(c.state==CampaignState::Choice){for(size_t i=0;i<c.choices.size() && i<2;++i)button(30,390+i*24,580,std::to_string(i+1)+" "+c.choices[i]);}
    button(20,450,110,"ARMY BOOK");button(145,450,110,"CAMPAIGN MAP");button(490,438,125,"CONTINUE",c.state!=CampaignState::Choice);
}
void GameUi::cancel_drag(){drag_unit=-1;drag_moved=drag_valid=false;SDL_CaptureMouse(SDL_FALSE);}
void GameUi::begin_drag(const Battle& battle,size_t index,const SDL_Event& e){
    const auto& u=battle.units[index];if(battle.phase!=Phase::Deployment || u.enemy || !u.regiment.alive || u.routing)return;
    drag_unit=int(index);drag_start_x=e.button.x;drag_start_y=e.button.y;drag_moved=drag_valid=false;notice.clear();SDL_CaptureMouse(SDL_TRUE);
}
void GameUi::update_drag(const Battle& battle,int x,int y){
    drag_mouse=renderer.ui_mouse(float(x),float(y));
    if(std::abs(x-drag_start_x)+std::abs(y-drag_start_y)>=5)drag_moved=true;
    drag_valid=drag_unit>=0 && size_t(drag_unit)<battle.units.size() && battle.phase==Phase::Deployment &&
        drag_mouse.x>=0 && drag_mouse.x<640 && drag_mouse.y>=20 && drag_mouse.y<317 &&
        renderer.ground(float(x),float(y),drag_point) && battle.can_deploy(drag_point,battle.units[size_t(drag_unit)].regiment.alive);
}
bool GameUi::covers_battle(const Battle& battle,Vec3 p) const {
    if(hit(p,0,0,640,20) || hit(p,132,338,85,110) || magic_button.contains(p))return true;
    for(unsigned i=0;i<4;++i)if(command_button(i).contains(p))return true;
    if(battle.phase==Phase::Deployment && (hit(p,8,335,110,135) || hit(p,510,317,120,20)))return true;
    return battle.voice_active && mission_portrait && hit(p,8,337,110,133);
}
bool GameUi::battle_event(Battle& battle,const SDL_Event& e){
    if(drag_unit>=0){
        if(battle.phase!=Phase::Deployment || size_t(drag_unit)>=battle.units.size() || e.type==SDL_QUIT ||
           (e.type==SDL_WINDOWEVENT && (e.window.event==SDL_WINDOWEVENT_FOCUS_LOST || e.window.event==SDL_WINDOWEVENT_LEAVE))){cancel_drag();}
        else if(e.type==SDL_KEYDOWN){cancel_drag();if(e.key.keysym.sym==SDLK_ESCAPE)return true;}
        else if(e.type==SDL_MOUSEMOTION){update_drag(battle,e.motion.x,e.motion.y);return true;}
        else if(e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT){
            update_drag(battle,e.button.x,e.button.y);
            if(drag_moved){
                if(drag_valid){auto& u=battle.units[size_t(drag_unit)];u.position=u.destination=drag_point;u.target=-1;u.moving=false;notice.clear();audio.cue();}
                else notice="OUTSIDE DEPLOYMENT AREA";
            }
            cancel_drag();return true;
        }else if(e.type==SDL_MOUSEBUTTONDOWN){cancel_drag();if(e.button.button==SDL_BUTTON_RIGHT)return true;}
    }
    if(e.type==SDL_MOUSEMOTION){
        auto p=renderer.ui_mouse(float(e.motion.x),float(e.motion.y));hovered_command=-1;hovered_magic=magic_button.contains(p);
        for(unsigned i=0;i<4;++i)if(command_button(i).contains(p))hovered_command=int(i);
        return hovered_command>=0 || hovered_magic;
    }
    bool key=e.type==SDL_KEYDOWN && !e.key.repeat;
    if(key){
        if(e.key.keysym.sym==SDLK_g){notice=battle.cast_magic()?"":"MAGIC UNAVAILABLE";if(notice.empty())audio.effect("FIRECAST");return true;}
        UnitCommand cmd=UnitCommand::Automatic;switch(e.key.keysym.sym){case SDLK_h:cmd=UnitCommand::Halt;break;case SDLK_t:cmd=UnitCommand::Shoot;break;case SDLK_b:cmd=UnitCommand::Break;break;case SDLK_c:cmd=UnitCommand::Charge;break;default:return false;}notice=battle.command(cmd)?"":"ORDER UNAVAILABLE";if(notice.empty())audio.cue();return true;
    }
    if(e.type!=SDL_MOUSEBUTTONDOWN)return false;
    auto p=renderer.ui_mouse(float(e.button.x),float(e.button.y));bool left=e.button.button==SDL_BUTTON_LEFT;
    // Banner tray works in deployment and battle; dead units stay visible/disabled.
    std::vector<size_t> friendly;for(size_t i=0;i<battle.units.size();++i)if(!battle.units[i].enemy)friendly.push_back(i);
    if(battle.phase==Phase::Deployment && hit(p,8,335,110,135)){
        if(left && hit(p,12,452,48,15)){if(banner_page) --banner_page;return true;}
        if(left && hit(p,62,452,48,15)){if((banner_page+1)*12<friendly.size())++banner_page;return true;}
        if(left)for(size_t slot=0;slot<12 && banner_page*12+slot<friendly.size();++slot)if(hit(p,13+(slot%4)*25,354+(slot/4)*31,24,30)){
            auto index=friendly[banner_page*12+slot];if(!battle.units[index].regiment.alive)return true;
            if(!(SDL_GetModState()&KMOD_SHIFT))for(auto& u:battle.units)u.selected=false;
            battle.units[index].selected=true;begin_drag(battle,index,e);audio.cue();return true;
        }return true;
    }
    if(left && hit(p,510,317,120,20) && battle.phase==Phase::Deployment){battle.start();audio.effect("HORNURG");notice.clear();return true;}
    if(magic_button.contains(p)){if(left){notice=battle.cast_magic()?"":"MAGIC UNAVAILABLE";if(notice.empty())audio.effect("FIRECAST");}return true;}
    const UnitCommand commands[]={UnitCommand::Halt,UnitCommand::Shoot,UnitCommand::Break,UnitCommand::Charge};
    for(unsigned i=0;i<4;++i)if(command_button(i).contains(p)){
        if(left){notice=battle.command(commands[i])?"":"ORDER UNAVAILABLE";if(notice.empty())audio.cue();}
        return true;
    }
    if(covers_battle(battle,p))return true;
    if(left){for(size_t i=0;i<battle.units.size();++i){const auto& u=battle.units[i];if(!u.regiment.alive || !u.regiment.banner)continue;float x,y;if(renderer.project(u.position+math3d::Vec3{0,6,0},x,y) && hit(p,x-10,y-27,20,28)){
                if(!(SDL_GetModState()&KMOD_SHIFT))for(auto& other:battle.units)other.selected=false;
                battle.units[i].selected=true;begin_drag(battle,i,e);audio.cue();return true;
            }}
    }return false;
}
std::string GameUi::mission_clip(const Regiment& unit,unsigned id){
    if(!executable || id>174)return {};
    uint32_t entry=0x4e7650+id*12;
    if(id<36){unsigned group=(unit.race&7)==5?1:((unit.race&7)==3 || (unit.race&7)==4)?2:0;entry=0x4e8008+(group*36+id)*12;}
    auto address=executable->word(entry);
    if(int32_t(address)<0){unsigned count=unsigned(-int64_t(int32_t(address)));if(count>32)return {};entry=executable->word(entry+4);address=executable->word(entry);}
    if(!address)return {};
    auto name=executable->string(address,64);if(name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)return {};
    return name;
}
void GameUi::mission_dialogue(Battle& battle){
    double elapsed=(SDL_GetTicks64()-mission_voice_start)/1000.;
    if(battle.voice_active && elapsed>=double(mission_speech.samples.size())/22050. && (!audio.available() || !audio.speaking())){battle.voice_active=false;mission_portrait=false;}
    while(!battle.voice_active && !battle.speech_queue.empty()){
        auto request=battle.speech_queue.front();battle.speech_queue.pop_front();if(request.unit>=battle.units.size())continue;
        const auto& u=battle.units[request.unit];
        try{
            auto name=mission_clip(u.regiment,request.clip);if(name.empty())continue;
            auto file=asset("Sound/SP_ENG/"+name+".MAD");if(file.empty()){std::cerr<<"Missing mission voice: "<<name<<'\n';continue;}
            mission_speech=decode_adpcm(read_file(file),1);if(mission_speech.samples.empty())continue;
            mission_portrait=false;
            if(executable && u.regiment.head>0 && u.regiment.head<253){mission_head=executable->word(0x4ceb7c+(u.regiment.head-1)*44+40);mission_portrait=mission_head<63;}
            mission_speaker=u.regiment.leader.empty()?u.regiment.name:u.regiment.leader;
            mission_voice_start=SDL_GetTicks64();elapsed=0;audio.play(mission_speech,22050);battle.voice_active=true;
        }catch(const std::exception& e){std::cerr<<"Mission dialogue: "<<e.what()<<'\n';}
    }
    if(battle.voice_active && mission_portrait){
        float mouth=0;size_t at=size_t(elapsed*22050);
        for(size_t i=at;i<std::min(at+256,mission_speech.samples.size());++i)mouth+=std::abs(float(mission_speech.samples[i]));
        try{renderer.portrait(root,mission_head,9,338,108,112,std::min(1.f,mouth/(256*6000.f)),elapsed);}
        catch(const std::exception& e){mission_portrait=false;std::cerr<<"Mission portrait: "<<e.what()<<'\n';}
        renderer.ui_text(10,453,mission_speaker.substr(0,17),gold,.85f);
    }
}
void GameUi::battle_draw(Battle& battle,bool paused){
    if(mission_generation!=battle.generation){mission_generation=battle.generation;mission_portrait=false;mission_speech={};audio.stop();battle.voice_active=false;}
    renderer.ui_begin();
    for(const auto& u:battle.units)if(u.regiment.alive && u.regiment.banner){float x,y;if(renderer.project(u.position+math3d::Vec3{0,6,0},x,y) && y>45 && y<480 && !covers_battle(battle,{x,y,0})){
        if(u.selected)renderer.ui_rect(x-11,y-28,22,30,gold);
        auto name=registry(u.regiment.banner);if(!name.empty())image("Graphics/Banners/"+name+".SPR",0,x-10,y-27,20,28);else renderer.ui_rect(x-8,y-20,16,20,u.enemy?math3d::Vec3{.7f,.1f,.1f}:math3d::Vec3{.1f,.3f,.7f});
    }}
    renderer.ui_text(8,6,battle.phase==Phase::Deployment?"DEPLOYMENT - POSITION YOUR REGIMENTS":battle.phase==Phase::Battle?(paused?"BATTLE PAUSED":"BATTLE"):battle.phase==Phase::Victory?"VICTORY":"DEFEAT",gold);
    renderer.ui_text(470,6,"ESC MENU / SPACE PAUSE",gold);
    if(battle.phase==Phase::Deployment)image("Graphics/Sprites/DEPLOY.SPR",0,8,335,110,129);
    std::vector<const Unit*> friendly;for(const auto& u:battle.units)if(!u.enemy)friendly.push_back(&u);
    if(banner_page*12>=friendly.size())banner_page=0;
    for(size_t slot=0;battle.phase==Phase::Deployment && slot<12 && banner_page*12+slot<friendly.size();++slot){const auto& u=*friendly[banner_page*12+slot];float x=13+(slot%4)*25,y=354+(slot/4)*31;
        if(u.selected)renderer.ui_rect(x,y,24,30,gold);
        auto name=registry(u.regiment.banner);if(!name.empty())image("Graphics/Banners/"+name+".SPR",0,x+1,y+1,22,28);else renderer.ui_text(x+7,y+10,std::to_string(slot+1),u.regiment.alive?gold:dim);
        if(!u.regiment.alive)renderer.ui_text(x+7,y+10,"X",{.9f,.1f,.1f});
    }
    if(battle.phase==Phase::Deployment){renderer.ui_text(14,455,"PREV",gold);renderer.ui_text(66,455,"NEXT",gold);}
    const Unit* selected=nullptr;for(const auto& u:battle.units)if(u.selected && !u.enemy){selected=&u;break;}
    if(selected){
        renderer.ui_text(132,349,selected->regiment.name,gold,.85f);
        renderer.ui_text(132,365,"STRENGTH",gold,.75f);
        renderer.ui_rect(132,378,80,7,{.22f,.15f,.12f});renderer.ui_rect(132,378,80.f*selected->regiment.alive/std::max(1u,unsigned(selected->regiment.maximum)),7,{.8f,.18f,.1f});
        renderer.ui_text(132,394,std::to_string(selected->regiment.alive)+" / "+std::to_string(selected->regiment.maximum),gold,.75f);
        renderer.ui_text(132,410,"MORALE "+std::to_string(int(selected->morale)),gold,.75f);
        const char* orders[]={"READY","HALTED","SHOOTING","BREAKING OFF","CHARGING"};renderer.ui_text(132,425,orders[unsigned(selected->command)],gold,.75f);
        if(selected->regiment.wizard){
            // PANELS frames 1, 2 and 3 form the original five-slot spell tray.
            for(unsigned slot=0;slot<5;++slot){float x=224+slot*38.f;image("Graphics/Sprites/PANELS.SPR",slot==0?1:slot==4?3:2,x,354,38,62);}
            for(unsigned spell=0;spell<4;++spell)image("Graphics/Books/spells.spr",(selected->regiment.magic_book*4+spell)%32,227+spell*38,367,31,31);
            auto item=selected->regiment.items[0];if(item!=65535 && item<72)image("Graphics/Books/MAG_ITEM.SPR",item,379,367,31,31);
            bool magic_enabled=battle.can_cast_magic(*selected);renderer.ui_text(250,404,"CAST G",magic_enabled?(hovered_magic?math3d::Vec3{.8f,.65f,1.f}:gold):dim,.7f);
        }
    }else renderer.ui_text(132,354,"SELECT A REGIMENT BANNER",gold);
    // Frame 0 is the original transparent combat-control surround. It carries
    // the strength and Winds windows shown by the reference HUD.
    image("Graphics/Sprites/PANELS.SPR",0,490,338,150,138);
    renderer.ui_text(507,414,std::to_string(battle.magic_power),gold);
    const char* labels[]={"HALT H","SHOOT T","BREAK B","CHARGE C"};unsigned frames[]={0,3,9,12};UnitCommand commands[]={UnitCommand::Halt,UnitCommand::Shoot,UnitCommand::Break,UnitCommand::Charge};
    for(unsigned i=0;i<4;++i){const auto rect=command_button(i);float x=rect.x,y=rect.y;bool enabled=selected && selected->regiment.alive && !selected->routing && battle.phase==Phase::Battle && (i!=1 || battle.can_shoot(*selected));
        bool active=enabled && selected->command==commands[i];bool hover=enabled && hovered_command==int(i);
        image("Graphics/Sprites/BUTTONS.SPR",frames[i]+(enabled?((active || hover)?2:1):0),x+8,y,42,42);renderer.ui_text(x+(58-std::string(labels[i]).size()*4.8f)/2,y+45,labels[i],enabled?(hover?math3d::Vec3{1,1,.55f}:gold):dim,.8f);
    }
    if(hovered_command>=0 && hovered_command<4){
        static const char* help[]={"HALT: HOLD POSITION AND CANCEL TARGET", "SHOOT: FIRE WHILE STATIONARY", "BREAK: DISENGAGE FROM THE ENEMY", "CHARGE: RUSH A NEARBY ENEMY"};
        renderer.ui_text(198,322,help[hovered_command],{1,.7f,.3f},.75f);
    }
    if(battle.phase==Phase::Deployment)button(510,317,120,"START BATTLE");
    else mission_dialogue(battle);
    if(dragging() && size_t(drag_unit)<battle.units.size()){
        auto color=drag_valid?math3d::Vec3{.3f,1,.3f}:math3d::Vec3{1,.2f,.15f};
        renderer.ui_rect(drag_mouse.x-12,drag_mouse.y-30,24,32,color);
        auto name=registry(battle.units[size_t(drag_unit)].regiment.banner);
        if(!name.empty())image("Graphics/Banners/"+name+".SPR",0,drag_mouse.x-10,drag_mouse.y-28,20,28);
        if(drag_valid){for(unsigned i=0;i<battle.units[size_t(drag_unit)].regiment.alive;++i){
            auto p=drag_point+math3d::Vec3{(float(i%5)-2)*1.2f,0,float(i/5)*1.2f};p.y=renderer.elevation(p.x,p.z)+.3f;float x,y;
            if(renderer.project(p,x,y))renderer.ui_rect(x-2,y-2,4,4,color);
        }}
    }
    if(!notice.empty()){renderer.ui_rect(150,303,330,20,{.12f,.06f,.035f});renderer.ui_text(160,309,notice,{1,.5f,.2f});}
}
}
