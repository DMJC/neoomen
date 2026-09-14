#include "formats.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace neo {
namespace {
void need(bool ok,const char* error) {if(!ok)throw std::runtime_error(error);}
void range(const prj::Bytes& b,size_t p,size_t n) {need(p<=b.size() && n<=b.size()-p,"Truncated asset");}
uint16_t u16(const prj::Bytes& b,size_t p) {range(b,p,2);return uint16_t(b[p]) | uint16_t(b[p+1])<<8;}
std::string str(const prj::Bytes& b,size_t p,size_t n) {range(b,p,n);auto end=std::find(b.begin()+p,b.begin()+p+n,0);return {b.begin()+p,end};}
constexpr int steps[]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
constexpr int adjust[]={-1,-1,-1,-1,2,4,6,8};
struct Decoder {
    int predictor,index;
    int16_t sample(unsigned n) {
        int step=steps[index],diff=step>>3;
        if(n&4)diff+=step;
        if(n&2)diff+=step>>1;
        if(n&1)diff+=step>>2;
        predictor=std::clamp(predictor+((n&8)?-diff:diff),-32768,32767);
        index=std::clamp(index+adjust[n&7],0,88);return int16_t(predictor);
    }
};
}
prj::Bytes read_file(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Cannot open "+path.string());
    auto n=f.tellg();need(n>=0 && n<=128*1024*1024,"Asset exceeds 128 MiB");
    prj::Bytes b(static_cast<size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);
    need(bool(f),"Cannot read asset");return b;
}
Army Army::decode(const prj::Bytes& b) {
    range(b,0,192);need(prj::u32(b,0)==670,"Invalid ARM signature");
    auto count=prj::u32(b,4);need(prj::u32(b,8)==188,"Unsupported ARM record size");
    need(count<65536 && b.size()==192+size_t(count)*188,"ARM size/count mismatch");
    Army a;a.race=b[12];a.gold=u16(b,0x94);a.name=str(b,0x12,32);
    for(size_t p=192;p<b.size();p+=188) {
        a.regiments.push_back({prj::u32(b,p+4),prj::u32(b,p+0x10),u16(b,p+0x14),b[p+0x39],b[p+0x3a],str(b,p+0x16,32),str(b,p+0x56,32)});
        a.regiments.back().status=u16(b,p);
        auto& r=a.regiments.back();r.banner=u16(b,p+12);r.cost=u16(b,p+10);r.wizard=b[p+8];r.armour=b[p+74];r.head=u16(b,p+146);r.experience=u16(b,p+156);r.magic_book=u16(b,p+160);
        std::copy_n(b.begin()+p+64,9,r.stats.begin());for(unsigned i=0;i<3;++i)r.items[i]=u16(b,p+162+i*2);
        a.regiments.back().unit_class=b[p+0x4c];a.regiments.back().missile_weapon=b[p+0x4e];a.regiments.back().race=b[p+0x38];
    }
    return a;
}
std::vector<BtbChunk> decode_btb(const prj::Bytes& b) {
    constexpr uint32_t sentinel=0xbeafeed0;
    need(b.size()>=16 && prj::u32(b,0)==sentinel && prj::u32(b,4)==0,"Invalid BTB start");
    std::vector<BtbChunk> out;size_t p=8;
    while(p<b.size()) {
        range(b,p,8);auto type=prj::u32(b,p),size=prj::u32(b,p+4);p+=8;range(b,p,size);
        if(type==sentinel) {need(size==0 && p==b.size(),"Invalid BTB end");return out;}
        out.push_back({type,{b.begin()+p,b.begin()+p+size}});p+=size;
    }
    throw std::runtime_error("Missing BTB end");
}
BattleSetup BattleSetup::decode(const prj::Bytes& bytes) {
    BattleSetup setup;
    auto items=[](const prj::Bytes& b) {
        std::vector<BtbChunk> result;
        for(size_t p=0;p<b.size();) {
            range(b,p,8);auto type=prj::u32(b,p),length=prj::u32(b,p+4);
            need(length>=8,"Invalid BTB item size");range(b,p,length);
            result.push_back({type,{b.begin()+p+8,b.begin()+p+length}});p+=length;
        }
        return result;
    };
    bool header=false;
    for(const auto& chunk:decode_btb(bytes)) {
        if(chunk.type==1) {
            need(!header,"Duplicate BTB header");header=true;
            for(const auto& item:items(chunk.payload))switch(item.type) {
                case 1:setup.width=prj::u32(item.payload,0);break;
                case 2:setup.height=prj::u32(item.payload,0);break;
                case 1001:setup.player_army=str(item.payload,0,item.payload.size());break;
                case 1002:setup.enemy_army=str(item.payload,0,item.payload.size());break;
                case 1003:setup.script=str(item.payload,0,item.payload.size());break;
                default:break;
            }
        }
        if(chunk.type==4){
            BattleRegion region;
            for(const auto& field:items(chunk.payload)){
                if(field.type==1006)region.name=str(field.payload,0,field.payload.size());
                else if(field.type==5)region.flags=prj::u32(field.payload,0);
                else if(field.type==502){range(field.payload,0,16);region.points.push_back({int32_t(prj::u32(field.payload,0))/8.f,int32_t(prj::u32(field.payload,4))/8.f});}
            }
            if(!region.points.empty())setup.regions.push_back(std::move(region));
        }
        // The function labeled Btb_ParseRegionChunk actually populates the
        // 24-byte game-object table from disk tag 5 / nested tag 503.
        if(chunk.type==5)for(const auto& item:items(chunk.payload))if(item.type==503) {
            BattleNode node;bool x=false,z=false;
            for(const auto& field:items(item.payload))switch(field.type) {
                case 1:node.x=int16_t(prj::u32(field.payload,0));x=true;break;
                case 2:node.z=int16_t(prj::u32(field.payload,0));z=true;break;
                case 5:node.flags=prj::u32(field.payload,0);break;
                case 6:node.radius=uint16_t(prj::u32(field.payload,0));break;
                case 7:node.heading=uint16_t(prj::u32(field.payload,0));break;
                case 11:node.group=prj::u32(field.payload,0);break;
                case 12:node.unit_id=prj::u32(field.payload,0);break;
                case 13:node.script_id=prj::u32(field.payload,0);break;
                default:break;
            }
            need(x && z,"BTB node missing coordinates");setup.nodes.push_back(node);
        }
    }
    need(header && setup.width && setup.height,"BTB missing map dimensions");return setup;
}
bool BattleRegion::contains(float x,float z) const {
    if(points.size()<3)return false;
    bool inside=false;
    for(size_t i=0,j=points.size()-1;i<points.size();j=i++){
        auto a=points[j],b=points[i];float dx=b[0]-a[0],dz=b[1]-a[1],cross=(x-a[0])*dz-(z-a[1])*dx;
        if(std::abs(cross)<.001f && x>=std::min(a[0],b[0])-.001f && x<=std::max(a[0],b[0])+.001f && z>=std::min(a[1],b[1])-.001f && z<=std::max(a[1],b[1])+.001f)return true;
        if((a[1]>z)!=(b[1]>z) && x<(b[0]-a[0])*(z-a[1])/(b[1]-a[1])+a[0])inside=!inside;
    }
    return inside;
}
ShadowMap ShadowMap::decode(const prj::Bytes& b){
    range(b,0,28);need(std::memcmp(b.data(),"SHAD",4)==0,"Invalid SHD signature");
    need(prj::u32(b,4)==b.size()-8,"SHD size mismatch");
    ShadowMap out;out.width=prj::u32(b,8);out.height=prj::u32(b,12);
    need(out.width && out.height && out.width<=8192 && out.height<=8192 && uint64_t(out.width)*out.height<=16*1024*1024,"Invalid SHD dimensions");
    size_t columns=(out.width+7)/8,rows=(out.height+7)/8,blocks=columns*rows;
    need(prj::u32(b,20)==blocks && prj::u32(b,24)==blocks*8,"Invalid SHD block table");
    size_t data=28+blocks*8;range(b,data,4);size_t bytes=prj::u32(b,data);data+=4;range(b,data,bytes);
    need(bytes==size_t(prj::u32(b,16))*64 && data+bytes==b.size(),"Invalid SHD detail dictionary");
    out.heights.resize(size_t(out.width)*out.height);
    for(unsigned y=0;y<out.height;++y)for(unsigned x=0;x<out.width;++x){
        size_t block=28+((y/8)*columns+x/8)*8,offset=prj::u32(b,block+4);
        need(offset<=bytes && bytes-offset>=64,"SHD detail offset outside dictionary");
        int64_t value=int32_t(prj::u32(b,block));value+=int64_t(b[data+offset+(y%8)*8+x%8])*128;
        need(value<=INT32_MAX,"SHD height overflow");out.heights[size_t(y)*out.width+x]=float(value)/1024.f;
    }return out;
}
SpriteFrame decode_sprite(const prj::Bytes& b,unsigned frame) {
    range(b,0,32);need(std::memcmp(b.data(),"WHDO",4)==0,"Invalid SPR signature");
    need(prj::u32(b,4)==b.size(),"SPR size mismatch");
    auto table=prj::u32(b,8),pixels=prj::u32(b,12),palette=prj::u32(b,16),colors=prj::u32(b,20),count=prj::u32(b,28);
    need(colors<=65536 && frame<count,"Unsupported SPR palette or frame");range(b,table,size_t(count)*32);range(b,palette,size_t(colors)*4);
    size_t p=table+size_t(frame)*32;auto type=u16(b,p);if(type==5)return {};
    need(type<=4,"Unsupported SPR frame type");unsigned key=u16(b,p+2),w=u16(b,p+8),h=u16(b,p+10);
    size_t n=size_t(w)*h;need(w<=8192 && h<=8192 && n==prj::u32(b,p+16),"Unsupported SPR pixel encoding");
    size_t start=size_t(pixels)+prj::u32(b,p+12);range(b,start,n);
    SpriteFrame out{w,h,prj::Bytes(n*4),int16_t(u16(b,p+4)),int16_t(u16(b,p+6))};
    unsigned bank=prj::u32(b,p+24);
    for(size_t i=0;i<n;++i) {
        unsigned index=b[start+i];if(index==key)continue;
        need(bank<colors && index<colors-bank,"SPR palette index out of range");size_t c=palette+(bank+index)*4;
        // Black is transparent regardless of its palette index; cyan marks shadow pixels.
        if(b[c+2]==0 && b[c+1]==0 && b[c]==0)continue;
        if(b[c+2]==0 && b[c+1]==255 && b[c]==255){out.rgba[i*4+3]=128;continue;}
        out.rgba[i*4]=b[c+2];out.rgba[i*4+1]=b[c+1];out.rgba[i*4+2]=b[c];out.rgba[i*4+3]=255;
    }
    return out;
}
std::vector<std::array<float,2>> decode_travel_route(const prj::Bytes& b,unsigned route){
    range(b,0,16);need(std::memcmp(b.data(),"TODW",4)==0,"Invalid travel path signature");unsigned count=prj::u32(b,12);need(count<=50 && route<count,"Invalid travel route index");
    struct Route {std::vector<std::array<float,2>> points;int next=-1;};std::vector<Route> routes;size_t p=16;
    for(unsigned i=0;i<count;++i){unsigned n=prj::u32(b,p);p+=4;need(n<=10,"Travel route point count");range(b,p,size_t(n)*16+44);Route r;
        for(unsigned j=0;j<n;++j){r.points.push_back({float(int32_t(prj::u32(b,p))),float(int32_t(prj::u32(b,p+4)))});p+=16;}
        r.next=int32_t(prj::u32(b,p+20));p+=44;routes.push_back(std::move(r));
    }
    std::vector<std::array<float,2>> result;std::vector<bool> seen(count);int at=int(route);
    while(at>=0){need(unsigned(at)<count && !seen[size_t(at)],"Invalid travel route chain");seen[size_t(at)]=true;const auto& r=routes[size_t(at)];result.insert(result.end(),r.points.begin(),r.points.end());at=r.next;}
    return result;
}
Pcm decode_adpcm(const prj::Bytes& b,unsigned channels) {
    need(channels==1 || channels==2,"ADPCM requires mono or stereo");Pcm out{channels,{}};
    for(size_t block=0;block<b.size();block+=1024) {
        size_t end=std::min(block+1024,b.size()),body=block+channels*4;range(b,block,channels*4);
        Decoder state[2]{};bool raw=b[block+2]>=89 || (channels==2 && b[block+6]>=89);
        for(unsigned c=0;c<channels;++c)state[c]={int16_t(u16(b,block+c*4)),b[block+c*4+2]};
        // Both original block decompressors emit the header predictor frame.
        for(unsigned c=0;c<channels;++c)out.samples.push_back(int16_t(state[c].predictor));
        if(raw) {
            // The original switches into terminal passthrough state: subsequent
            // read-ahead chunks are PCM continuation, not new ADPCM headers.
            end=b.size();
            need((end-body)%(channels*2)==0,"Incomplete raw PCM frame");
            for(size_t p=body;p<end;p+=2)out.samples.push_back(int16_t(u16(b,p)));
            break;
        } else if(channels==1) {
            need((end-body)%4==0,"Incomplete mono ADPCM word");
            for(size_t p=body;p<end;++p) {out.samples.push_back(state[0].sample(b[p]&15));out.samples.push_back(state[0].sample(b[p]>>4));}
        } else {
            need(state[1].index<89 && (end-body)%8==0,"Invalid stereo ADPCM block");
            for(size_t p=body;p<end;p+=8)for(unsigned i=0;i<8;++i) {
                out.samples.push_back(state[0].sample((b[p+i/2]>>((i%2)*4))&15));
                out.samples.push_back(state[1].sample((b[p+4+i/2]>>((i%2)*4))&15));
            }
        }
    }
    return out;
}
}
