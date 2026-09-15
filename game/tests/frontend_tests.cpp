#include "frontend.hpp"
#include "cursor.hpp"
#include "game_ui.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: neoomen_frontend_tests GAME_ROOT (requires OpenGL display)");
    check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0,SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto* window=SDL_CreateWindow("neoomen frontend test",0,0,960,600,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    check(window,SDL_GetError());auto context=SDL_GL_CreateContext(window);check(context,SDL_GetError());
    {
        neo::CursorTheme cursors(argv[1]);
        cursors.update("SELECT",false,0);auto system_cursor=SDL_GetCursor();
        for(auto name:{"SELECT","HAND","HAND3","MOVE","SWORD","ROTATE","SELL","STAFF","POINT","HGLASS"}){
            cursors.update(name,true,0);check(SDL_GetCursor()!=system_cursor,"Original colour cursor loaded");auto first=SDL_GetCursor();
            cursors.update(name,true,180);if(std::string(name)=="MOVE" || std::string(name)=="SWORD")check(SDL_GetCursor()!=first,"ANI timing advances colour cursor frame");
        }
        cursors.update("MISSING",true,0);check(SDL_GetCursor()==system_cursor,"Missing cursor falls back to system");
        cursors.update("SELECT",false,0);check(SDL_GetCursor()==system_cursor,"System theme restored");
        neo::Renderer renderer(window);neo::Frontend menu(window,renderer,argv[1],"/tmp/neoomen-deliberately-missing-checkpoint",true);
        auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;return menu.event(e);};
        menu.menu(true);check(key(SDLK_ESCAPE)==neo::FrontAction::Continue && !menu.active(),"Continue from menu");
        menu.menu();check(key(SDLK_l)==neo::FrontAction::None && menu.active(),"Unavailable load disabled");
        key(SDLK_DOWN);key(SDLK_RETURN);key(SDLK_m);check(!menu.muted(),"Options sound toggle");key(SDLK_c);check(!menu.colour_cursors,"Options cursor theme toggle");key(SDLK_ESCAPE);
        menu.draw();check(glGetError()==GL_NO_ERROR,"Menu rendering");
        SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;click.button.x=480;click.button.y=275;
        check(menu.event(click)==neo::FrontAction::NewCampaign,"Mouse New Campaign");
        menu.intro();menu.draw();check(glGetError()==GL_NO_ERROR,"Movie rendering");key(SDLK_SPACE);
        check(menu.active() && key(SDLK_n)==neo::FrontAction::NewCampaign,"Skip splash goes to main menu");
        menu.campaign_movie(m3d::resolve(argv[1],"Movies/INTRO.TGQ"));check(key(SDLK_ESCAPE)==neo::FrontAction::MovieDone && !menu.active(),"Campaign movie skip resumes game");
        menu.menu();check(key(SDLK_ESCAPE)==neo::FrontAction::Quit,"Quit from main menu");
        neo::Audio voice;voice.mute(true);neo::GameUi ui(renderer,voice,argv[1]);neo::Campaign campaign(argv[1]);campaign.advance();
        for(unsigned i=0;i<100 && campaign.presentation.screen!=neo::CampaignScreen::TalkingHead;++i){if(campaign.state==neo::CampaignState::Mission)campaign.complete_mission(true,campaign.active_army());else campaign.answer();}
        check(campaign.presentation.screen==neo::CampaignScreen::TalkingHead,"Reach original talking head");ui.campaign_draw(campaign);check(glGetError()==GL_NO_ERROR,"Talking head rendering");renderer.screenshot("/tmp/neoomen-head.bmp");
        SDL_Event book{};book.type=SDL_KEYDOWN;book.key.keysym.sym=SDLK_b;ui.campaign_event(campaign,book);ui.campaign_draw(campaign);check(glGetError()==GL_NO_ERROR,"Army book rendering");renderer.screenshot("/tmp/neoomen-book.bmp");
        book.key.keysym.sym=SDLK_s;ui.campaign_event(campaign,book);ui.campaign_draw(campaign);check(glGetError()==GL_NO_ERROR,"Book equipment page");
        book.key.keysym.sym=SDLK_m;ui.campaign_event(campaign,book);ui.campaign_draw(campaign);check(glGetError()==GL_NO_ERROR,"Campaign map rendering");renderer.screenshot("/tmp/neoomen-map.bmp");
        auto mission=m3d::resolve(argv[1],"GameData/1pbat/B1_01/B1_01.PRJ");renderer.load(mission);
        neo::Battle battle;battle.reset(campaign.active_army(),campaign.active_army(),renderer.center,renderer.radius);auto setup=neo::BattleSetup::decode(neo::read_file(m3d::resolve(mission.parent_path(),"B1_01.BTB")));battle.deploy(setup);renderer.load_units(argv[1],battle);
        // With the camera and troops fixed, only the water advances between these draws.
        auto pixels=[&](){int w,h;SDL_GL_GetDrawableSize(window,&w,&h);std::vector<unsigned char> rgba(size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());return rgba;};
        renderer.draw(battle,false);
        int viewport[4],drawable_w,drawable_h;glGetIntegerv(GL_VIEWPORT,viewport);SDL_GL_GetDrawableSize(window,&drawable_w,&drawable_h);
        check(viewport[0]==0 && viewport[1]==0 && viewport[2]==drawable_w && viewport[3]==drawable_h,"Battle scene fills window behind HUD");
        math3d::Vec3 ground_point;check(renderer.ground(480,300,ground_point),"Center ray intersects terrain");float projected_x,projected_y;
        check(renderer.project(ground_point,projected_x,projected_y),"Picked terrain projects to screen");auto logical_center=renderer.ui_mouse(480,300);
        check(std::abs(projected_x-logical_center.x)<.1f && std::abs(projected_y-logical_center.y)<.1f,"Full-window picking and projection agree");
        auto water_before=pixels();SDL_Delay(100);renderer.draw(battle,false);auto water_after=pixels();
        check(water_before!=water_after,"Water animation changes rendered pixels");
        renderer.draw(battle,true);auto paused_water=pixels();SDL_Delay(30);renderer.draw(battle,true);
        check(paused_water==pixels(),"Pause freezes water vertex and UV animation");
        check(glGetError()==GL_NO_ERROR,"Animated water shader");
        click.button.x=105;click.button.y=450;check(ui.battle_event(battle,click) && battle.units[0].selected,"Deployment tray selects regiment");
        // Drag a tray banner onto a valid terrain point without moving other regiments.
        auto original=battle.units[0].position;auto other=battle.units.size()>1?battle.units[1].position:original;
        SDL_Event motion{};motion.type=SDL_MOUSEMOTION;motion.motion.state=SDL_BUTTON_LMASK;
        bool destination_found=false;
        for(int y=40;y<390 && !destination_found;y+=3)for(int x=80;x<880 && !destination_found;x+=3){
            math3d::Vec3 point;auto logical=renderer.ui_mouse(float(x),float(y));
            if(logical.y<317 && renderer.ground(float(x),float(y),point) && battle.can_deploy(point,battle.units[0].regiment.alive) && std::abs(point.x-original.x)>2){motion.motion.x=x;motion.motion.y=y;destination_found=true;}
        }
        check(destination_found,"Find valid deployment drag target");
        check(ui.battle_event(battle,motion) && ui.dragging() && ui.valid_drop(),"Valid drag preview");
        check(battle.units[0].position.x==original.x,"Preview does not move regiment before release");
        renderer.draw(battle,false);ui.battle_draw(battle,false);check(glGetError()==GL_NO_ERROR,"Drag preview rendering");
        SDL_Event release{};release.type=SDL_MOUSEBUTTONUP;release.button.button=SDL_BUTTON_LEFT;release.button.x=motion.motion.x;release.button.y=motion.motion.y;
        check(ui.battle_event(battle,release) && !ui.dragging(),"Drop ends drag");
        check(std::abs(battle.units[0].position.x-original.x)>2 && battle.can_deploy(battle.units[0].position,battle.units[0].regiment.alive),"Drop repositions formation inside deployment area");
        if(battle.units.size()>1)check(battle.units[1].position.x==other.x && battle.units[1].position.z==other.z,"Drag leaves other regiments in place");
        original=battle.units[0].position;
        ui.battle_event(battle,click);motion.motion.x=10;motion.motion.y=10;ui.battle_event(battle,motion);
        check(!ui.valid_drop(),"Invalid drag preview");release.button.x=10;release.button.y=10;ui.battle_event(battle,release);
        check(battle.units[0].position.x==original.x && battle.units[0].position.z==original.z,"Invalid drop preserves position");
        ui.battle_event(battle,click);ui.battle_event(battle,motion);SDL_Event cancel{};cancel.type=SDL_KEYDOWN;cancel.key.keysym.sym=SDLK_ESCAPE;
        check(ui.battle_event(battle,cancel) && !ui.dragging(),"Escape cancels banner drag");
        renderer.draw(battle,false);ui.battle_draw(battle,false);check(glGetError()==GL_NO_ERROR,"Deployment HUD rendering");renderer.screenshot("/tmp/neoomen-selected-hud.bmp");
        click.button.x=790;click.button.y=407;check(ui.battle_event(battle,click) && battle.phase==neo::Phase::Battle,"Start battle button");
        check(!ui.covers_battle(battle,{125,460,0}) && ui.covers_battle(battle,{535,380,0}),"Only occupied HUD areas intercept battlefield input");
        battle.units[0].regiment.missile_weapon=10;battle.units[0].regiment.stats[2]=10;battle.units[0].regiment.stats[3]=10;
        for(size_t i=0;i<battle.units.size();++i)if(battle.units[i].enemy){battle.units[0].position={0,0,0};battle.units[0].destination=battle.units[0].position;battle.units[i].position={35,0,0};battle.units[i].destination=battle.units[i].position;break;}
        check(battle.command(neo::UnitCommand::Shoot),"Shoot command launches ranged attack");battle.tick();check(!battle.projectiles.empty(),"Ranged projectile in flight");
        renderer.draw(battle,false);ui.battle_draw(battle,false);check(glGetError()==GL_NO_ERROR,"Ranged projectile rendering");renderer.screenshot("/tmp/neoomen-ranged.bmp");
        battle.say(0,37);renderer.draw(battle,true);ui.battle_draw(battle,true);
        check(battle.voice_active && battle.speech_queue.empty(),"Mission dialogue starts original voice and consumes request");
        check(glGetError()==GL_NO_ERROR,"In-mission animated portrait rendering");renderer.screenshot("/tmp/neoomen-mission-head.bmp");
        battle.units[0].regiment.wizard=1;battle.magic_power=1;
        click.button.x=500;click.button.y=470;check(ui.battle_event(battle,click) && battle.magic_power==0 && !battle.projectiles.empty(),"Magic button casts from selected wizard");
        renderer.draw(battle,false);ui.battle_draw(battle,false);check(glGetError()==GL_NO_ERROR,"Mage spell HUD rendering");renderer.screenshot("/tmp/neoomen-mage-hud.bmp");
        for(auto& unit:battle.units)unit.selected=false;
        click.button.x=105;click.button.y=450;ui.battle_event(battle,click);
        check(!battle.units[0].selected,"Deployment banner tray is inactive after battle starts");battle.units[0].selected=true;
        click.button.x=735;click.button.y=470;check(ui.battle_event(battle,click) && battle.units[0].command==neo::UnitCommand::Halt,"Halt button executes order");
        battle.reset(campaign.active_army(),{},renderer.center,renderer.radius);renderer.draw(battle,true);ui.battle_draw(battle,true);
        check(!voice.speaking() && !battle.voice_active,"Retry into deployment cancels old mission dialogue");



    }
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"Frontend, campaign screens, deployment tray, command buttons and OpenGL passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';SDL_Quit();return 1;}}
