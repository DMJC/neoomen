#include "battle.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}check(threw,"Malformed input accepted");}
void word(prj::Bytes& b,size_t p,uint32_t n){prj::put32(b,p,n);}
}
int main(){try {
    prj::Bytes arm(192+188);word(arm,0,670);word(arm,4,1);word(arm,8,188);word(arm,192+4,129);arm[192+0x39]=arm[192+0x3a]=16;arm[0x94]=100;
    auto army=neo::Army::decode(arm);check(army.gold==100 && army.regiments[0].id==129 && army.regiments[0].alive==16,"ARM known fields");
    arm.pop_back();rejects([&]{neo::Army::decode(arm);});arm.resize(380);word(arm,4,0xffffffff);rejects([&]{neo::Army::decode(arm);});
    prj::Bytes btb(28);word(btb,0,0xbeafeed0);word(btb,8,42);word(btb,12,4);word(btb,16,123);word(btb,20,0xbeafeed0);
    auto chunks=neo::decode_btb(btb);check(chunks.size()==1 && chunks[0].type==42 && prj::u32(chunks[0].payload,0)==123,"BTB unknown chunk retention");
    btb.pop_back();rejects([&]{neo::decode_btb(btb);});
    auto tlv=[](uint32_t tag,const prj::Bytes& payload,bool item) {
        prj::Bytes b(8);word(b,0,tag);word(b,4,uint32_t(payload.size()+(item?8:0)));b.insert(b.end(),payload.begin(),payload.end());return b;
    };
    auto scalar=[&](uint32_t tag,uint32_t n){prj::Bytes b(4);word(b,0,n);return tlv(tag,b,true);};
    auto append=[](prj::Bytes& to,const prj::Bytes& from){to.insert(to.end(),from.begin(),from.end());};
    prj::Bytes header,node,objects;
    append(header,scalar(1,1440));append(header,scalar(2,1600));
    append(node,scalar(1,1210));append(node,scalar(2,957));append(node,scalar(5,16387));append(node,scalar(12,129));
    append(objects,tlv(503,node,true));
    prj::Bytes setupBytes=tlv(0xbeafeed0,{},false);append(setupBytes,tlv(1,header,false));append(setupBytes,tlv(5,objects,false));append(setupBytes,tlv(0xbeafeed0,{},false));
    auto setup=neo::BattleSetup::decode(setupBytes);
    check(setup.width==1440 && setup.nodes.size()==1 && setup.nodes[0].unit_id==129 && setup.nodes[0].z==957,"BTB nested game object fields");
    neo::Battle deployment;deployment.reset(army,{}, {},20);check(deployment.deploy(setup)==1 && deployment.units[0].position.x==151.25f,"BTB/ARM identity join and coordinate scale");
    word(setupBytes,20,0);rejects([&]{neo::BattleSetup::decode(setupBytes);});
    prj::Bytes spr(70);word(spr,0,0x4f444857);word(spr,4,70);word(spr,8,32);word(spr,12,68);word(spr,16,64);word(spr,20,1);word(spr,28,1);spr[32]=4;spr[34]=255;spr[40]=2;spr[42]=1;word(spr,48,2);spr[64]=20;spr[65]=40;spr[66]=60;spr[69]=255;
    auto frame=neo::decode_sprite(spr,0);check(frame.rgba==prj::Bytes({60,40,20,255,0,0,0,0}),"SPR BGR palette and transparency");
    spr[68]=1;rejects([&]{neo::decode_sprite(spr,0);});
    spr.resize(74);word(spr,4,74);word(spr,12,72);word(spr,20,2);word(spr,56,1);spr[36]=0xfe;spr[37]=0xff;spr[38]=3;
    spr[68]=10;spr[69]=20;spr[70]=30;spr[72]=0;spr[73]=255;
    auto bank=neo::decode_sprite(spr,0);check(bank.anchor_x==-2 && bank.anchor_y==3 && bank.rgba==prj::Bytes({30,20,10,255,0,0,0,0}),"SPR bank and signed anchors");
    spr[68]=spr[69]=spr[70]=0;check(neo::decode_sprite(spr,0).rgba[3]==0,"Black SPR palette entry is transparent");
    spr[68]=255;spr[69]=255;spr[70]=0;auto shadow_pixel=neo::decode_sprite(spr,0);check(shadow_pixel.rgba[0]==0 && shadow_pixel.rgba[1]==0 && shadow_pixel.rgba[2]==0 && shadow_pixel.rgba[3]==128,"Cyan SPR pixel becomes translucent shadow");
    prj::Bytes shd(32+16+64);word(shd,0,0x44414853);word(shd,4,uint32_t(shd.size()-8));word(shd,8,9);word(shd,12,8);word(shd,16,1);word(shd,20,2);word(shd,24,16);word(shd,28,1024);word(shd,36,2048);word(shd,44,64);shd[48]=8;shd[48+9]=4;
    auto shadows=neo::ShadowMap::decode(shd);check(shadows.width==9 && shadows.heights[0]==2 && shadows.heights[8]==3 && shadows.heights[10]==1.5f,"SHD base plus fine detail, shared blocks and row addressing");
    word(shd,40,64);rejects([&]{neo::ShadowMap::decode(shd);});word(shd,40,0);shd.pop_back();rejects([&]{neo::ShadowMap::decode(shd);});
    // One CTL local function: initialize, await deployment, set a register,
    // wait two ticks, increment, yield, then terminate.
    prj::Bytes ctl(19*4);unsigned code[]={1,0,3,0x8000,0,0x8001,0x801e,0,7,0x8018,2,0x801a,0x801f,0,2,0x8015,0x80f1,0,0};
    for(unsigned i=0;i<19;++i)word(ctl,i*4,code[i]);
    // Local ID zero resolves table[bias]; entry one points to word three.
    word(ctl,4,3);neo::Battle scripted;scripted.reset(army,army,{},20);neo::BattleSetup scriptSetup;neo::BattleNode scriptNode;scriptNode.flags=2;scriptNode.unit_id=129;scriptSetup.nodes.push_back(scriptNode);
    scripted.attach_script(ctl,scriptSetup);check(scripted.scripts->states[0].registers[0]==0,"CTL deployment barrier");scripted.start();scripted.tick();check(scripted.scripts->states[0].registers[0]==7,"CTL register assignment");scripted.tick();check(scripted.scripts->states[0].registers[0]==7,"CTL timer suspension");scripted.tick();check(scripted.scripts->states[0].registers[0]==9,"CTL timer resume and arithmetic");scripted.tick();check(!scripted.scripts->states[0].active,"CTL termination");
    rejects([]{neo::CtlProgram bad({0,0,0});});word(ctl,4,999);rejects([&]{neo::CtlProgram bad(ctl);bad.function(0);});
    unsigned voice_code[]={1,3,0,0x80ae,37,0x80af,129,38,0x80ea,0x8015,0x80ea,0x80f1};
    prj::Bytes voice_ctl(sizeof voice_code);for(unsigned i=0;i<sizeof voice_code/sizeof *voice_code;++i)word(voice_ctl,i*4,voice_code[i]);
    neo::Battle voiced;voiced.reset(army,{}, {},20);voiced.attach_script(voice_ctl,scriptSetup);
    check(voiced.speech_queue.size()==2 && voiced.speech_queue[0].clip==37 && voiced.speech_queue[1].clip==38,"CTL play_self/play_other queue original clip IDs");
    check(voiced.scripts->states[0].control&4,"CTL sound test sees queued dialogue");voiced.speech_queue.clear();voiced.scripts->tick(voiced);check(!(voiced.scripts->states[0].control&4),"CTL sound test clears after playback");
    voiced.say(0,37);voiced.say(0,37);check(voiced.speech_queue.size()==1,"Duplicate mission speech suppressed");voiced.reset(army,{}, {},20);check(voiced.speech_queue.empty() && !voiced.voice_active,"Mission reset clears speech");
    auto mono=neo::decode_adpcm({0,0,0,0,0x71,0,0,0},1);check(mono.samples.size()==9 && mono.samples[0]==0 && mono.samples[1]==1 && mono.samples[2]==12,"IMA low nibble first golden vector");
    prj::Bytes stereo(16);stereo[8]=0x71;stereo[12]=0xF9;
    auto pcm=neo::decode_adpcm(stereo,2);check(pcm.samples.size()==18 && pcm.samples[0]==0 && pcm.samples[1]==0 && pcm.samples[2]==1 && pcm.samples[3]==-1 && pcm.samples[4]==12 && pcm.samples[5]==-12,"SAD planar stereo golden vector");
    auto raw=neo::decode_adpcm({0,0,99,0,0x00,0x80,0xff,0x7f},1);check(raw.samples==std::vector<int16_t>({0,-32768,32767}),"PCM escape signed values");
    prj::Bytes tail(1030);tail[2]=99;tail[1024]=0x34;tail[1025]=0x12;
    auto continuation=neo::decode_adpcm(tail,1);
    check(continuation.samples.size()==514 && continuation.samples[511]==0x1234,"Raw tail crosses read-ahead boundary without a new header");
    auto rightEscape=neo::decode_adpcm({1,0,0,0,2,0,99,0,3,0,4,0},2);
    check(rightEscape.samples==std::vector<int16_t>({1,2,3,4}),"Either stereo channel can trigger PCM escape");
    rejects([]{neo::decode_adpcm({0,0,0},1);});rejects([]{neo::decode_adpcm(prj::Bytes(9),2);});
    neo::Battle battle;battle.reset(army,army,{0,0,0},20);battle.units[0].selected=true;
    battle.order({-4,0,-4});check(battle.units[0].position.x==-4,"Deployment orders");battle.tick();check(battle.ticks==0,"Deployment freezes simulation");
    battle.start();battle.order({4,0,4});auto x=battle.units[0].position.x;battle.tick();check(battle.units[0].position.x>x,"Movement advances");
    neo::Battle copy=battle;for(int i=0;i<3000;++i){battle.tick();copy.tick();}
    check(battle.phase!=neo::Phase::Battle,"Battle reaches result");check(copy.phase==battle.phase && copy.ticks==battle.ticks && copy.units[0].regiment.alive==battle.units[0].regiment.alive,"Deterministic battle replay");
    neo::BattleRegion zone;zone.points={{{0,0}},{{30,0}},{{30,30}},{{0,30}}};
    check(zone.contains(15,15) && zone.contains(0,15) && !zone.contains(31,15),"Deployment polygon and boundary");
    neo::Battle placement;placement.reset(army,{}, {},20);placement.deployment={zone};placement.units[0].selected=true;
    check(placement.order({10,0,10}),"Valid formation deployment");auto before=placement.units[0].position;
    check(!placement.order({1,0,1}) && placement.units[0].position.x==before.x,"Reject whole formation crossing deployment edge");
    auto cmds=[&](){neo::Battle b;b.reset(army,army,{},20);b.units[0].selected=true;b.units[0].position={0,0,0};b.units[0].destination={0,0,0};b.units[1].position={40,0,0};b.units[1].destination=b.units[1].position;b.units[1].command=neo::UnitCommand::Halt;b.start();return b;};
    auto controls=cmds();check(!controls.command(neo::UnitCommand::Shoot),"Non-ranged regiment cannot shoot");controls.order({20,0,0});controls.command(neo::UnitCommand::Halt);controls.tick();check(controls.units[0].position.x==0 && controls.units[0].target==-1,"Halt cancels movement and target");
    controls.units[0].regiment.missile_weapon=1;controls.units[0].regiment.stats[2]=10;controls.units[0].regiment.stats[3]=10;controls.units[1].regiment.stats[4]=1;
    check(controls.command(neo::UnitCommand::Shoot),"Ranged regiment enables shoot");auto target_alive=controls.units[1].regiment.alive;controls.tick();
    check(controls.ranged_attacks==1 && !controls.projectiles.empty() && controls.units[0].position.x==0,"Shoot launches projectiles without moving");
    check(controls.units[1].regiment.alive==target_alive,"Projectile does not damage target before impact");
    for(unsigned i=0;i<60 && !controls.projectiles.empty();++i)controls.tick();
    check(controls.projectiles.empty() && controls.units[1].regiment.alive<=target_alive,"Projectile resolves at end of flight");
    auto breaker=cmds();breaker.units[0].target=1;breaker.command(neo::UnitCommand::Break);breaker.tick();check(breaker.units[0].position.x<0 && breaker.units[0].target==-1,"Break disengages from target");
    auto charge=cmds(),walk=cmds();charge.command(neo::UnitCommand::Charge);charge.order({30,0,0});walk.order({30,0,0});charge.tick();walk.tick();check(charge.units[0].position.x>walk.units[0].position.x,"Charge movement responds to button command");
    auto winds=cmds();winds.units[0].regiment.wizard=2;winds.command(neo::UnitCommand::Halt);for(unsigned i=0;i<901;++i)winds.tick();check(winds.magic_power>0 && winds.magic_power<=3 && winds.magic_countdown>29,"Magic countdown replenishes visible power");
    prj::Bytes dots(16+4+32+44);word(dots,0,0x57444f54);word(dots,12,1);word(dots,16,2);word(dots,20,10);word(dots,24,20);word(dots,36,30);word(dots,40,40);word(dots,72,0xffffffff);
    auto route=neo::decode_travel_route(dots,0);check(route.size()==2 && route[1][0]==30 && route[1][1]==40,"Original travel polyline decode");word(dots,72,0);rejects([&]{neo::decode_travel_route(dots,0);});
    std::cout<<"Asset validation, codec vectors, deployment, movement and deterministic battle passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
