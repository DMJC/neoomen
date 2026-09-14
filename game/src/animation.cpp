#include "animation.hpp"
#include <stdexcept>
namespace neo {
UnitAnimation UnitAnimation::load(const Executable& exe,unsigned sprite) {
    if(sprite<1 || sprite>94)throw std::runtime_error("Unknown original sprite ID");
    UnitAnimation out;out.filename=exe.string(0x4ceb7c+(sprite-1)*44+4);
    auto indexBytes=exe.bytes(0x4ebc88+sprite*2,2);
    int index=int16_t(unsigned(indexBytes[0])|unsigned(indexBytes[1])<<8);
    if(index<0)throw std::runtime_error("Sprite has no unit animation table");
    auto clip=[&](unsigned state) {
        AnimationClip c;uint32_t address=exe.word(0x4eb358+(index+state)*4);
        // Initial visual run: opcode 9 selects the directional frame bank;
        // negative shorts emit ~value for one simulation tick each.
        for(unsigned i=0;i<128;++i) {
            auto bytes=exe.bytes(address+i*2,2);int16_t op=int16_t(unsigned(bytes[0])|unsigned(bytes[1])<<8);
            if(op==9 && c.frames.empty()) {auto b=exe.bytes(address+(++i)*2,2);c.base=unsigned(b[0])|unsigned(b[1])<<8;}
            else if(op<0)c.frames.push_back(unsigned(~op));
            else if(!c.frames.empty())break;
        }
        if(c.frames.empty())throw std::runtime_error("Animation has no visual frame run");
        return c;
    };
    out.idle=clip(1);out.walk=clip(3);out.attack=clip(4);return out;
}
unsigned UnitAnimation::frame(bool moving,bool attacking,uint64_t tick,unsigned direction) const {
    const auto& c=moving?walk:attacking?attack:idle;
    return c.base+c.frames.at(tick%c.frames.size())*8+(direction&7);
}
}
