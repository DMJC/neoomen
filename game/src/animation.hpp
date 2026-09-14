#pragma once
#include "executable.hpp"
namespace neo {
// Visual frame runs from the original executable's animation bytecode.
// Gameplay side effects of animation instructions belong to the battle host.
struct AnimationClip {unsigned base=0;std::vector<unsigned> frames;};
struct UnitAnimation {
    std::string filename;
    AnimationClip idle,walk,attack;
    static UnitAnimation load(const Executable&,unsigned sprite);
    unsigned frame(bool moving,bool attacking,uint64_t tick,unsigned direction) const;
};
}
