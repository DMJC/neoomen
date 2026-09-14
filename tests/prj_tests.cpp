#include "prj.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <fstream>
#include <iterator>
#include <limits>
void check(bool b,const char* s) { if(!b) throw std::runtime_error(s); }
void rejects(const std::function<void()>& f) { bool caught=false; try { f(); } catch(const std::exception&) { caught=true; } check(caught,"Expected rejection"); }
int main(int argc,char** argv) { try {
    auto d=prj::Document::blank(16,24); auto original=d.encode();
    check(prj::Document::decode(original).encode()==original,"Blank round trip");
    d.set_elevation(0,1,1,128); check(d.elevation(0,1,1)==128,"Height edit");
    check(d.elevation(1,1,1)==0 && d.elevation(0,9,1)==0,"Shared dictionary isolation");
    auto edited=d.encode(); rejects([&]{d.set_elevation(0,1,1,1);}); check(d.encode()==edited,"Rejected height edit mutated data");
    auto negative=prj::Document::blank(8,8); negative.set_elevation(0,0,0,-128);
    check(prj::Document::decode(negative.encode()).elevation(0,0,0)==-128,"Signed height round trip");
    auto before_invalid=d.encode(); rejects([&]{d.add_instance(0,std::numeric_limits<double>::quiet_NaN(),0,0);});
    check(d.encode()==before_invalid,"Invalid instance insertion must be atomic");
    d.set_attribute(0,0,2); d.set_attribute(1,0,8); check(d.attributes[16]==0x82,"Nibble packing");
    check(d.attribute(0,0,true)==8,"Alternate nibble order");
    d.add_mesh("tree.m3d"); d.add_instance(1,-1.5,2,3); d.set_field(0,0x90,0xdeadbeef);
    d.duplicate_instance(0); check(d.field(1,0x90)==0xdeadbeef,"Unknown field preservation");
    d.remove_instance(0); check(int32_t(d.field(0,0x10))==-1536,"Signed position");
    d.set_music("battle1.fsm"); check(d.music()=="battle1.fsm","Music");
    check(prj::Document::decode(d.encode()).encode()==d.encode(),"Edited round trip");
    for(size_t n=0;n<original.size();++n) {
        // Opaque trailing bytes may be missing; structured chunks may not.
        if(n<original.size()-88) rejects([&]{prj::Document::decode(prj::Bytes(original.begin(),original.begin()+n));});
    }
    auto corrupt=original;
    corrupt[32]='X'; rejects([&]{prj::Document::decode(corrupt);});
    auto invalid=d; prj::put32(invalid.terrain,32,1); rejects([&]{prj::Document::decode(invalid.encode());});
    d.save("/tmp/neoomen-roundtrip-test.prj"); check(prj::Document::open("/tmp/neoomen-roundtrip-test.prj").encode()==d.encode(),"Atomic save");
    std::filesystem::remove("/tmp/neoomen-roundtrip-test.prj");
    size_t count=0;
    for(int arg=1;arg<argc;++arg) for(auto& entry:std::filesystem::recursive_directory_iterator(argv[arg])) {
        auto ext=entry.path().extension().string(); if(ext!=".PRJ" && ext!=".prj") continue;
        auto path=entry.path().string(); auto level=prj::Document::open(path);
        std::ifstream input(path,std::ios::binary);
        prj::Bytes source((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        auto raw=level.encode(); check(raw==source,"Unmodified file must match original bytes"); check(prj::Document::decode(raw).encode()==raw,"Real level round trip");
        auto original_level=level;
        auto h=level.elevation(0,0,0); level.set_elevation(0,0,0,h);
        check(level.encode()==raw,"No-op height edit must preserve bytes");
        level.set_elevation(0,0,0,h+128);
        check(level.elevation(1,0,0)==original_level.elevation(1,0,0),"Real layer isolation");
        for(uint32_t y=0;y<level.height();++y) for(uint32_t x=0;x<level.width();++x)
            check(level.elevation(0,x,y)==original_level.elevation(0,x,y)+((x==0 && y==0)?128:0),"Height edit leaked to another cell");
        level.set_attribute(0,0,level.attribute(0,0)^2);
        check(prj::Document::decode(level.encode()).attribute(0,0)==level.attribute(0,0),"Real edit reload");
        check(level.tail==original_level.tail,"Opaque trailer preservation"); ++count;
    }
    std::cout<<"All PRJ tests passed; "<<count<<" installed levels checked.\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
