#include "campaign.hpp"
#include "animation.hpp"
#include "battle.hpp"
#include "m3d.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try {
    if(argc!=3)throw std::runtime_error("Usage: neoomen_runtime_tests GAME_ROOT SAVE_PATH");
    std::filesystem::path root=argv[1],save=argv[2];
    unsigned shadow_count=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(m3d::resolve(root,"GameData/1pbat"))){
        auto ext=entry.path().extension().string();if(ext==".SHD" || ext==".shd"){
            auto map=neo::ShadowMap::decode(neo::read_file(entry.path()));check(map.heights.size()==size_t(map.width)*map.height,"Original SHD grid size");++shadow_count;
        }
    }
    check(shadow_count>0,"Original SHD files found");std::cout<<"Decoded "<<shadow_count<<" original SHD maps\n";
    std::set<std::string> routes[2];
    neo::Executable exe(m3d::resolve(root,"PRG_ENG/DarkOmen.exe"));
    unsigned spriteCount=0,frameCount=0;
    auto army=neo::Army::decode(neo::read_file(m3d::resolve(root,"GameData/1PARM/PLYR_ALL.ARM")));
    std::set<unsigned> ids;for(auto& r:army.regiments)if(r.sprite)ids.insert(r.sprite);
    for(auto id:ids) {
        auto animation=neo::UnitAnimation::load(exe,id);
        auto bytes=neo::read_file(m3d::resolve(root,"Graphics/Sprites/"+animation.filename+".SPR"));
        unsigned count=prj::u32(bytes,28);for(unsigned f=0;f<count;++f){neo::decode_sprite(bytes,f);++frameCount;}
        for(unsigned t=0;t<120;++t)for(unsigned d=0;d<8;++d)for(unsigned s=0;s<3;++s)
            check(animation.frame(s==1,s==2,t,d)<count,"Animation exceeds frame count");
        ++spriteCount;
    }
    for(unsigned choice=0;choice<2;++choice) {
        neo::Campaign campaign(root);campaign.advance();
        for(unsigned steps=0;campaign.state!=neo::CampaignState::Finished && steps<3000;++steps) {
            if(campaign.presentation.screen==neo::CampaignScreen::Map){auto name=campaign.presentation.dot_file;std::replace(name.begin(),name.end(),'\\','/');auto slash=name.find_last_of('/');if(slash!=std::string::npos)name=name.substr(slash+1);auto dots=neo::read_file(m3d::resolve(root,"GameData/GameFlow/"+name));check(!neo::decode_travel_route(dots,campaign.presentation.route).empty(),"Campaign travel route");}
            campaign.save(save);neo::Campaign restored(root);restored.restore(save);
            check(restored.pc()==campaign.pc() && restored.state==campaign.state && restored.army.gold==campaign.army.gold && restored.presentation.screen==campaign.presentation.screen && restored.presentation.background==campaign.presentation.background && restored.presentation.speech==campaign.presentation.speech,"Checkpoint roundtrip");
            if(campaign.state==neo::CampaignState::Mission) {
                routes[choice].insert(campaign.mission);auto survivors=campaign.active_army();
                campaign.complete_mission(true,survivors);restored.complete_mission(true,survivors);
            }else {campaign.answer(choice);restored.answer(choice);}
            check(restored.pc()==campaign.pc() && restored.message==campaign.message && restored.mission==campaign.mission,"Checkpoint continuation diverged");
        }
        check(campaign.state==neo::CampaignState::Finished,"Campaign did not terminate");
        check(campaign.completed>=20,"Campaign skipped missions");
        std::cout<<"Campaign choice "<<choice<<": "<<campaign.completed<<" missions\n";
    }
    check(routes[0]!=routes[1],"Choices did not change campaign route");
    neo::Campaign loss(root);loss.advance();while(loss.state!=neo::CampaignState::Mission)loss.answer();
    auto survivors=loss.active_army();check(!survivors.regiments.empty(),"No starting army");auto id=survivors.regiments[0].id;--survivors.regiments[0].alive;auto alive=survivors.regiments[0].alive;
    loss.complete_mission(false,survivors);bool retained=false;for(auto& r:loss.army.regiments)if(r.id==id)retained=r.alive==alive;check(retained,"Casualty carryover");
    auto pc=loss.pc();{std::ofstream out(save);out<<"NEOOMEN_CAMPAIGN 1\n";}
    bool rejected=false;try{loss.restore(save);}catch(const std::exception&){rejected=true;}check(rejected && pc==loss.pc(),"Invalid save mutated campaign");
    auto native=root/"SaveGame"/"darkomen.000";
    if(std::filesystem::is_regular_file(native)){
        neo::Campaign imported(root);imported.restore(native);
        check(imported.loaded_original_save(),"Original save was not detected");
        check(imported.army.regiments.size()==24 && imported.army.gold==4113,"Original save ARM roster");
        check(imported.presentation.screen==neo::CampaignScreen::Book && imported.mission=="Trading Post 1","Original save campaign metadata");
        auto imported_gold=imported.army.gold;{std::ofstream out(save,std::ios::binary);std::string corrupt(0x4a34,'\0');out.write(corrupt.data(),corrupt.size());}
        rejected=false;try{imported.restore(save);}catch(const std::exception&){rejected=true;}check(rejected && imported.army.gold==imported_gold,"Invalid original save mutated campaign");
    }
    auto path=m3d::resolve(root,"GameData/1pbat/B1_01/B1_01.BTB");auto setup=neo::BattleSetup::decode(neo::read_file(path));
    neo::Battle battle;battle.reset(neo::Army::decode(neo::read_file(m3d::resolve(path.parent_path(),setup.player_army+".ARM"))),neo::Army::decode(neo::read_file(m3d::resolve(path.parent_path(),setup.enemy_army+".ARM"))),{},100);
    battle.deploy(setup);battle.attach_script(neo::read_file(m3d::resolve(path.parent_path(),setup.script+".CTL")),setup);battle.start();
    for(unsigned i=0;i<3000 && battle.phase==neo::Phase::Battle;++i)battle.tick();
    check(battle.scripts->instructions>3000,"CTL did not execute");
    std::cout<<spriteCount<<" original sprites / "<<frameCount<<" decoded frames; "<<battle.scripts->instructions<<" CTL instructions\n";
    std::filesystem::remove(save);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
