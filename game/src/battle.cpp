#include "battle.hpp"
using namespace math3d;
namespace neo {
void Battle::reset(const Army& player,const Army& enemy,Vec3 center,float radius) {
    ++generation;speech_queue.clear();voice_active=false;scripts.reset();units.clear();projectiles.clear();deployment.clear();magic_countdown=30;magic_power=0;phase=Phase::Deployment;ticks=attacks=ranged_attacks=0;random.seed(1);
    auto add=[&](const Army& army,bool hostile) {
        size_t i=0;
        for(const auto& regiment:army.regiments) {
            if(!regiment.alive)continue;
            Vec3 pos=center+Vec3{(float(i%4)-1.5f)*radius*.13f,0,(hostile?1.f:-1.f)*radius*(.25f+.1f*float(i/4))};
            Unit u;u.regiment=regiment;u.position=u.destination=pos;u.enemy=hostile;units.push_back(u);++i;
        }
    };
    add(player,false);add(enemy,true);
}
unsigned Battle::deploy(const BattleSetup& setup) {
    deployment.clear();for(const auto& region:setup.regions)if(region.flags&0x100)deployment.push_back(region);
    unsigned placed=0;
    for(auto& unit:units)for(const auto& node:setup.nodes)if((node.flags&2) && node.unit_id==unit.regiment.id) {
        unit.position.x=node.x/8.f;unit.position.z=node.z/8.f;
        unit.destination=unit.position;unit.heading=node.heading*6.2831853f/512;++placed;break;
    }
    return placed;
}
void Battle::attach_script(const prj::Bytes& data,const BattleSetup& setup) {scripts=std::make_shared<CtlRuntime>(data,setup,*this);scripts->tick(*this);}
void Battle::say(size_t unit,unsigned clip){
    if(unit>=units.size() || speech_queue.size()>=16)return;
    if(!speech_queue.empty()){const auto& last=speech_queue.back();if(last.unit==unit && last.clip==clip && ticks-last.tick<=30)return;}
    speech_queue.push_back({unit,clip,ticks});
}
void Battle::start() {if(phase==Phase::Deployment)phase=Phase::Battle;}
void Battle::set_terrain(unsigned width,unsigned height,float min_x,float max_x,float min_z,float max_z,std::vector<uint8_t> attributes,std::vector<float> heights){
    if(!width || !height || attributes.size()!=size_t(width)*height || heights.size()!=size_t(width)*height || max_x<=min_x || max_z<=min_z)throw std::runtime_error("Invalid battle terrain");
    terrain={width,height,min_x,max_x,min_z,max_z,std::move(attributes),std::move(heights)};
}
bool Battle::terrain_blocked(Vec3 point) const {
    if(!terrain.width)return false;
    auto x=unsigned(std::clamp((point.x-terrain.min_x)/(terrain.max_x-terrain.min_x)*float(terrain.width-1),0.f,float(terrain.width-1)));
    auto z=unsigned(std::clamp((point.z-terrain.min_z)/(terrain.max_z-terrain.min_z)*float(terrain.height-1),0.f,float(terrain.height-1)));
    return terrain.attributes[size_t(z)*terrain.width+x]&(2|8);
}
bool Battle::terrain_clear(Vec3 a,Vec3 b) const {
    if(!terrain.width)return true;
    auto cell=[&](Vec3 p){return std::array<int,2>{
        int(std::lround(std::clamp((p.x-terrain.min_x)/(terrain.max_x-terrain.min_x)*float(terrain.width-1),0.f,float(terrain.width-1)))),
        int(std::lround(std::clamp((p.z-terrain.min_z)/(terrain.max_z-terrain.min_z)*float(terrain.height-1),0.f,float(terrain.height-1))))};};
    auto start=cell(a),end=cell(b);int dx=std::abs(end[0]-start[0]),sx=start[0]<end[0]?1:-1,dz=-std::abs(end[1]-start[1]),sz=start[1]<end[1]?1:-1,error=dx+dz,steps=std::max(dx,-dz),n=0;
    float ay=terrain.heights[size_t(start[1])*terrain.width+start[0]],by=terrain.heights[size_t(end[1])*terrain.width+end[0]];
    for(;;){float line=ay+(by-ay)*float(n)/std::max(1,steps);if(terrain.heights[size_t(start[1])*terrain.width+start[0]]>line+.25f)return false;if(start==end)break;int twice=2*error;if(twice>=dz){error+=dz;start[0]+=sx;}if(twice<=dx){error+=dx;start[1]+=sz;}++n;}return true;
}
bool Battle::occupied(const Unit& self,Vec3 point) const {for(const auto& other:units)if(&other!=&self && other.regiment.alive && !other.routing && length(Vec3{other.position.x-point.x,0,other.position.z-point.z})<5)return true;return false;}
bool Battle::can_deploy(Vec3 point,unsigned troops) const {
    if(deployment.empty())return true;
    for(unsigned i=0;i<troops;++i){float x=point.x+(float(i%5)-2)*1.2f,z=point.z+float(i/5)*1.2f;
        if(terrain_blocked({x,0,z}))return false;
        if(!std::any_of(deployment.begin(),deployment.end(),[&](const BattleRegion& r){return r.contains(x-.45f,z-.45f)&&r.contains(x+.45f,z+.45f)&&r.contains(x-.45f,z+.45f)&&r.contains(x+.45f,z-.45f);}))return false;
    }return true;
}
bool Battle::is_artillery(const Unit& u) const {return (u.regiment.unit_class&0xf8)==32;}
bool Battle::can_shoot(const Unit& u) const {return u.regiment.missile_weapon!=0 || is_artillery(u);}
float Battle::shoot_range(const Unit& u) const {return is_artillery(u)?150.f:70.f;}
bool Battle::can_fire_at(const Unit& u,Vec3 point) const {Vec3 delta=point-u.position;delta.y=0;return phase==Phase::Battle && is_artillery(u) && u.regiment.alive && !u.routing && u.cooldown==0 && length(delta)<=shoot_range(u) && terrain_clear(u.position,point);}
bool Battle::fire_artillery(Vec3 point){bool fired=false;for(size_t i=0;i<units.size();++i){auto& u=units[i];if(!u.selected || !can_fire_at(u,point))continue;int target=-1;for(size_t j=0;j<units.size();++j)if(units[j].enemy && units[j].regiment.alive && length(units[j].position-point)<6){target=int(j);break;}Vec3 start=u.position+Vec3{0,2.5f,0};projectiles.push_back({start,start,point+Vec3{0,.8f,0},int(i),target,0,std::clamp(length(point-start)/95.f,.25f,1.5f),target>=0,true,false,3});u.cooldown=5;fired=true;}return fired;}
bool Battle::order(Vec3 point,int target) {
    if(phase==Phase::Victory || phase==Phase::Defeat)return false;
    size_t ordinal=0;
    if(phase==Phase::Deployment)for(const auto& u:units)if(u.selected && !u.enemy && u.regiment.alive && !u.routing){if(!can_deploy(point+Vec3{float(ordinal++)*7,0,0},u.regiment.alive))return false;}
    ordinal=0;bool ordered=false;
    for(auto& u:units)if(u.selected && !u.enemy && u.regiment.alive && !u.routing) {
        auto destination=point+Vec3{float(ordinal++)*7,0,0};if(terrain_blocked(destination) || !terrain_clear(u.position,destination) || (target<0 && occupied(u,destination)))continue;
        u.destination=destination;u.target=target;u.moving=true;ordered=true;
        if(u.command==UnitCommand::Halt || u.command==UnitCommand::Break)u.command=UnitCommand::Automatic;
        if(u.command==UnitCommand::Shoot && target>=0)u.moving=false;
        if(phase==Phase::Deployment) {u.position=u.destination;u.moving=false;u.target=-1;}
    }return ordered;
}
bool Battle::command(UnitCommand action){
    if(phase!=Phase::Battle)return false;
    bool applied=false;
    for(auto& u:units)if(u.selected && !u.enemy && u.regiment.alive && !u.routing){
        if(action==UnitCommand::Shoot && !can_shoot(u))continue;
        if(action==UnitCommand::Charge){
            if(u.target<0 || size_t(u.target)>=units.size() || !units[size_t(u.target)].enemy || !units[size_t(u.target)].regiment.alive){
                float nearest=176.f;
                for(size_t i=0;i<units.size();++i){const auto& other=units[i];if(!other.enemy || !other.regiment.alive || other.routing)continue;
                    Vec3 delta=other.position-u.position;delta.y=0;float distance=length(delta);if(distance<nearest){nearest=distance;u.target=int(i);}}
            }
            if(u.target<0)continue;
            u.destination=units[size_t(u.target)].position;u.moving=true;
        }
        u.command=action;applied=true;
        if(action==UnitCommand::Halt){u.moving=false;u.target=-1;u.destination=u.position;}
        if(action==UnitCommand::Shoot)u.moving=false;
        if(action==UnitCommand::Charge)u.charge_time=8;
        if(action==UnitCommand::Break){Vec3 away{0,0,-1};if(u.target>=0 && size_t(u.target)<units.size())away=normal(u.position-units[size_t(u.target)].position);u.target=-1;u.destination=u.position+away*25;u.moving=true;}
    }return applied;
}
bool Battle::can_cast_magic(const Unit& u) const {
    if(phase!=Phase::Battle || !magic_power || u.enemy || !u.selected || !u.regiment.alive || u.routing || !u.regiment.wizard)return false;
    if(u.target>=0 && size_t(u.target)<units.size()){const auto& target=units[size_t(u.target)];if(target.enemy && target.regiment.alive && !target.routing){Vec3 delta=target.position-u.position;delta.y=0;if(length(delta)<=120)return true;}}
    for(const auto& target:units)if(target.enemy && target.regiment.alive && !target.routing){Vec3 delta=target.position-u.position;delta.y=0;if(length(delta)<=120)return true;}
    return false;
}
bool Battle::cast_magic(){
    if(phase!=Phase::Battle || !magic_power)return false;
    for(size_t i=0;i<units.size();++i){
        auto& wizard=units[i];if(!can_cast_magic(wizard))continue;
        int target=wizard.target;
        if(target<0 || size_t(target)>=units.size() || !units[size_t(target)].enemy || !units[size_t(target)].regiment.alive){
            float nearest=120.f;target=-1;
            for(size_t j=0;j<units.size();++j){const auto& hostile=units[j];if(!hostile.enemy || !hostile.regiment.alive || hostile.routing)continue;
                Vec3 delta=hostile.position-wizard.position;delta.y=0;float distance=length(delta);if(distance<nearest){nearest=distance;target=int(j);}}
        }
        if(target<0)continue;
        wizard.target=target;
        const auto& hostile=units[size_t(target)];Vec3 start=wizard.position+Vec3{0,2.5f,0};Vec3 end=hostile.position+Vec3{0,.8f,0};
        float flight=std::clamp(length(end-start)/95.f,.25f,1.25f);
        projectiles.push_back({start,start,end,int(i),target,0,flight,true,false,true,3});
        --magic_power;return true;
    }
    return false;
}
void Battle::tick() {
    if(phase!=Phase::Battle)return;
    constexpr float dt=1.f/30; ++ticks;
    // Provisional Winds-of-Magic host cycle, shared with the UI countdown.
    magic_countdown-=dt;if(magic_countdown<=0){magic_countdown+=30;if(std::any_of(units.begin(),units.end(),[](const Unit& u){return !u.enemy && u.regiment.alive && u.regiment.wizard;}))magic_power=std::min(magic_capacity,magic_power+1+unsigned(random()%3));}
    if(scripts){scripts->tick(*this);if(phase!=Phase::Battle)return;}
    std::vector<unsigned> damage(units.size());
    // Flight and impact are separate from firing, so volleys do not deal damage instantly.
    for(auto& shot:projectiles){
        shot.age+=dt;float t=std::min(1.f,shot.age/shot.duration);shot.position=shot.start*(1-t)+shot.end*t;
        if(t==1 && shot.hit && shot.target>=0 && size_t(shot.target)<units.size() && units[size_t(shot.target)].regiment.alive)damage[size_t(shot.target)]+=shot.damage;
    }
    projectiles.erase(std::remove_if(projectiles.begin(),projectiles.end(),[](const Projectile& shot){return shot.age>=shot.duration;}),projectiles.end());
    auto rolls_to_wound=[&](const Unit& attacker,const Unit& defender){
        unsigned strength=std::max(1u,unsigned(attacker.regiment.stats[3])),toughness=std::max(1u,unsigned(defender.regiment.stats[4]));
        if(strength>=toughness*2)return 2u;
        if(strength>toughness)return 3u;
        if(strength==toughness)return 4u;
        if(strength*2<=toughness)return 6u;
        return 5u;
    };
    auto resolve_missile=[&](const Unit& attacker,const Unit& defender){
        unsigned ballistic=std::max(1u,unsigned(attacker.regiment.stats[2]));unsigned hit=std::clamp(7-int(ballistic),2,6);
        if(random()%6+1<hit || random()%6+1<rolls_to_wound(attacker,defender))return false;
        // ARM armour encoding has not been fully recovered; use the low three bits as a provisional save class.
        unsigned save=attacker.regiment.unit_class&0xf8?7u:std::clamp(7-int(defender.regiment.armour&7),2,7);
        return save==7 || random()%6+1<save;
    };
    for(size_t i=0;i<units.size();++i) {
        auto& u=units[i];if(!u.regiment.alive)continue;
        u.cooldown=std::max(0.f,u.cooldown-dt);u.charge_time=std::max(0.f,u.charge_time-dt);if(u.command==UnitCommand::Charge && u.charge_time==0)u.command=UnitCommand::Automatic;
        if(u.routing) {u.engaged=false;u.position.z+=(u.enemy?1:-1)*12*dt;continue;}
        if(u.target>=0 && (size_t(u.target)>=units.size() || !units[u.target].regiment.alive || units[u.target].routing)){u.target=-1;u.engaged=false;}
        ++u.animation_tick;
        bool ranged=u.command==UnitCommand::Shoot || (u.enemy && can_shoot(u));
        if(u.target<0 && u.command!=UnitCommand::Halt && u.command!=UnitCommand::Break && ((!scripts && u.enemy) || (!u.enemy && !u.moving))) {
            float nearest=ranged?shoot_range(u):(!scripts && u.enemy)?1e30f:12.f;
            for(size_t j=0;j<units.size();++j) {
                const auto& other=units[j];if(other.enemy==u.enemy || !other.regiment.alive || other.routing)continue;
                Vec3 offset=other.position-u.position;offset.y=0;float d=length(offset);if(d<nearest) {nearest=d;u.target=int(j);}
            }
        }
        if(u.target>=0) {u.destination=units[u.target].position;u.moving=!ranged;}
        Vec3 delta=u.destination-u.position;delta.y=0;float distance=length(delta);
        if(u.target>=0 && distance<(ranged?shoot_range(u):5)) {
            u.moving=false;
            if(!ranged){u.engaged=true;units[size_t(u.target)].engaged=true;}
            if(u.cooldown==0) {
                if(ranged){
                    const auto& target=units[size_t(u.target)];unsigned shots=is_artillery(u)?1:std::min(24u,unsigned(u.regiment.alive));
                    for(unsigned shot=0;shot<shots && projectiles.size()<128;++shot){
                        Vec3 start=u.position+Vec3{(float(shot%5)-2)*.7f,1.5f,float(shot/5)*.7f};
                        Vec3 end=target.position+Vec3{float(int(random()%7)-3)*.35f,.5f,float(int(random()%7)-3)*.35f};
                        float flight=std::clamp(length(end-start)/70.f,.18f,1.5f);
                        projectiles.push_back({start,start,end,int(i),u.target,0,flight,resolve_missile(u,target)&&terrain_clear(start,end),is_artillery(u)});
                    }
                    u.cooldown=is_artillery(u)?5.f:3.f;++ranged_attacks;
                }else{
                    // Representative melee hit/wound/save stages, not the original lookup tables.
                    unsigned hits=0;for(unsigned n=0;n<u.regiment.alive;++n)if(random()%6+1>=4 && random()%6+1>=4 && random()%6+1<5)++hits;
                    damage[size_t(u.target)]+=hits;u.cooldown=1.f;++attacks;
                }
            }
        } else if(u.moving) {
            if(distance>0.001f)u.heading=std::atan2(delta.x,delta.z);
            float step=std::min(distance,(u.command==UnitCommand::Charge && u.charge_time>0?13.f:8.f)*dt);auto next=u.position+normal(delta)*step;if(terrain_blocked(next) || !terrain_clear(u.position,next) || (u.target<0 && occupied(u,next))){u.moving=false;u.destination=u.position;}else u.position=next;
            if(distance<=step){u.moving=false;if(u.command==UnitCommand::Break)u.command=UnitCommand::Halt;}
        }
    }
    bool friendly=false,hostile=false;
    for(size_t i=0;i<units.size();++i) {
        auto& u=units[i];unsigned lost=std::min<unsigned>(u.regiment.alive,damage[i]);u.regiment.alive-=lost;
        u.morale=std::max(0.f,u.morale-float(lost)*8);
        if(u.morale<25 && !(u.regiment.attributes&1))u.routing=true;
        if(u.regiment.alive && !u.routing) {if(u.enemy)hostile=true;else friendly=true;}
    }
    if(!friendly)phase=Phase::Defeat;else if(!hostile)phase=Phase::Victory;
}
}
