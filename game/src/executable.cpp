#include "executable.hpp"
#include <stdexcept>
namespace neo {
Executable::Executable(const std::filesystem::path& path):data(read_file(path)) {
    auto need=[](bool ok){if(!ok)throw std::runtime_error("Unsupported Dark Omen executable (expected PE32 image base 0x400000)");};
    need(data.size()>=64 && data[0]=='M' && data[1]=='Z');size_t pe=prj::u32(data,60);
    need(pe<data.size() && data.size()-pe>=248 && prj::u32(data,pe)==0x4550);
    auto short_at=[&](size_t p){need(p+2<=data.size());return unsigned(data[p])|(unsigned(data[p+1])<<8);};
    need(short_at(pe+4)==0x14c && short_at(pe+24)==0x10b && prj::u32(data,pe+52)==0x400000);
    size_t table=pe+24+short_at(pe+20);unsigned count=short_at(pe+6);need(count>0 && count<=96 && table<=data.size() && count*40<=data.size()-table);
    for(unsigned i=0;i<count;++i){size_t p=table+i*40;uint32_t rva=prj::u32(data,p+12),size=prj::u32(data,p+16),raw=prj::u32(data,p+20);need(rva<0xffbfffff && raw<=data.size() && size<=data.size()-raw);sections.push_back({0x400000+rva,size,raw});}
    need(word(0x4c3d68)==0x41df20);
}
size_t Executable::offset(uint32_t a,size_t n) const {
    for(const auto& s:sections)if(a>=s.address && uint64_t(a)-s.address<=s.size && n<=s.size-(uint64_t(a)-s.address))return size_t(s.offset)+(a-s.address);
    throw std::runtime_error("Executable address outside file-backed section: "+std::to_string(a));
}
uint32_t Executable::word(uint32_t a) const {return prj::u32(data,offset(a,4));}
prj::Bytes Executable::bytes(uint32_t a,size_t n) const {size_t p=offset(a,n);return {data.begin()+p,data.begin()+p+n};}
std::string Executable::string(uint32_t a,size_t limit) const {
    if(a==0 || a==0xffffffff)return {};
    std::string out;
    for(size_t i=0;i<limit;++i){if(uint64_t(a)+i>0xffffffff)break;char c=char(data[offset(a+uint32_t(i),1)]);if(!c)return out;out+=c;}
    throw std::runtime_error("Unterminated executable string");
}
}
