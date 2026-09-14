#include "m3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
namespace m3d {
namespace {
void need(bool v,const char* s) {if(!v) throw std::runtime_error(s);}
uint16_t u16(const prj::Bytes& b,size_t p) {need(p<=b.size() && b.size()-p>=2,"Truncated M3D short");return uint16_t(b[p])|uint16_t(b[p+1])<<8;}
float f32(const prj::Bytes& b,size_t p) {auto bits=prj::u32(b,p);float f;std::memcpy(&f,&bits,4);need(std::isfinite(f) && std::abs(f)<1e10f,"Invalid M3D float");return f;}
std::string str(const prj::Bytes& b,size_t p,size_t len) {
    need(p<=b.size() && len<=b.size()-p,"Truncated M3D string");auto end=std::find(b.begin()+p,b.begin()+p+len,0);return {b.begin()+p,end};
}
std::string lower(std::string s) {for(auto& c:s) if(c>='A' && c<='Z') c=char(c+'a'-'A');return s;}
}
unsigned render_flags(const std::string& s) {
    if(s.size()<2 || s[0]!='_') return 0;
    char c=lower(s)[1];return c>='0' && c<='9'?unsigned(c-'0'):c>='a' && c<='z'?unsigned(c-'a'+10):0;
}
Model Model::decode(const prj::Bytes& b) {
    need(b.size()>=24 && b.size()<=128*1024*1024,"Invalid M3D size");need(std::memcmp(b.data(),"PD3M",4)==0,"Expected PD3M mesh signature");
    need(prj::u32(b,4)==0x36243600 && prj::u32(b,8)==1,"Unsupported M3D version");
    auto nt=u16(b,20),ng=u16(b,22);size_t p=24;Model model;
    for(unsigned i=0;i<nt;++i) {model.textures.push_back(str(b,p+64,32));p+=96;}
    for(unsigned g=0;g<ng;++g) {
        auto name=str(b,p,32);auto nv=u16(b,p+48),nf=u16(b,p+50);auto flags=prj::u32(b,p+52);
        size_t face=p+64,vert=face+size_t(nf)*28,end=vert+size_t(nv)*44;
        need(end<=b.size(),"Truncated M3D geometry");
        Vec3 pivot{};if(flags&2) pivot={f32(b,p+36),f32(b,p+40),f32(b,p+44)};
        std::vector<Vertex> vertices;vertices.reserve(nv);
        for(unsigned i=0;i<nv;++i) {
            auto v=vert+i*44;Vertex value;
            value.position={f32(b,v)+pivot.x,f32(b,v+4)+pivot.y,f32(b,v+8)+pivot.z};
            value.normal={f32(b,v+12),f32(b,v+16),f32(b,v+20)};
            value.u=f32(b,v+28);value.v=f32(b,v+32);vertices.push_back(value);
            model.minimum.x=std::min(model.minimum.x,value.position.x);model.maximum.x=std::max(model.maximum.x,value.position.x);
            model.minimum.y=std::min(model.minimum.y,value.position.y);model.maximum.y=std::max(model.maximum.y,value.position.y);
            model.minimum.z=std::min(model.minimum.z,value.position.z);model.maximum.z=std::max(model.maximum.z,value.position.z);
        }
        std::map<int,size_t> batches;
        for(unsigned i=0;i<nf;++i) {
            auto f=face+i*28;int material=int16_t(u16(b,f+6));
            need(material>=-1 && material<int(nt),"M3D material index out of range");
            if(material==-1 && nt) material=0; // LoadM3DModel's normal fallback.
            if(!batches.count(material)) {batches[material]=model.batches.size();model.batches.push_back({material,render_flags(name),{}});}
            auto& out=model.batches[batches[material]].vertices;
            for(unsigned corner=0;corner<3;++corner) {auto index=u16(b,f+corner*2);need(index<nv,"M3D vertex index out of range");out.push_back(vertices[index]);}
            ++model.triangles;
        }
        p=end;
    }
    need(p==b.size(),"Unexpected bytes after M3D groups");need(model.triangles>0,"Mesh has no triangles");return model;
}
Model Model::open(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file) throw std::runtime_error("Missing mesh: "+path.string());
    auto size=file.tellg();need(size>=0 && size<=128*1024*1024,"Invalid M3D file size");file.seekg(0);
    prj::Bytes b(static_cast<size_t>(size));file.read(reinterpret_cast<char*>(b.data()),size);need(bool(file),"Cannot read M3D");return decode(b);
}
std::filesystem::path resolve(const std::filesystem::path& root,const std::string& name) {
    std::string normalized=name;std::replace(normalized.begin(),normalized.end(),'\\','/');
    std::filesystem::path rel(normalized);if(rel.is_absolute() || normalized.find(':')!=std::string::npos) rel=rel.filename();
    auto path=root;
    for(const auto& part:rel) {
        if(part==".") continue;
        auto exact=path/part;std::error_code ec;
        if(std::filesystem::exists(exact,ec)) {path=exact;continue;}
        bool found=false;
        for(std::filesystem::directory_iterator it(path,ec),end;!ec && it!=end;it.increment(ec)) {
            if(lower(it->path().filename().string())==lower(part.string())) {path=it->path();found=true;break;}
        }
        if(!found) return {};
    }
    return path;
}
std::filesystem::path texture_path(const std::filesystem::path& root,const std::string& name) {
    std::string n=name;std::replace(n.begin(),n.end(),'\\','/');n=std::filesystem::path(n).filename().string();
    for(auto folder:{"TEXTURE/","LTEXTURE/",""}) {auto path=resolve(root,std::string(folder)+n);if(!path.empty()) return path;}
    return {};
}
}
