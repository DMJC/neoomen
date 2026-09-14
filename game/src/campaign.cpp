#include "campaign.hpp"
#include "m3d.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace neo {
namespace {
const std::map<uint32_t,unsigned> widths={
#include "campaign_opcodes.inc"
};
bool compare(int32_t a,uint32_t op,int32_t b) {
    switch(op){case 1:return a==b;case 2:return a!=b;case 3:return a<b;case 4:return a>b;default:throw std::runtime_error("Invalid campaign comparison");}
}
void valid_pc(uint32_t p){if(p<0x4c3c48 || p>=0x4ccd28 || (p&3))throw std::runtime_error("Campaign PC outside script blob");}
}
Campaign::Campaign(const std::filesystem::path& root):executable(m3d::resolve(root,"PRG_ENG/DarkOmen.exe")) {
    army=Army::decode(read_file(m3d::resolve(root,"GameData/1PARM/PLYR_ALL.ARM")));
    // PLYR_ALL keeps localized names empty. Mission MRC rosters contain them.
    auto missions=m3d::resolve(root,"GameData/1pbat");
    if(!missions.empty())for(const auto& directory:std::filesystem::directory_iterator(missions))if(directory.is_directory())for(const auto& file:std::filesystem::directory_iterator(directory.path())){
        auto name=file.path().filename().string();for(auto& c:name)c=char(std::toupper(static_cast<unsigned char>(c)));
        if(name.size()<7 || name.substr(name.size()-7)!="MRC.ARM")continue;
        auto localized=Army::decode(read_file(file.path()));for(auto& r:army.regiments)if(r.name.empty())for(const auto& source:localized.regiments)if(source.id==r.id && !source.name.empty()){r.name=source.name;r.leader=source.leader;break;}
    }
}
uint32_t Campaign::get(uint32_t address) const {
    uint32_t result=0;
    for(unsigned i=0;i<4;++i){auto it=memory.find(address+i);uint8_t byte=0;if(it!=memory.end())byte=it->second;else if(address+i<0x500000)byte=executable.bytes(address+i,1)[0];result|=uint32_t(byte)<<(i*8);}
    return result;
}
void Campaign::put(uint32_t address,uint32_t value) {
    if(address<0x4ccd28 || address>0x600000-4)throw std::runtime_error("Campaign write outside emulated data");
    for(unsigned i=0;i<4;++i)memory[address+i]=uint8_t(value>>(i*8));
}
unsigned Campaign::operands(uint32_t op,uint32_t at) const {
    auto it=widths.find(op);if(it==widths.end())throw std::runtime_error("Unknown campaign opcode at "+std::to_string(at-4));
    if(op==0x41d2a0){auto n=executable.word(at);if(n>32)throw std::runtime_error("Campaign menu too large");return 2+4*n;}
    return it->second;
}
void Campaign::push(uint32_t v){if(stack.size()>=20)throw std::runtime_error("Campaign stack overflow");stack.push_back(v);}
uint32_t Campaign::pop(){if(stack.empty())throw std::runtime_error("Campaign stack underflow");auto v=stack.back();stack.pop_back();return v;}
void Campaign::skip_conditional(bool stop_at_else) {
    unsigned depth=0;
    for(unsigned budget=0;budget<10000;++budget){valid_pc(cursor);auto op=executable.word(cursor);cursor+=4;unsigned n=operands(op,cursor);cursor+=n*4;
        if(op==0x41db80)++depth;
        if(op==0x41dc30){if(!depth)return;--depth;}
        if(op==0x41dc60 && !depth && stop_at_else)return;
    }
    throw std::runtime_error("Campaign conditional has no terminator");
}
void Campaign::prompt(const std::string& s){presentation.screen=CampaignScreen::Text;presentation.speech.clear();state=CampaignState::Dialogue;message=s;choices.clear();}
void Campaign::advance() {
    if(state!=CampaignState::Running)return;
    for(unsigned budget=0;budget<10000 && state==CampaignState::Running;++budget) {
        valid_pc(cursor);uint32_t at=cursor,op=executable.word(cursor);cursor+=4;unsigned count=operands(op,cursor);
        std::vector<uint32_t> a;for(unsigned i=0;i<count;++i){valid_pc(cursor);a.push_back(executable.word(cursor));cursor+=4;}++instructions;
        auto reg=[&]() -> Regiment* {auto it=std::find_if(army.regiments.begin(),army.regiments.end(),[&](const Regiment& r){return r.id==(a.at(0)&0xffff);});return it==army.regiments.end()?nullptr:&*it;};
        auto string=[&](unsigned i){return executable.string(a.at(i));};
        switch(op) {
            case 0x41db60:cursor=a[0];break;
            case 0x41dc80:push(cursor);cursor=a[0];break;
            case 0x41dcd0:cursor=pop();break;
            case 0x41db80:if(!compare(value,a[0],int32_t(a[1])))skip_conditional(true);break;
            case 0x41dc60:skip_conditional(false);break;
            case 0x41dc30:break;
            case 0x41dd00:push(cursor);break;
            case 0x41dd20:{auto target=pop();if(!compare(value,a[0],int32_t(a[1]))){push(target);cursor=target;}break;}
            case 0x41de30:push(cursor);push(a[0]);break;
            case 0x41de60:{auto n=pop(),target=pop();if(n>1){push(target);push(n-1);cursor=target;}break;}
            case 0x41ddf0:push(uint32_t(value));break;
            case 0x41de10:value=int32_t(pop());break;
            case 0x41de20:value=int32_t(a[0]);break;
            case 0x41dee0:put(a[0],a[1]);break;
            case 0x41df00:value=int32_t(get(a[0]));break;
            case 0x41df20:if(a[1]>65536 || a[0]<0x4ccd28 || uint64_t(a[0])+a[1]>0x600000)throw std::runtime_error("Invalid campaign clear range");for(unsigned i=0;i<a[1];++i)memory[a[0]+i]=0;break;
            case 0x41d280:value=int32_t(a[0]);break;
            case 0x41ce80:if(auto r=reg();r && r->alive)r->status|=1;break;
            case 0x41cfb0:if(auto r=reg())r->status|=8;break;
            case 0x41cff0:if(auto r=reg())r->status&=~8u;break;
            case 0x41d040:if(auto r=reg())r->status|=64;break;
            case 0x41d080:if(auto r=reg())r->status&=~64u;break;
            case 0x41d0d0:if(auto r=reg())r->status|=256;break;
            case 0x41d120:if(auto r=reg())r->status&=~256u;break;
            case 0x41d170:if(auto r=reg())r->status|=512;break;
            case 0x41d1c0:if(auto r=reg())r->status&=~512u;break;
            case 0x41ced0:if(auto r=reg())r->status&=~1u;break;
            case 0x41cdc0:{auto r=reg();value=r && r->alive;break;}
            case 0x41ce20:{auto r=reg();value=r && (r->status&1);break;}
            case 0x41d620:army.gold=uint16_t(army.gold+a[0]);break;
            case 0x41d640:if(magic.size()<40)magic.push_back(uint8_t(a[0]));break;
            case 0x41d690:{auto it=std::find(magic.begin(),magic.end(),uint8_t(a[0]));if(it!=magic.end())magic.erase(it);break;}
            case 0x41d210:value=objectives[a[0]]==1;break;
            case 0x41d250:objectives[a[0]]=a[1];break;
            case 0x41cb20:mission=string(0);mission_index=a[1];state=CampaignState::Mission;message="Deploy for "+mission;break;
            case 0x41d420:prompt("Dialogue: "+string(2));presentation.screen=CampaignScreen::TalkingHead;presentation.head=a[0];presentation.expression=a[1];presentation.speech=string(2);break;
            case 0x41d550:prompt("Message: "+string(0));break;
            case 0x41d740:state=CampaignState::Choice;message="Choose your next action";choices={string(0),string(1)};presentation.screen=CampaignScreen::TalkingHead;presentation.head=a[2];presentation.expression=a[3];presentation.speech=string(4);break;
            case 0x41c970:screen_returns.push_back(cursor);push(cursor);cursor=a[1];prompt("Meeting point: "+string(0));presentation.screen=CampaignScreen::Meeting;presentation.background=string(0);break;
            case 0x41ded0:
                if(!screen_returns.empty()){cursor=screen_returns.back();screen_returns.pop_back();if(!stack.empty() && stack.back()==cursor)stack.pop_back();prompt("Meeting complete - continue campaign");}
                else state=CampaignState::Finished;
                break;
            case 0x41d970:state=CampaignState::Finished;message="Campaign complete";break;
            case 0x41c920:prompt("Movie: "+string(0));presentation.screen=CampaignScreen::Movie;break;
            case 0x41cba0:prompt("Scene: "+string(0));break;
            case 0x41ca00:prompt("Travel: "+string(0));presentation.screen=CampaignScreen::Map;presentation.background=string(0);presentation.map_icons=string(1);presentation.dot_file=string(2);presentation.route=a[3];presentation.markers.clear();for(unsigned i=4;i+2<a.size();i+=3)if(a[i]!=0xffffffff)presentation.markers.push_back({int32_t(a[i]),int32_t(a[i+1]),int32_t(a[i+2])});break;
            case 0x41cab0:prompt("Army book - "+std::to_string(army.gold)+" gold");presentation.screen=CampaignScreen::Book;break;
            case 0x41cb70:put(0x538044,4);prompt("Battle debriefing");presentation.screen=CampaignScreen::Debrief;break;
            case 0x41c9d0:put(0x538044,2);prompt("Continue from the meeting point");break;
            case 0x41cae0:prompt("Campaign checkpoint - F5 saves, F9 restores");break;
            case 0x41cda0:if(a[0]!=0xffffffff)value=int32_t(a[0]);break;
            // Presentation setup has no effect on the campaign's control flow.
            case 0x41cb10:case 0x41d9f0:case 0x41dec0:case 0x41cbe0:case 0x41cc00:case 0x41cce0:
            case 0x41d360:case 0x41d410:case 0x41d720:case 0x41d870:case 0x41d8b0:case 0x41d950:
            case 0x41d8a0:case 0x41d9e0:case 0x41d980:case 0x41d9a0:case 0x41d9c0:case 0x41cb40:case 0x41d2a0:break;
            default:if(op>=0x41da00 && op<=0x41db50 && (op&15)==0)break;
                throw std::runtime_error("Unsupported campaign operation "+std::to_string(op)+" at "+std::to_string(at));
        }
    }
    if(state==CampaignState::Running)throw std::runtime_error("Campaign instruction budget exhausted");
}
void Campaign::answer(unsigned choice) {
    if(state==CampaignState::Choice){if(choice>=choices.size())throw std::runtime_error("Invalid campaign choice");value=int32_t(choice);}
    else if(state!=CampaignState::Dialogue)return;
    state=CampaignState::Running;choices.clear();advance();
}
Army Campaign::active_army() const {Army result=army;result.regiments.clear();for(const auto& r:army.regiments)if((r.status&1) && r.alive)result.regiments.push_back(r);return result;}
void Campaign::complete_mission(bool victory,const Army& survivors) {
    if(state!=CampaignState::Mission)throw std::runtime_error("No campaign mission is running");
    for(const auto& r:survivors.regiments)for(auto& saved:army.regiments)if(r.id==saved.id){saved.alive=r.alive;break;}
    objectives[mission_index]=victory?0:1;value=victory?0:1;++completed;state=CampaignState::Running;advance();
}
void Campaign::save(const std::filesystem::path& path) const {
    std::ostringstream out;out<<"NEOOMEN_CAMPAIGN 2\n"<<cursor<<' '<<value<<' '<<unsigned(state)<<' '<<mission_index<<' '<<completed<<' '<<instructions<<'\n';
    out<<std::quoted(mission)<<' '<<std::quoted(message)<<'\n';
    auto list=[&](const auto& values){out<<values.size();for(auto v:values)out<<' '<<uint32_t(v);out<<'\n';};list(stack);list(screen_returns);list(magic);
    out<<choices.size()<<'\n';for(const auto& s:choices)out<<std::quoted(s)<<'\n';
    out<<memory.size()<<'\n';for(const auto& p:memory)out<<p.first<<' '<<unsigned(p.second)<<'\n';
    out<<objectives.size()<<'\n';for(const auto& p:objectives)out<<p.first<<' '<<p.second<<'\n';
    out<<army.gold<<' '<<army.regiments.size()<<'\n';for(const auto& r:army.regiments)out<<r.id<<' '<<r.status<<' '<<unsigned(r.alive)<<'\n';
    out<<unsigned(presentation.screen)<<' '<<presentation.head<<' '<<presentation.expression<<' '<<presentation.route<<'\n';
    out<<std::quoted(presentation.background)<<' '<<std::quoted(presentation.speech)<<' '<<std::quoted(presentation.map_icons)<<' '<<std::quoted(presentation.dot_file)<<'\n';
    out<<presentation.markers.size()<<'\n';for(const auto& m:presentation.markers)out<<m[0]<<' '<<m[1]<<' '<<m[2]<<'\n';
    auto data=out.str();auto temp=path;temp+=".tmp";{std::ofstream file(temp,std::ios::binary);file<<data;file.close();if(!file)throw std::runtime_error("Cannot save campaign");}std::filesystem::rename(temp,path);
}
void Campaign::restore(const std::filesystem::path& path) {
    // Parse into a copy; malformed saves cannot partially modify the active campaign.
    Campaign next=*this;std::ifstream in(path);std::string magicWord;unsigned version=0,status=0;in>>magicWord>>version;
    if(magicWord!="NEOOMEN_CAMPAIGN" || (version!=1 && version!=2))throw std::runtime_error("Unsupported campaign save");
    in>>next.cursor>>next.value>>status>>next.mission_index>>next.completed>>next.instructions;if(!in)throw std::runtime_error("Truncated campaign save header");valid_pc(next.cursor);
    if(status>unsigned(CampaignState::Finished))throw std::runtime_error("Invalid campaign save state");
    next.state=CampaignState(status);
    in>>std::quoted(next.mission)>>std::quoted(next.message);
    auto size=[&](size_t max){size_t n=0;in>>n;if(!in || n>max)throw std::runtime_error("Invalid campaign save count");return n;};
    auto list=[&](auto& values,size_t max){values.clear();auto n=size(max);for(size_t i=0;i<n;++i){uint32_t v=0;in>>v;values.push_back(v);}};
    list(next.stack,20);list(next.screen_returns,10);list(next.magic,40);next.choices.clear();auto n=size(32);for(size_t i=0;i<n;++i){std::string s;in>>std::quoted(s);next.choices.push_back(s);}
    next.memory.clear();n=size(131072);for(size_t i=0;i<n;++i){uint32_t a=0,b=0;in>>a>>b;if(a<0x4ccd28 || a>=0x600000 || b>255)throw std::runtime_error("Invalid saved memory");next.memory[a]=uint8_t(b);}
    next.objectives.clear();n=size(65536);for(size_t i=0;i<n;++i){uint32_t a=0,b=0;in>>a>>b;next.objectives[a]=b;}
    unsigned gold=0;in>>gold;n=size(65536);if(n!=next.army.regiments.size() || gold>65535)throw std::runtime_error("Campaign roster mismatch");next.army.gold=uint16_t(gold);
    for(auto& r:next.army.regiments){unsigned id=0,alive=0;in>>id>>r.status>>alive;if(id!=r.id || alive>r.maximum)throw std::runtime_error("Invalid saved regiment");r.alive=uint8_t(alive);}
    next.presentation={};
    if(version>=2){unsigned screen=0;in>>screen>>next.presentation.head>>next.presentation.expression>>next.presentation.route;if(screen>unsigned(CampaignScreen::Debrief))throw std::runtime_error("Invalid campaign screen");next.presentation.screen=CampaignScreen(screen);
        in>>std::quoted(next.presentation.background)>>std::quoted(next.presentation.speech)>>std::quoted(next.presentation.map_icons)>>std::quoted(next.presentation.dot_file);
        n=size(64);for(size_t i=0;i<n;++i){std::array<int32_t,3> m{};in>>m[0]>>m[1]>>m[2];next.presentation.markers.push_back(m);}
    }else if(next.message.rfind("Dialogue: ",0)==0){next.presentation.screen=CampaignScreen::TalkingHead;next.presentation.speech=next.message.substr(10);}
    else if(next.message.rfind("Meeting point: ",0)==0){next.presentation.screen=CampaignScreen::Meeting;next.presentation.background=next.message.substr(15);}
    else if(next.message.rfind("Army book",0)==0)next.presentation.screen=CampaignScreen::Book;
    in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing campaign save data");if(in.bad() || in.fail())throw std::runtime_error("Truncated campaign save");*this=std::move(next);
}
}
