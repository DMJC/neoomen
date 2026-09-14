#include "cursor.hpp"
#include "formats.hpp"
#include "m3d.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <memory>
namespace neo {
namespace {
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
struct Timing {uint32_t rate=10;std::vector<uint32_t> rates,sequence;std::vector<SDL_Point> hotspots;};
SDL_Point hotspot(const prj::Bytes& b,size_t p,size_t size){
    if(size<22 || p>b.size() || size>b.size()-p || b[p]!=0 || b[p+2]!=2 || b[p+4]==0)throw std::runtime_error("Invalid CUR directory");
    int x=b[p+10]|(b[p+11]<<8),y=b[p+12]|(b[p+13]<<8);if(x>=32 || y>=32)throw std::runtime_error("Invalid cursor hotspot");return {x,y};
}
// ANI timing is measured in 1/60-second jiffies. Colour pixels come from BMP sheets.
Timing timing(const std::filesystem::path& path){
    Timing out;if(path.empty())return out;auto b=read_file(path);
    if(b.size()<12 || std::memcmp(b.data(),"RIFF",4) || std::memcmp(b.data()+8,"ACON",4))throw std::runtime_error("Invalid ANI header");
    size_t end=size_t(prj::u32(b,4))+8;
    // Original ANI writers included the RIFF header in the recorded size.
    if(end==b.size()+8)end=b.size();
    if(end>b.size())throw std::runtime_error("Truncated ANI");
    for(size_t p=12;p+8<=end;){size_t n=prj::u32(b,p+4),data=p+8;if(n>end-data)throw std::runtime_error("Truncated ANI chunk");
        if(!std::memcmp(b.data()+p,"anih",4) && n>=36)out.rate=prj::u32(b,data+28);
        if(!std::memcmp(b.data()+p,"rate",4) || !std::memcmp(b.data()+p,"seq ",4)){auto& values=b[p]=='r'?out.rates:out.sequence;for(size_t i=0;i+4<=n;i+=4)values.push_back(prj::u32(b,data+i));}
        if(!std::memcmp(b.data()+p,"LIST",4) && n>=4 && !std::memcmp(b.data()+data,"fram",4)){
            for(size_t q=data+4;q+8<=data+n;){size_t length=prj::u32(b,q+4);if(length>data+n-q-8)throw std::runtime_error("Truncated ANI frame");if(!std::memcmp(b.data()+q,"icon",4))out.hotspots.push_back(hotspot(b,q+8,length));q+=8+length+(length&1);}
        }
        p=data+n+(n&1);
    }return out;
}
}
CursorTheme::CursorTheme(const std::filesystem::path& data):root(data){system=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);}
CursorTheme::~CursorTheme(){SDL_SetCursor(SDL_GetDefaultCursor());for(auto& entry:cursors)for(auto& frame:entry.second)SDL_FreeCursor(frame.cursor);if(system)SDL_FreeCursor(system);}
const std::vector<CursorTheme::Frame>& CursorTheme::load(const std::string& name){
    auto found=cursors.find(name);if(found!=cursors.end())return found->second;
    auto& frames=cursors[name];if(root.empty())return frames;
    try{
        auto file=m3d::resolve(root,"Graphics/Cursors/"+name+".BMP");if(file.empty())return frames;
        Surface image(SDL_LoadBMP(file.c_str()),SDL_FreeSurface);if(!image)throw std::runtime_error(SDL_GetError());
        if((image->w!=32 || image->h!=32) && (image->w!=64 || image->h!=64))throw std::runtime_error("Unsupported cursor sheet dimensions");
        auto animation=timing(m3d::resolve(root,"Graphics/Cursors/"+name+".ANI"));unsigned count=unsigned(image->w/32)*unsigned(image->h/32);
        SDL_Point static_hotspot{};auto cur=m3d::resolve(root,"Graphics/Cursors/"+name+".CUR");if(!cur.empty()){auto bytes=read_file(cur);static_hotspot=hotspot(bytes,0,bytes.size());}
        if(animation.sequence.empty())for(unsigned i=0;i<count;++i)animation.sequence.push_back(i);
        SDL_SetColorKey(image.get(),SDL_TRUE,SDL_MapRGB(image->format,0,0,0));
        for(size_t i=0;i<animation.sequence.size();++i){unsigned index=animation.sequence[i];if(index>=count)throw std::runtime_error("ANI frame outside colour sheet");
            Surface frame(SDL_CreateRGBSurfaceWithFormat(0,32,32,32,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);if(!frame)throw std::runtime_error(SDL_GetError());SDL_FillRect(frame.get(),nullptr,0);
            SDL_Rect source{int(index%unsigned(image->w/32))*32,int(index/unsigned(image->w/32))*32,32,32};if(SDL_BlitSurface(image.get(),&source,frame.get(),nullptr)<0)throw std::runtime_error(SDL_GetError());
            // Flip each frame independently, preserving the sheet's animation order.
            if(SDL_LockSurface(frame.get())<0)throw std::runtime_error(SDL_GetError());
            auto* pixels=static_cast<Uint8*>(frame->pixels);
            for(int y=0;y<frame->h/2;++y){
                auto* top=pixels+y*frame->pitch;
                auto* bottom=pixels+(frame->h-1-y)*frame->pitch;
                std::swap_ranges(top,top+frame->w*4,bottom);
            }
            SDL_UnlockSurface(frame.get());
            auto origin=index<animation.hotspots.size()?animation.hotspots[index]:static_hotspot;
            auto cursor=SDL_CreateColorCursor(frame.get(),origin.x,frame->h-1-origin.y);if(!cursor)throw std::runtime_error(SDL_GetError());
            uint32_t rate=i<animation.rates.size()?animation.rates[i]:animation.rate;frames.push_back({cursor,uint32_t(std::clamp<uint64_t>((uint64_t(rate)*1000+30)/60,1,60000))});
        }
    }catch(const std::exception& e){for(auto& f:frames)SDL_FreeCursor(f.cursor);frames.clear();std::cerr<<"Cursor "<<name<<": "<<e.what()<<'\n';}
    return frames;
}
void CursorTheme::update(const std::string& name,bool original,uint64_t now){
    if(active!=name){active=name;started=now;}SDL_Cursor* next=system;
    if(original){const auto& frames=load(name);if(!frames.empty()){uint64_t duration=0;for(auto& f:frames)duration+=f.duration;uint64_t elapsed=(now-started)%duration;for(auto& f:frames){if(elapsed<f.duration){next=f.cursor;break;}elapsed-=f.duration;}}}
    if(next && next!=current){SDL_SetCursor(next);current=next;}
}
}
