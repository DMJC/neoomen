#include "ctl.hpp"
#include "battle.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
using namespace math3d;
namespace neo {
namespace {
constexpr unsigned widths[240]={
#include "ctl_widths.inc"
};
}
CtlProgram::CtlProgram(const prj::Bytes& bytes) {
    if(bytes.size()<8 || bytes.size()%4 || bytes.size()>4*1024*1024)throw std::runtime_error("Invalid CTL word stream");
    for(size_t i=0;i<bytes.size();i+=4)code.push_back(prj::u32(bytes,i));
    if(code[0]==0 || code[0]>=100 || code[0]*2>=code.size())throw std::runtime_error("Invalid CTL function table");
}
uint32_t CtlProgram::word(uint32_t at) const {if(at>=code.size())throw std::runtime_error("CTL IP outside script");return code[at];}
uint32_t CtlProgram::function(uint32_t id) const {
    uint32_t bias=code[0],index;
    if(id<bias)index=bias+id;else if(id>=100)index=id-99;else throw std::runtime_error("Invalid CTL function ID");
    uint32_t at=word(index),op=word(at)&0xffff;
    if(!at || (op!=0xabc && (op<0x8000 || op>0x80f1)))throw std::runtime_error("Unpopulated CTL function "+std::to_string(id));
    return at;
}
unsigned CtlProgram::width(uint32_t at) const {
    uint32_t op=word(at)&0xffff;if(op==0xabc || op==0x80f1)return 0;
    if(op<0x8000 || op>=0x80f0)throw std::runtime_error("Invalid CTL opcode at word "+std::to_string(at));
    unsigned n=widths[op&0x7fff];word(at+n);return n;
}
CtlRuntime::CtlRuntime(const prj::Bytes& bytes,const BattleSetup& initial,const Battle& battle):program(bytes),setup(initial) {
    states.resize(battle.units.size());
    for(size_t i=0;i<states.size();++i)for(const auto& n:setup.nodes)if((n.flags&2) && n.unit_id==battle.units[i].regiment.id){auto& s=states[i];s.function=n.script_id;program.function(s.function);s.active=true;break;}
}
void CtlRuntime::send(size_t target,CtlEvent event){if(target<states.size()){if(states[target].events.size()>=64)throw std::runtime_error("CTL event queue overflow");states[target].events.push_back(event);}}
void CtlRuntime::tick(Battle& battle) {
    for(size_t i=0;i<states.size();++i)if(states[i].active && battle.units[i].regiment.alive){if(states[i].timer)--states[i].timer;run(battle,i);}
}
void CtlRuntime::run(Battle& battle,size_t index) {
    auto& s=states[index];auto& unit=battle.units[index];
    auto push=[&](uint32_t n){if(s.stack.size()>=48)throw std::runtime_error("CTL stack overflow");s.stack.push_back(n);};
    auto pop=[&](){if(s.stack.empty())throw std::runtime_error("CTL stack underflow");auto n=s.stack.back();s.stack.pop_back();return n;};
    auto jump=[&](uint32_t f){program.function(f);s.function=f;s.ip=0;};
    auto condition=[&](bool b){s.control=uint16_t((s.control&~4u)|(b?4:0));};
    auto reg=[&](uint32_t r)->int32_t& {if(r>=s.registers.size())throw std::runtime_error("CTL register index");return s.registers[r];};
    auto global=[&](uint32_t r)->int32_t& {if(r>=globals.size())throw std::runtime_error("CTL global index");return globals[r];};
    auto node=[&](uint32_t n)->const BattleNode& {if(n>=setup.nodes.size())throw std::runtime_error("CTL node index");return setup.nodes[n];};
    auto find_unit=[&](uint32_t id){for(size_t i=0;i<battle.units.size();++i)if(battle.units[i].regiment.id==id)return int(i);return -1;};
    auto search=[&](float radius){int target=-1;float nearest=radius;for(size_t j=0;j<battle.units.size();++j){const auto& other=battle.units[j];if(other.enemy==unit.enemy || !other.regiment.alive || other.routing)continue;auto delta=other.position-unit.position;delta.y=0;float d=length(delta);if(d<nearest){nearest=d;target=int(j);}}if(target>=0){unit.target=target;unit.moving=true;}condition(target>=0);return target;};
    if(!s.in_event && s.event_handler && !s.events.empty()) {push(s.ip);push(s.function);s.in_event=true;jump(s.event_handler);}
    for(unsigned budget=0;budget<4096;++budget) {
        auto base=program.function(s.function),at=base+s.ip;uint32_t op=program.word(at)&0xffff;
        if(op==0x80f1){s.active=false;return;}if(op==0xabc){++s.ip;continue;}
        unsigned n=program.width(at);std::array<uint32_t,4> a{};if(n>a.size())throw std::runtime_error("CTL operand count");for(unsigned i=0;i<n;++i)a[i]=program.word(at+1+i);
        uint32_t old=s.ip;s.ip+=1+n;op&=0x7fff;++s.instructions;++instructions;bool truth=(s.control&4)!=0;
        auto wait=[&](bool blocked){if(blocked)s.ip=old;return blocked;};
        auto skip=[&](bool allow_else){unsigned nesting=0;for(unsigned b=0;b<8192;++b){uint32_t p=program.function(s.function)+s.ip,v=program.word(p)&0xffff;if(v==0x80f1)throw std::runtime_error("Unterminated CTL conditional");s.ip+=1+program.width(p);if(v==0x8076 || v==0x8077)++nesting;else if(v==0x8079){if(!nesting)return;--nesting;}else if(v==0x8078 && !nesting && allow_else)return;}throw std::runtime_error("CTL conditional budget");};
        switch(op) {
            case 0:s.timer=0;s.stack.clear();s.registers.fill(0);s.saved_function=s.function;s.saved_ip=s.ip;s.flag1=(s.flag1&0x7effdd63)|0x10;break;
            case 1:if(battle.phase==Phase::Deployment){s.flag1|=0x80000000;s.ip=old;return;}s.flag1&=0x7fffffff;break;
            case 2:s.stack.clear();break;
            case 3:s.function=s.saved_function;s.ip=s.saved_ip;break;
            case 4:s.saved_function=s.function;s.saved_ip=s.ip;break;
            case 5:push(old);break;
            case 6:s.ip=pop();break;
            case 7:{auto target=pop();if(truth)s.ip=target;break;}
            case 8:{auto target=pop();if(!truth)s.ip=target;break;}
            case 9:push(s.ip);push(uint16_t(a[0]));break;
            case 10:{auto count=pop();if(count!=1){push(uint16_t(count-1));if(s.stack.size()<2)throw std::runtime_error("CTL loop stack");s.ip=s.stack[s.stack.size()-2];}else pop();break;}
            case 11:jump(a[0]);break;
            case 12:case 13:case 14:case 15:case 16:
                if(op==12 || op==14 || ((op==13 || op==15)&&truth) || (op==16 && !truth)){s.return_function=a[0];s.control|=8;if(op==14 || op==15)s.control|=32;}break;
            case 17:push(s.ip);push(s.function);jump(a[0]);break;
            case 18:case 19:{auto f=pop(),ip=pop();s.function=f;s.ip=ip;if(op==19){s.in_event=false;if(s.control&8){jump(s.return_function);s.control&=~40u;}}break;}
            case 20:s.registers[7]=int16_t(a[0]);break;
            case 21:return;
            case 22:if(truth)return;break;
            case 23:if(truth)s.ip=uint32_t(int64_t(s.ip)+int16_t(a[0]));break;
            case 24:s.timer=uint16_t(a[0]);break;
            case 25:condition(s.timer!=0);break;
            case 26:if(wait(s.timer!=0))return;break;
            case 27:s.x=int16_t(a[0]);break;
            case 28:s.x=int16_t(s.x+int16_t(a[0]));break;
            case 29:condition(s.x==0);break;
            case 30:reg(a[0])=int16_t(a[1]);break;
            case 31:reg(a[0])=int32_t(uint32_t(reg(a[0]))+uint32_t(int32_t(int16_t(a[1]))));break;
            case 32:condition(reg(a[0])==int16_t(a[1]));break;
            case 33:condition(reg(a[0])==reg(a[1]));break;
            case 34:random=random*1664525u+1013904223u;reg(a[0])=int32_t(random%10)+1;break;
            case 35:global(a[0])=int32_t(a[1]);break;
            case 36:global(a[0])=int32_t(uint32_t(global(a[0]))+a[1]);break;
            case 37:condition(global(a[0])==int32_t(a[1]));break;
            case 38:break; // Pending orders are applied by the Battle adapter.
            case 39:s.function=s.saved_function;s.ip=s.saved_ip;break;
            case 40:case 41:{const auto& p=node(a[0]);unit.destination={p.x/8.f,unit.position.y,p.z/8.f};unit.moving=true;unit.target=-1;break;}
            case 42:{auto it=std::find_if(setup.nodes.begin(),setup.nodes.end(),[&](const BattleNode& p){return p.group==a[0];});if(it!=setup.nodes.end()){unit.destination={it->x/8.f,unit.position.y,it->z/8.f};unit.moving=true;unit.target=-1;}break;}
            case 43:case 101:unit.moving=false;unit.target=-1;break;
            case 44:if(wait((s.flag1&a[0])!=0))return;break;
            case 45:if(wait((s.flag1&a[0])==0))return;break;
            case 46:condition(s.flag1&a[0]);break;
            case 47:s.flag1|=a[0];break;
            case 48:s.flag1&=~a[0];break;
            case 49:if(wait((s.flag2&a[0])!=0))return;break;
            case 50:if(wait((s.flag2&a[0])==0))return;break;
            case 51:condition(s.flag2&a[0]);break;
            case 52:s.flag2|=a[0];break;
            case 53:s.flag2&=~a[0];break;
            case 54:if(wait((s.flag3&a[0])!=0))return;break;
            case 55:if(wait((s.flag3&a[0])==0))return;break;
            case 56:condition(s.flag3&a[0]);break;
            case 57:s.control=uint16_t((a[0]&0x8000)?(a[0]&0x7fff):(s.control|a[0]));break;
            case 58:s.control=uint16_t(s.control&~a[0]);break;
            case 59:condition(s.control&a[0]);break;
            case 60:s.registers[7]=int32_t(a[0]);break; // threat radius retained by host
            case 61:s.event_handler=uint16_t(a[0]);break;
            case 62:break; // Periodic action scheduling is not yet part of the combat adapter.
            case 63:s.label=a[0];break;
            case 64:s.stored=-1;for(size_t j=0;j<states.size();++j)if(states[j].label==a[0]){s.stored=int(j);break;}break;
            case 65:condition(std::any_of(states.begin(),states.end(),[&](const CtlState& st){return st.label==a[0];}));break;
            case 66:if(std::any_of(states.begin(),states.end(),[&](const CtlState& st){return st.label==a[0];}))send(index,{uint16_t(a[1]),int(index),{}});break;
            case 76:break;
            case 82:break; // Individual-member wander: sprite formation remains host-owned.
            case 83:{const auto& p=node(a[0]);if(p.flags&1){unit.position={p.x/8.f,unit.position.y,p.z/8.f};unit.destination=unit.position;unit.heading=p.heading*6.2831853f/512;unit.moving=false;}break;}
            case 99:unit.cooldown=0;break;
            case 103:unit.moving=false;break;
            case 104:unit.heading+=int16_t(a[0])*6.2831853f/512;break;
            case 105:case 106:case 107:if(op==105 || (op==106&&truth) || (op==107&&!truth))send(index,{uint16_t(a[0]),int(index),{}});break;
            case 108:s.stored=s.event.source;break;
            case 109:s.event.id=uint16_t(a[0]);break;
            case 110:case 111:for(size_t j=0;j<states.size();++j)if((battle.units[j].enemy==unit.enemy)==(op==110))send(j,{uint16_t(a[0]),int(index),{}});break;
            case 112:if(s.stored>=0)send(size_t(s.stored),{uint16_t(a[0]),int(index),{}});break;
            case 113:s.flag1&=~8u;s.flag3&=~1u;break;
            case 114:if(s.events.empty())s.event={};else {s.event=s.events.front();s.events.pop_front();}break;
            case 115:condition(!s.events.empty());break;
            case 116:if(s.event.id!=a[0]){for(unsigned k=0;k<8192;++k){auto p=program.function(s.function)+s.ip;auto v=program.word(p)&0xffff;s.ip+=1+program.width(p);if(v==0x8075)break;if(k==8191)throw std::runtime_error("CTL missing end_event");}}break;
            case 117:if((a[0]&~0x1000u)!=0xdef){auto marker=a[0]&~0x1000u;for(unsigned k=0;k<8192;++k){auto p=program.function(s.function)+s.ip;if(program.word(p)==marker)break;s.ip+=1+program.width(p);if(k==8191)throw std::runtime_error("CTL missing event marker");}}break;
            case 118:if(!truth)skip(true);break;
            case 119:if(truth)skip(true);break;
            case 120:skip(false);break;
            case 121:break;
            case 146:break; // Original Nop92.
            case 157:if(std::find(s.abilities.begin(),s.abilities.end(),a[0])==s.abilities.end())s.abilities.push_back(a[0]);break;
            case 206:unit.regiment.attributes|=a[0];break;
            case 207:unit.regiment.attributes&=~a[0];break;
            case 181:{const auto& p=node(a[1]);condition(std::any_of(battle.units.begin(),battle.units.end(),[&](const Unit& u){return u.regiment.alive && (u.regiment.race&0xe0)==a[0] && !(states[size_t(&u-battle.units.data())].flag1&a[2]) && std::hypot(u.position.x-p.x/8.f,u.position.z-p.z/8.f)<=std::max(1.f,p.radius/8.f);}));break;}
            case 141:condition(unit.regiment.missile_weapon && (a[0]==0xffffffff || unit.regiment.missile_weapon==a[0]));break;
            case 162:case 163:case 164:case 165:case 166:case 167:case 168:case 169:case 170:case 171:case 172:search(512.f);break;
            case 174:battle.say(index,uint16_t(a[0]));break;
            case 175:{int j=find_unit(a[0]);condition(j>=0);if(j>=0)battle.say(size_t(j),uint16_t(a[1]));break;}
            case 179:{const auto& p=node(a[0]);condition(std::hypot(unit.position.x-p.x/8.f,unit.position.z-p.z/8.f)<=std::max(1.f,p.radius/8.f));break;}
            case 180:{const auto& p=node(a[0]);condition(std::any_of(battle.units.begin(),battle.units.end(),[&](const Unit& u){return u.regiment.alive && std::hypot(u.position.x-p.x/8.f,u.position.z-p.z/8.f)<=std::max(1.f,p.radius/8.f);}));break;}
            case 182:for(size_t j=0;j<states.size();++j)if(states[j].label==a[0])send(j,{uint16_t(a[1]),int(index),{}});break;
            case 186:condition((unit.regiment.unit_class&0xf8)==a[0]);break;
            case 208:condition(search(30.f)>=0);break;
            case 213:{unsigned count=0;for(const auto& u:battle.units)if((u.regiment.race&0xe0)==a[0] && u.regiment.alive && !u.routing)++count;condition(count<a[1]);break;}
            case 214:condition(std::any_of(battle.units.begin(),battle.units.end(),[&](const Unit& u){return (u.regiment.race&0xe0)==a[0] && u.regiment.alive && !u.routing;}));break;
            case 215:case 216:{int j=find_unit(a[0]);condition(j>=0 && battle.units[size_t(j)].regiment.alive);break;}
            case 224:case 225:case 235:{int j=find_unit(a[0]);condition(j>=0 && ((op==224?states[size_t(j)].flag2:op==225?states[size_t(j)].flag3:states[size_t(j)].flag1)&a[1]));break;}
            case 226:condition(unit.selected);break;
            case 234:condition(battle.voice_active || !battle.speech_queue.empty());break;
            case 237:battle.phase=Phase::Victory;return;
            default:{std::ostringstream out;out<<"Unsupported CTL opcode 0x"<<std::hex<<op<<" in function "<<std::dec<<s.function<<" at "<<old<<" (unit "<<unit.regiment.id<<")";throw std::runtime_error(out.str());}
        }
    }
    throw std::runtime_error("CTL instruction budget exhausted");
}
}
