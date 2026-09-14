#include "prj.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>

namespace prj {
namespace {
void need(bool b, const std::string& s) { if (!b) throw std::runtime_error(s); }
void append32(Bytes& b, uint32_t v) { for (int i=0;i<4;++i) b.push_back(uint8_t(v>>(8*i))); }
void append(Bytes& a, const Bytes& b) { a.insert(a.end(),b.begin(),b.end()); }
Bytes chunk(const char* tag, uint32_t size) { Bytes b(tag,tag+4); append32(b,size); return b; }
std::string string_at(const Bytes& b, size_t p, size_t n) {
    need(p<=b.size() && n<=b.size()-p,"Truncated string");
    auto end=std::find(b.begin()+p,b.begin()+p+n,0);
    return std::string(b.begin()+p,end);
}
void name_ok(const std::string& s) { need(s.size()<4096 && s.find('\0')==std::string::npos,"Invalid mesh filename"); }
void add_string(Bytes& b, const std::string& s) {
    name_ok(s); append32(b,uint32_t(s.size()+1)); b.insert(b.end(),s.begin(),s.end()); b.push_back(0);
}
struct Reader {
    const Bytes& b; size_t p=0;
    void tag(const char* s) { need(p+4<=b.size() && std::memcmp(b.data()+p,s,4)==0,std::string("Expected ")+s+" at byte "+std::to_string(p)); }
    void skip(size_t n) { need(p<=b.size() && n<=b.size()-p,"Truncated PRJ at byte "+std::to_string(p)); p+=n; }
    uint32_t read() { auto v=u32(b,p); p+=4; return v; }
    void str() { auto n=read(); need(n<=4096,"Mesh filename too long"); skip(n); }
    Bytes slice(size_t start) { return Bytes(b.begin()+start,b.begin()+p); }
};
size_t music_offset(const Bytes& b) {
    const std::array<uint8_t,4> tag={'M','U','S','C'};
    auto p=std::search(b.begin(),b.end(),tag.begin(),tag.end());
    return p==b.end() || size_t(b.end()-p)<24 ? b.size() : size_t(p-b.begin());
}
size_t patch_pos(const Document& d, unsigned l, uint32_t x, uint32_t y) {
    need(l<2 && x<d.width() && y<d.height(),"Terrain cell out of range");
    return 28+8*(size_t(l)*d.patch_count()+(y/8)*((d.width()+7)/8)+x/8);
}
}
uint32_t u32(const Bytes& b, size_t p) {
    need(p<=b.size() && b.size()-p>=4,"Truncated 32-bit field");
    return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
void put32(Bytes& b, size_t p, uint32_t v) {
    need(p<=b.size() && b.size()-p>=4,"32-bit write out of bounds");
    for(int i=0;i<4;++i) b[p+i]=uint8_t(v>>(8*i));
}
Document Document::decode(const Bytes& bytes) {
    need(bytes.size()<=256*1024*1024,"PRJ exceeds 256 MiB limit");
    Reader r{bytes}; Document d; r.skip(32); d.banner=r.slice(0);
    const std::array<std::pair<const char*,Bytes*>,2> names = {{{"BASE", &d.base}, {"WATR", &d.water}}};
    for(auto pair : names) {
        auto start=r.p; r.tag(pair.first); r.skip(4); r.str(); *pair.second=r.slice(start);
    }
    auto start=r.p; r.tag("FURN"); r.skip(8); auto count=r.read();
    need(count<=100000,"Too many furniture meshes");
    for(uint32_t i=0;i<count;++i) r.str();
    d.furniture=r.slice(start);
    start=r.p; r.tag("INST"); r.skip(8); count=r.read(); auto record=r.read();
    need(record>=152 && record<=4096 && count<=100000,"Unsupported instance count or record size");
    r.skip(size_t(count)*record); d.instances=r.slice(start);
    start=r.p; r.tag("TERR"); r.skip(4); auto marker=r.read();
    need(marker!=0,"This PRJ has no TERR grid; grid-less projects are not supported");
    auto w=r.read(), h=r.read(), n2=r.read(), n1=r.read();
    need(w>0 && h>0 && w<=4096 && h<=4096,"Invalid terrain dimensions (maximum 4096)");
    need(n1==size_t((w+7)/8)*((h+7)/8),"Terrain patch count does not match dimensions");
    need(n2>0 && n2<=1000000,"Invalid terrain dictionary size");
    need(r.read()==size_t(n1)*16,"Invalid Layer1 array byte length");
    r.skip(size_t(n1)*16);
    need(r.read()==size_t(n2)*64,"Invalid Layer2 array byte length");
    r.skip(size_t(n2)*64); d.terrain=r.slice(start);
    for(size_t i=0;i<size_t(n1)*2;++i) {
        auto off=u32(d.terrain,32+8*i);
        need(off%64==0 && off/64<n2,"Invalid terrain dictionary offset");
        auto base=int32_t(u32(d.terrain,28+8*i));
        for(size_t j=0;j<64;++j)
            need(int64_t(base)+int64_t(d.terrain[32+size_t(n1)*16+off+j])*128<=INT32_MAX,"Terrain height overflow");
    }
    start=r.p; r.tag("ATTR"); r.skip(4); auto size=r.read(); auto aw=r.read(), ah=r.read();
    need(aw>0 && ah>0 && aw<=4096 && ah<=4096 && aw%2==0,"Invalid ATTR dimensions (width must be even)");
    size_t packed=size_t(aw/2)*ah;
    need(size>=8+packed,"ATTR size is shorter than its grid");
    r.skip(size_t(size)-8); d.attributes=r.slice(start);
    d.tail=Bytes(bytes.begin()+r.p,bytes.end());
    return d;
}
Document Document::open(const std::string& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate); need(bool(f),"Cannot open "+path);
    auto n=f.tellg(); need(n>=0 && n<=256*1024*1024,"Invalid file size"); f.seekg(0);
    Bytes b(static_cast<size_t>(n)); f.read(reinterpret_cast<char*>(b.data()),n); need(bool(f),"Cannot read "+path);
    return decode(b);
}
Bytes Document::encode() const {
    Bytes b=banner;
    for(auto p:{&base,&water,&furniture,&instances,&terrain,&attributes,&tail}) append(b,*p);
    return b;
}
void Document::save(const std::string& path) const {
    auto bytes=encode(); (void)decode(bytes);
    // Same-directory temporary + rename prevents partial destination files.
    std::string temp=path+".neoomen-XXXXXX";
    std::vector<char> name(temp.begin(),temp.end()); name.push_back(0);
    int fd=mkstemp(name.data()); need(fd>=0,"Cannot create temporary save file next to "+path);
    bool ok=true; size_t pos=0;
    while(pos<bytes.size()) {
        auto n=::write(fd,bytes.data()+pos,bytes.size()-pos);
        if(n<=0) { ok=false; break; } pos+=size_t(n);
    }
    if(::fsync(fd)!=0) ok=false;
    if(::close(fd)!=0) ok=false;
    if(ok && ::rename(name.data(),path.c_str())==0) return;
    ::unlink(name.data()); throw std::runtime_error("Could not finish saving "+path);
}
Document Document::blank(uint32_t w,uint32_t h) {
    need(w>=8 && h>=8 && w<=4096 && h<=4096 && w%8==0 && h%8==0,"New grid dimensions must be multiples of 8 (8–4096)");
    Document d; d.banner.resize(32); const char* title="Dark Omen Battle file 1.10";
    std::copy(title,title+std::strlen(title),d.banner.begin());
    d.set_mesh("base.M3D"); d.set_mesh("",true);
    d.furniture=chunk("FURN",4); append32(d.furniture,0);
    d.instances=chunk("INST",0); append32(d.instances,0); append32(d.instances,152);
    auto n=(w/8)*(h/8); d.terrain=chunk("TERR",24+8*n+64);
    for(auto v:{w,h,1u,n}) append32(d.terrain,v);
    append32(d.terrain,n*16);
    d.terrain.resize(28+size_t(n)*16,0); append32(d.terrain,64);
    d.terrain.resize(32+size_t(n)*16+64,0);
    d.attributes=chunk("ATTR",8+w*h/2); append32(d.attributes,w); append32(d.attributes,h);
    d.attributes.resize(16+size_t(w)*h/2,0);
    // Unknown trailers are deliberately not invented. A template supplies them.
    d.tail.resize(64,0); auto music=chunk("MUSC",0); music.resize(24,0); append(d.tail,music);
    return decode(d.encode());
}
std::string Document::mesh(bool wm) const { const auto& b=wm?water:base; return string_at(b,8,u32(b,4)); }
void Document::set_mesh(const std::string& s,bool wm) {
    name_ok(s); Bytes b={'B','A','S','E'}; if(wm) b={'W','A','T','R'};
    add_string(b,s); (wm?water:base)=std::move(b);
}
std::vector<std::string> Document::catalog() const {
    std::vector<std::string> out; size_t p=12;
    for(uint32_t i=0;i<u32(furniture,8);++i) { auto n=u32(furniture,p); p+=4; out.push_back(string_at(furniture,p,n)); p+=n; }
    return out;
}
void Document::add_mesh(const std::string& s) {
    need(!s.empty(),"Mesh filename cannot be empty"); name_ok(s);
    need(u32(furniture,8)<100000,"Furniture limit reached");
    add_string(furniture,s); put32(furniture,8,u32(furniture,8)+1);
    put32(furniture,4,uint32_t(furniture.size()-8-4*u32(furniture,8)));
}
uint32_t Document::field(size_t i,size_t off) const {
    need(i<instance_count() && off%4==0 && off+4<=record_size(),"Instance field out of range"); return u32(instances,16+i*record_size()+off);
}
void Document::set_field(size_t i,size_t off,uint32_t v) {
    (void)field(i,off); put32(instances,16+i*record_size()+off,v);
}
void Document::add_instance(uint32_t slot,double x,double y,double z) {
    need(slot<=u32(furniture,8),"Furniture slot out of range"); need(instance_count()<100000,"Instance limit reached");
    for(auto v:{x,y,z}) need(std::isfinite(v) && v>=-2097152 && v<2097152,"Position out of range");
    auto n=instance_count(); instances.resize(instances.size()+record_size(),0);
    put32(instances,8,n+1); put32(instances,4,(n+1)*record_size());
    set_field(n,0x40,slot);
    size_t off=0x10;
    for(auto v:{x,y,z}) { need(v>=-2097152 && v<2097152,"Position out of range"); set_field(n,off,uint32_t(int32_t(v*1024))); off+=4; }
}
void Document::duplicate_instance(size_t i) {
    (void)field(i,0); need(instance_count()<100000,"Instance limit reached"); auto n=instance_count();
    Bytes copy(instances.begin()+16+i*record_size(),instances.begin()+16+(i+1)*record_size());
    append(instances,copy); put32(instances,8,n+1); put32(instances,4,(n+1)*record_size());
    for(size_t off:{0u,4u,8u,0x44u,0x80u}) set_field(n,off,0);
}
void Document::remove_instance(size_t i) {
    (void)field(i,0); auto n=instance_count(); auto size=record_size();
    instances.erase(instances.begin()+16+i*size,instances.begin()+16+(i+1)*size);
    put32(instances,8,n-1); put32(instances,4,(n-1)*size);
}
int32_t Document::elevation(unsigned l,uint32_t x,uint32_t y) const {
    auto p=patch_pos(*this,l,x,y); auto offset=u32(terrain,p+4);
    return int32_t(u32(terrain,p))+128*terrain.at(32+size_t(patch_count())*16+offset+(y%8)*8+x%8);
}
void Document::set_elevation(unsigned l,uint32_t x,uint32_t y,int32_t value) {
    auto p=patch_pos(*this,l,x,y);
    if(elevation(l,x,y)==value) return;
    auto old=u32(terrain,p+4); size_t dict=32+size_t(patch_count())*16;
    std::array<int64_t,64> heights{}; auto b=int32_t(u32(terrain,p));
    for(size_t i=0;i<64;++i) heights[i]=b+128*terrain.at(dict+old+i);
    heights[(y%8)*8+x%8]=value;
    auto lo=*std::min_element(heights.begin(),heights.end());
    for(auto h:heights) need(h-lo<=255*128 && (h-lo)%128==0,"Height must keep this 8×8 patch within 32640 raw units and the same remainder modulo 128");
    size_t refs=0;
    for(size_t i=0;i<size_t(patch_count())*2;++i) if(u32(terrain,32+i*8)==old) ++refs;
    uint32_t offset=old;
    if(refs>1) {
        auto n=u32(terrain,16); need(n<1000000,"Terrain dictionary limit reached");
        if(u32(terrain,4)==24+patch_count()*8+n*64) put32(terrain,4,u32(terrain,4)+64);
        offset=n*64; terrain.resize(terrain.size()+64); put32(terrain,16,n+1); put32(terrain,dict-4,(n+1)*64);
    }
    put32(terrain,p,lo); put32(terrain,p+4,offset);
    for(size_t i=0;i<64;++i) terrain[dict+offset+i]=uint8_t((heights[i]-lo)/128);
    // The 41 installed maps omit the second Layer1 copy from BSIZE.
    // Update that observed convention; preserve other nonzero marker conventions.
}
uint8_t Document::attribute(uint32_t x,uint32_t y,bool high) const {
    need(x<attr_width() && y<attr_height(),"Attribute cell out of range");
    unsigned shift=((x%2)^unsigned(high))*4;
    return (attributes.at(16+size_t(y)*(attr_width()/2)+x/2)>>shift)&15;
}
void Document::set_attribute(uint32_t x,uint32_t y,uint8_t v,bool high) {
    (void)attribute(x,y,high); need(v<16,"Attribute must be 0–15");
    unsigned shift=((x%2)^unsigned(high))*4; auto& b=attributes.at(16+size_t(y)*(attr_width()/2)+x/2);
    b=uint8_t((b & ~(15<<shift)) | (v<<shift));
}
bool Document::has_music() const { return music_offset(tail)!=tail.size(); }
std::string Document::music() const { auto p=music_offset(tail); return p==tail.size()?"":string_at(tail,p+4,20); }
void Document::set_music(const std::string& s) {
    need(s.size()<=19 && s.find('\0')==std::string::npos,"Music cue must fit 19 bytes plus terminator");
    auto p=music_offset(tail); need(p!=tail.size(),"No complete MUSC field found in this template");
    std::fill(tail.begin()+p+4,tail.begin()+p+24,0); std::copy(s.begin(),s.end(),tail.begin()+p+4);
}
}
