#pragma once
#include "executable.hpp"
#include <map>
namespace neo {
enum class CampaignState {Running, Dialogue, Choice, Mission, Finished};
enum class CampaignScreen {Text,Meeting,Book,Map,TalkingHead,Movie,Debrief};
struct CampaignPresentation {
    CampaignScreen screen=CampaignScreen::Text;
    std::string background,speech,map_icons,dot_file;
    unsigned head=0,expression=0,route=0;
    std::vector<std::array<int32_t,3>> markers;
};
class Campaign {
public:
    explicit Campaign(const std::filesystem::path& root);
    void advance();
    void answer(unsigned choice=0);
    void complete_mission(bool victory,const Army& survivors);
    Army active_army() const;
    void save(const std::filesystem::path&) const;
    void restore(const std::filesystem::path&);
    CampaignState state=CampaignState::Running;
    Army army;
    CampaignPresentation presentation;
    const std::vector<uint8_t>& magic_items() const {return magic;}
    bool loaded_original_save() const {return original_save;}
    std::string message,mission;
    std::vector<std::string> choices;
    unsigned completed=0;
    uint64_t instructions=0;
    uint32_t pc() const {return cursor;}
private:
    Executable executable;
    uint32_t cursor=0x4c3d68,mission_index=0;
    int32_t value=0;
    std::vector<uint32_t> stack;
    std::map<uint32_t,uint8_t> memory;
    std::map<uint32_t,uint32_t> objectives;
    std::vector<uint32_t> screen_returns;
    std::vector<uint8_t> magic;
    bool original_save=false;
    uint32_t get(uint32_t) const;
    void put(uint32_t,uint32_t);
    unsigned operands(uint32_t op,uint32_t at) const;
    uint32_t pop();
    void push(uint32_t);
    void skip_conditional(bool stop_at_else);
    void prompt(const std::string&);
    void restore_original_save(const prj::Bytes&);
};
}
