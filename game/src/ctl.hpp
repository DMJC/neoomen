#pragma once
#include "formats.hpp"
#include <array>
#include <deque>
#include <map>
namespace neo {
class Battle;
class CtlProgram {
public:
    explicit CtlProgram(const prj::Bytes&);
    uint32_t function(uint32_t id) const;
    uint32_t word(uint32_t at) const;
    unsigned width(uint32_t at) const;
    size_t size() const {return code.size();}
private:
    std::vector<uint32_t> code;
};
struct CtlEvent {uint16_t id=0;int source=-1;std::array<int16_t,5> args{};};
struct CtlState {
    uint32_t function=0,ip=0,saved_function=0,saved_ip=0,return_function=0;
    uint32_t flag1=1,flag2=0,flag3=0,label=0;
    uint16_t control=0,timer=0,event_handler=0;
    int16_t x=0;
    std::array<int32_t,8> registers{};
    std::vector<uint32_t> stack;
    std::deque<CtlEvent> events;
    CtlEvent event;
    int stored=-1;
    bool active=false,in_event=false;
    std::vector<uint32_t> abilities;
    uint64_t instructions=0;
};
class CtlRuntime {
public:
    CtlRuntime(const prj::Bytes&,const BattleSetup&,const Battle&);
    void tick(Battle&);
    void send(size_t target,CtlEvent);
    std::vector<CtlState> states;
    std::array<int32_t,16> globals{};
    uint64_t instructions=0;
private:
    CtlProgram program;
    BattleSetup setup;
    uint32_t random=1;
    void run(Battle&,size_t);
};
}
