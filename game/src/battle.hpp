#pragma once
#include "formats.hpp"
#include "math3d.hpp"
#include <random>
#include <memory>
#include "ctl.hpp"
namespace neo {
enum class UnitCommand {Automatic,Halt,Shoot,Break,Charge};
struct Unit {
    Regiment regiment;
    math3d::Vec3 position, destination;
    bool enemy=false, selected=false, moving=false, routing=false, engaged=false;
    float morale=100, cooldown=0,heading=0;
    uint64_t animation_tick=0;
    int target=-1;
    UnitCommand command=UnitCommand::Automatic;
    float charge_time=0;
};
enum class Phase {Deployment, Battle, Victory, Defeat};
struct Projectile {
    math3d::Vec3 start,position,end;
    int source=-1,target=-1;
    float age=0,duration=0;
    bool hit=false,artillery=false,magic=false;
    unsigned damage=1;
};
// Original mission scripts drive a provisional deterministic combat host.
class Battle {
public:
    std::vector<Unit> units;
    Phase phase=Phase::Deployment;
    uint64_t ticks=0;
    unsigned attacks=0;
    unsigned ranged_attacks=0;
    std::vector<Projectile> projectiles;
    struct SpeechRequest {size_t unit=0;unsigned clip=0;uint64_t tick=0;};
    std::deque<SpeechRequest> speech_queue;
    bool voice_active=false;uint64_t generation=0;
    void say(size_t unit,unsigned clip);

    void reset(const Army&,const Army&,math3d::Vec3 center,float radius);
    unsigned deploy(const BattleSetup&);
    void attach_script(const prj::Bytes&,const BattleSetup&);
    std::shared_ptr<CtlRuntime> scripts;
    void start();
    void set_terrain(unsigned,unsigned,float,float,float,float,std::vector<uint8_t>,std::vector<float>);
    bool order(math3d::Vec3 point,int target=-1);
    bool command(UnitCommand);
    // Cast the selected wizard's provisional Arcane Bolt at a nearby hostile unit.
    bool cast_magic();
    bool can_cast_magic(const Unit&) const;
    bool can_shoot(const Unit&) const;
    bool is_artillery(const Unit&) const;
    bool can_fire_at(const Unit&,math3d::Vec3) const;
    bool fire_artillery(math3d::Vec3);
    float shoot_range(const Unit&) const;
    bool can_deploy(math3d::Vec3,unsigned troops) const;
    std::vector<BattleRegion> deployment;
    float magic_countdown=30;unsigned magic_power=0;
    static constexpr unsigned magic_capacity=10;
    void tick();
private:
    struct Terrain {unsigned width=0,height=0;float min_x=0,max_x=0,min_z=0,max_z=0;std::vector<uint8_t> attributes;std::vector<float> heights;} terrain;
    bool terrain_clear(math3d::Vec3,math3d::Vec3) const;
    bool terrain_blocked(math3d::Vec3) const;
    bool occupied(const Unit&,math3d::Vec3) const;
    std::mt19937 random{1};
};
}
