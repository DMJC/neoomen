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
    bool hit=false,artillery=false;
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
    bool order(math3d::Vec3 point,int target=-1);
    bool command(UnitCommand);
    bool can_shoot(const Unit&) const;
    float shoot_range(const Unit&) const;
    bool can_deploy(math3d::Vec3,unsigned troops) const;
    std::vector<BattleRegion> deployment;
    float magic_countdown=30;unsigned magic_power=0;
    static constexpr unsigned magic_capacity=10;
    void tick();
private:
    std::mt19937 random{1};
};
}
