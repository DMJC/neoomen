#include "renderer.hpp"
#include "audio.hpp"
#include "cursor.hpp"
#include "campaign.hpp"
#include "frontend.hpp"
#include "game_ui.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
struct Options {
    std::filesystem::path data,mission,player,enemy,audio,inspect;
    std::string screenshot;bool colour_cursors=true;
    int frames=0,rate=22050;
    std::filesystem::path save="neoomen.save";
    bool campaign=false,resume=false,no_ctl=false,skip_intro=false;
    bool demo=false,autostart=false,hidden=false,mute=false;
};
int positive(const std::string& s) {size_t end=0;int n=std::stoi(s,&end);if(end!=s.size() || n<=0)throw std::runtime_error("Expected positive integer: "+s);return n;}
std::string lower(std::string s) {for(auto& c:s)if(c>='A' && c<='Z')c=char(c+32);return s;}
void inspect(const std::filesystem::path& path) {
    auto ext=lower(path.extension().string());auto bytes=neo::read_file(path);
    if(ext==".arm") {auto a=neo::Army::decode(bytes);std::cout<<a.regiments.size()<<" regiments, "<<a.gold<<" gold\n";for(const auto& r:a.regiments)std::cout<<r.id<<" "<<r.name<<" ("<<unsigned(r.alive)<<"/"<<unsigned(r.maximum)<<")\n";}
    else if(ext==".btb") {auto setup=neo::BattleSetup::decode(bytes);std::cout<<setup.width<<"x"<<setup.height<<" BTB units, "<<setup.nodes.size()<<" nodes, "<<setup.player_army<<" / "<<setup.enemy_army<<" / "<<setup.script<<"\n";for(const auto& c:neo::decode_btb(bytes))std::cout<<"Chunk "<<c.type<<": "<<c.payload.size()<<" bytes\n";}
    else if(ext==".mad" || ext==".sad") {auto pcm=neo::decode_adpcm(bytes,ext==".sad"?2:1);std::cout<<pcm.channels<<" channels, "<<pcm.samples.size()/pcm.channels<<" PCM frames\n";}
    else if(ext==".spr") {auto n=prj::u32(bytes,28);for(unsigned i=0;i<n;++i)neo::decode_sprite(bytes,i);std::cout<<n<<" sprite frames decoded\n";}
    else if(ext==".prj") {auto d=prj::Document::decode(bytes);std::cout<<d.width()<<"x"<<d.height()<<", "<<d.instance_count()<<" furniture instances\n";}
    else if(ext==".m3d" || ext==".m3x")std::cout<<m3d::Model::decode(bytes).triangles<<" triangles\n";
    else throw std::runtime_error("Unsupported inspect extension: "+ext);
}
neo::Army demo_army(bool enemy) {
    neo::Army a;
    for(unsigned i=0;i<3;++i) {neo::Regiment r;r.id=(enemy?129:1)+i;r.maximum=r.alive=16;r.name=std::string(enemy?"Enemy regiment ":"Player regiment ")+std::to_string(i+1);a.regiments.push_back(r);}
    return a;
}
struct Sdl {
    Sdl(){if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0)throw std::runtime_error(SDL_GetError());}
    ~Sdl(){SDL_Quit();}
};
struct Context {SDL_GLContext value;~Context(){if(value)SDL_GL_DeleteContext(value);}};
}
int main(int argc,char** argv) {
    try {
        Options options;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];auto value=[&](){if(++i==argc)throw std::runtime_error("Missing value for "+arg);return std::string(argv[i]);};
            if(arg=="--help") {std::cout<<"neoomen - Linux Dark Omen reconstruction\n"
                "Usage: neoomen [--demo] [--data GAME_ROOT --mission B1_01]\n"
                "  --mission FILE.PRJ   Load original terrain, textures and furniture\n"
                "  --player FILE.ARM    Player roster (otherwise mission MRC roster)\n"
                "  --enemy FILE.ARM     Enemy roster (otherwise mission NME roster)\n"
                "  --cursor-theme original|system  Choose mouse cursor theme\n"
                "  --audio FILE.MAD|SAD Play decoded audio through SDL2\n"
                "  --audio-rate HZ      Source rate; provisional default 22050\n"
                "  --inspect FILE       Validate and report an asset without a display\n"
                "  --frames N           Exit after N rendered frames\n"
                "  --screenshot FILE.bmp Save final frame before exiting\n"
                "  --autostart --hidden --mute\n  --skip-intro         Open the main menu immediately\n"
                "Controls: left select, Shift add, right order, middle orbit, wheel zoom,\n"
                "WASD pan, F fit, Enter begin, Space pause, R reset, M mute, Esc menu.\n"
                "--campaign: original campaign progression; --resume: load NeoOmen checkpoint or original DarkOmen.### save.\n  --save FILE: checkpoint path; F5 save / F9 restore (mission restarts).\n  --no-ctl: sandbox AI instead of original mission scripts.\n";return 0;}
            else if(arg=="--skip-intro")options.skip_intro=true;
            else if(arg=="--campaign")options.campaign=true;else if(arg=="--resume"){options.resume=true;options.campaign=true;}
            else if(arg=="--save")options.save=value();else if(arg=="--no-ctl")options.no_ctl=true;
            else if(arg=="--data")options.data=value();else if(arg=="--mission")options.mission=value();
            else if(arg=="--player")options.player=value();else if(arg=="--enemy")options.enemy=value();
            else if(arg=="--cursor-theme"){auto theme=value();if(theme!="original" && theme!="system")throw std::runtime_error("Cursor theme must be original or system");options.colour_cursors=theme=="original";}
            else if(arg=="--audio")options.audio=value();else if(arg=="--audio-rate")options.rate=positive(value());
            else if(arg=="--inspect")options.inspect=value();else if(arg=="--frames")options.frames=positive(value());
            else if(arg=="--screenshot")options.screenshot=value();else if(arg=="--demo")options.demo=true;
            else if(arg=="--autostart")options.autostart=true;else if(arg=="--hidden")options.hidden=true;else if(arg=="--mute")options.mute=true;
            else throw std::runtime_error("Unknown option: "+arg);
        }
        if(!options.inspect.empty()) {inspect(options.inspect);return 0;}
        if(options.demo && (options.campaign || !options.mission.empty()))throw std::runtime_error("Choose --demo or --mission");
        if(!options.screenshot.empty() && !options.frames)throw std::runtime_error("--screenshot requires --frames");
        bool front_start=!options.demo && !options.campaign && options.mission.empty();
        if(!options.mission.empty() && !std::filesystem::is_regular_file(options.mission)) {
            std::string name=options.mission.string();
            auto file=m3d::resolve(options.data,"GameData/1pbat/"+name+"/"+name+".PRJ");
            if(file.empty())throw std::runtime_error("Mission not found: "+name+"; supply --data GAME_ROOT or a PRJ path");
            options.mission=file;
        }
        if(options.data.empty() && !options.mission.empty())options.data=options.mission.parent_path().parent_path().parent_path().parent_path();
        std::unique_ptr<neo::Campaign> campaign;
        if(options.campaign){if(options.data.empty())throw std::runtime_error("Campaign requires --data GAME_ROOT");campaign=std::make_unique<neo::Campaign>(options.data);if(options.resume)campaign->restore(options.save);else campaign->advance();}
        std::optional<neo::BattleSetup> setup;
        if(!options.mission.empty()) {
            auto root=options.mission.parent_path();
            auto btb=m3d::resolve(root,options.mission.stem().string()+".BTB");
            if(!btb.empty()) {
                setup=neo::BattleSetup::decode(neo::read_file(btb));
                if(options.player.empty() && !setup->player_army.empty())options.player=m3d::resolve(root,setup->player_army+".ARM");
                if(options.enemy.empty() && !setup->enemy_army.empty())options.enemy=m3d::resolve(root,setup->enemy_army+".ARM");
            }
            for(const auto& entry:std::filesystem::directory_iterator(options.mission.parent_path())) {
                auto name=lower(entry.path().filename().string());
                if(name.size()>=7 && name.substr(name.size()-7)=="mrc.arm" && options.player.empty())options.player=entry.path();
                if(name.size()>=7 && name.substr(name.size()-7)=="nme.arm" && options.enemy.empty())options.enemy=entry.path();
            }
        }
        auto player=options.player.empty()?demo_army(false):neo::Army::decode(neo::read_file(options.player));
        auto enemy=options.enemy.empty()?demo_army(true):neo::Army::decode(neo::read_file(options.enemy));
        Sdl sdl;SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("neoomen - Dark Omen",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,800,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(options.hidden?SDL_WINDOW_HIDDEN:0)),SDL_DestroyWindow);
        if(!window)throw std::runtime_error(SDL_GetError());
        Context context{SDL_GL_CreateContext(window.get())};if(!context.value)throw std::runtime_error(SDL_GetError());SDL_GL_SetSwapInterval(options.hidden?0:1);
        neo::Renderer renderer(window.get());if(!options.mission.empty())renderer.load(options.mission);
        neo::Audio audio;audio.configure(options.data);audio.mute(options.mute);
        if(!options.audio.empty()) {auto ext=lower(options.audio.extension().string());if(ext!=".mad" && ext!=".sad")throw std::runtime_error("Expected MAD or SAD audio");audio.play(neo::decode_adpcm(neo::read_file(options.audio),ext==".sad"?2:1),options.rate);}
        neo::Battle battle;std::string script_error;
        auto reset=[&](){
            script_error.clear();battle.reset(player,enemy,renderer.center,renderer.radius);
            if(setup){std::cout<<"Applied "<<battle.deploy(*setup)<<" BTB unit positions\n";
                if(!options.no_ctl && !setup->script.empty())try{battle.attach_script(neo::read_file(m3d::resolve(options.mission.parent_path(),setup->script+".CTL")),*setup);}catch(const std::exception& e){script_error=e.what();std::cerr<<script_error<<'\n';}
            }
            renderer.load_units(options.data,battle);
            if(options.autostart)battle.start();
        };
        auto load_campaign_mission=[&](){
            if(!campaign || campaign->state!=neo::CampaignState::Mission)return;
            const auto& name=campaign->mission;
            options.mission=m3d::resolve(options.data,"GameData/1pbat/"+name+"/"+name+".PRJ");
            if(options.mission.empty())throw std::runtime_error("Campaign mission missing: "+name);
            auto root=options.mission.parent_path();
            setup=neo::BattleSetup::decode(neo::read_file(m3d::resolve(root,name+".BTB")));
            player=campaign->active_army();enemy=neo::Army::decode(neo::read_file(m3d::resolve(root,setup->enemy_army+".ARM")));
            renderer.load(options.mission);reset();
        };
        if(campaign){if(campaign->state==neo::CampaignState::Mission)load_campaign_mission();else battle.reset({}, {},renderer.center,renderer.radius);}else if(!front_start)reset();
        neo::Frontend frontend(window.get(),renderer,options.data,options.save,options.mute);
        neo::GameUi game_ui(renderer,audio,options.data);
        neo::CursorTheme cursors(options.data);frontend.colour_cursors=options.colour_cursors;
        if(front_start){if(options.skip_intro || options.data.empty())frontend.menu();else frontend.intro();}
        bool running=true,paused=false;int frames=0;double accumulator=0;
        auto previous=SDL_GetPerformanceCounter();const double frequency=double(SDL_GetPerformanceFrequency());
        auto front_action=[&](neo::FrontAction action){
            if(action==neo::FrontAction::Quit){running=false;return;}
            try {
                if(action==neo::FrontAction::NewCampaign || action==neo::FrontAction::LoadCampaign){
                    auto next=std::make_unique<neo::Campaign>(options.data);
                    if(action==neo::FrontAction::LoadCampaign)next->restore(options.save);else next->advance();
                    campaign=std::move(next);script_error.clear();paused=false;load_campaign_mission();
                }else if(action==neo::FrontAction::MovieDone && campaign){campaign->answer();load_campaign_mission();}
            }catch(const std::exception& e){std::cerr<<e.what()<<'\n';frontend.error(e.what());}
            if(action!=neo::FrontAction::None){accumulator=0;previous=SDL_GetPerformanceCounter();}
        };
        uint32_t failed_movie_pc=0;

        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(frontend.active()){if(!frontend.playing_movie() && (event.type==SDL_MOUSEBUTTONDOWN || (event.type==SDL_KEYDOWN && !event.key.repeat)))audio.cue();front_action(frontend.event(event));continue;}
                if(event.type==SDL_QUIT)running=false;
                if(campaign && campaign->state!=neo::CampaignState::Mission){if(game_ui.campaign_event(*campaign,event)){if(campaign->state==neo::CampaignState::Mission)load_campaign_mission();accumulator=0;continue;}}
                else if(game_ui.battle_event(battle,event))continue;
                if(event.type==SDL_KEYDOWN && !event.key.repeat) switch(event.key.keysym.sym) {
                    case SDLK_ESCAPE:frontend.menu(true);accumulator=0;break;
                    case SDLK_1:case SDLK_2:case SDLK_RETURN:
                        if(campaign && campaign->state!=neo::CampaignState::Mission) {
                            campaign->answer(event.key.keysym.sym==SDLK_2?1:0);load_campaign_mission();accumulator=0;
                        } else if(campaign && (battle.phase==neo::Phase::Victory || battle.phase==neo::Phase::Defeat)) {
                            neo::Army survivors;for(const auto& unit:battle.units)if(!unit.enemy)survivors.regiments.push_back(unit.regiment);
                            campaign->complete_mission(battle.phase==neo::Phase::Victory,survivors);load_campaign_mission();accumulator=0;
                        } else if(event.key.keysym.sym==SDLK_RETURN){battle.start();audio.cue();}break;
                    case SDLK_F5:if(campaign){campaign->save(options.save);std::cout<<"Campaign checkpoint saved\n";}break;
                    case SDLK_F9:if(campaign){campaign->restore(options.save);load_campaign_mission();accumulator=0;}break;
                    case SDLK_SPACE:paused=!paused;break;
                    case SDLK_f:renderer.fit();break;
                    case SDLK_r:if(!campaign || campaign->state==neo::CampaignState::Mission)reset();accumulator=0;paused=false;break;
                    case SDLK_m:options.mute=!options.mute;frontend.set_mute(options.mute);audio.mute(options.mute);break;
                    default:break;
                }
                if(event.type==SDL_MOUSEWHEEL)renderer.zoom(float(event.wheel.y)*(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1));
                if(event.type==SDL_MOUSEMOTION && (event.motion.state&SDL_BUTTON_MMASK))renderer.orbit(float(event.motion.xrel),float(event.motion.yrel));
                if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_RIGHT && (!campaign || campaign->state==neo::CampaignState::Mission)) {
                    int hit=renderer.pick(battle,float(event.button.x),float(event.button.y));
                    math3d::Vec3 point;
                    if(hit>=0)point=battle.units[size_t(hit)].destination;
                    else if(!renderer.ground(float(event.button.x),float(event.button.y),point))continue;
                    renderer.center_on(point);
                }
            }
            if(!running)break;
            auto now=SDL_GetPerformanceCounter();double dt=std::min(.25,double(now-previous)/frequency);previous=now;
            if(!frontend.active() && campaign && campaign->state==neo::CampaignState::Dialogue && campaign->message.rfind("Movie: ",0)==0 && campaign->pc()!=failed_movie_pc){
                auto name=campaign->message.substr(7);auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name=name.substr(slash+1);
                try{frontend.campaign_movie(m3d::resolve(options.data,"Movies/"+name));}catch(const std::exception& e){std::cerr<<e.what()<<'\n';failed_movie_pc=campaign->pc();}
            }
            options.mute=frontend.muted();
            audio.mute(options.mute);
            audio.suspend(frontend.playing_movie());
            if(!frontend.playing_movie()){
                if(frontend.active() || (campaign && campaign->state!=neo::CampaignState::Mission))audio.music("EERIE9","sNormal");
                else audio.music("BATTLE1",battle.phase==neo::Phase::Deployment?"sDeploy":battle.phase==neo::Phase::Battle?"sNormal":"sEnd");
            }
            if(frontend.active()){front_action(frontend.draw());accumulator=0;}
            else {
            const Uint8* keys=SDL_GetKeyboardState(nullptr);renderer.pan(float(keys[SDL_SCANCODE_RIGHT])-keys[SDL_SCANCODE_LEFT],float(keys[SDL_SCANCODE_UP])-keys[SDL_SCANCODE_DOWN],float(dt));
            if(script_error.empty() && !paused && (!campaign || campaign->state==neo::CampaignState::Mission)) {accumulator+=dt;while(accumulator>=1./30){try{auto attacks=battle.attacks,ranged=battle.ranged_attacks;battle.tick();if(battle.ranged_attacks!=ranged)audio.effect("ARROW02");else if(battle.attacks!=attacks)audio.effect("SWORD05");}catch(const std::exception& e){script_error=e.what();std::cerr<<script_error<<'\n';accumulator=0;break;}accumulator-=1./30;}}else accumulator=0;
            for(auto& unit:battle.units)unit.position.y=renderer.elevation(unit.position.x,unit.position.z);
            renderer.draw(battle,paused);
            if(campaign && campaign->state!=neo::CampaignState::Mission)game_ui.campaign_draw(*campaign);
            else {game_ui.battle_draw(battle,paused);if(campaign && (battle.phase==neo::Phase::Victory || battle.phase==neo::Phase::Defeat))renderer.overlay("BATTLE COMPLETE",{"ENTER TO CONTINUE CAMPAIGN / R RETRY"});}
            if(!script_error.empty())renderer.overlay("CTL EXECUTION STOPPED",{script_error,"R RETRY / F9 RESTORE CAMPAIGN / ESC MENU","--NO-CTL ENABLES THE SEPARATE SANDBOX MODE"});
            }
            int mouse_x,mouse_y;auto buttons=SDL_GetMouseState(&mouse_x,&mouse_y);
            std::string cursor="POINT";
            if(!frontend.active() && (!campaign || campaign->state==neo::CampaignState::Mission))cursor=game_ui.battle_cursor(battle,mouse_x,mouse_y,(SDL_GetModState()&KMOD_SHIFT)!=0,(buttons&SDL_BUTTON_MMASK)!=0);
            cursors.update(cursor,frontend.colour_cursors,SDL_GetTicks64());
            audio.update();
            ++frames;
            GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error "+std::to_string(error));
            if(options.frames && frames>=options.frames) {if(!options.screenshot.empty())renderer.screenshot(options.screenshot);running=false;}
            SDL_GL_SwapWindow(window.get());if(options.hidden)SDL_Delay(1);
        }
        std::cout<<"Rendered "<<frames<<" frames; simulated "<<battle.ticks<<" ticks\n";return script_error.empty()?0:2;
    }catch(const std::exception& error){std::cerr<<"neoomen: "<<error.what()<<'\n';return 1;}
}
