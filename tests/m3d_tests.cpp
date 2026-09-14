#include "m3d.hpp"
#include "math3d.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <functional>
void check(bool v,const char* s) {if(!v)throw std::runtime_error(s);}
void rejects(const std::function<void()>& fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught,"Expected invalid mesh rejection");}
void f32(prj::Bytes& b,size_t p,float f) {uint32_t n;std::memcpy(&n,&f,4);prj::put32(b,p,n);}
int main(int argc,char** argv) {try {
    prj::Bytes b(24+96+64+28+3*44);std::memcpy(b.data(),"PD3M",4);prj::put32(b,4,0x36243600);prj::put32(b,8,1);prj::put32(b,16,0xffffffff);
    b[20]=b[22]=1;std::memcpy(b.data()+88,"test.bmp",8);size_t group=120,face=184,vert=212;
    b[group+48]=3;b[group+50]=1;b[group+52]=2;f32(b,group+36,10);f32(b,group+40,20);f32(b,group+44,30);
    b[face+2]=1;b[face+4]=2;
    for(int i=0;i<3;++i) {f32(b,vert+i*44,float(i));f32(b,vert+i*44+16,1);f32(b,vert+i*44+28,0.25f);f32(b,vert+i*44+32,0.75f);}
    auto mesh=m3d::Model::decode(b);check(mesh.triangles==1 && mesh.textures[0]=="test.bmp","Mesh structure");
    auto v=mesh.batches[0].vertices[2];check(v.position.x==12 && v.position.y==20 && v.position.z==30,"Conditional pivot");check(v.u==0.25f && v.v==0.75f && v.normal.y==1,"Vertex field offsets");
    auto invalid=b;invalid[face]=3;rejects([&]{m3d::Model::decode(invalid);});invalid=b;invalid[face+6]=1;rejects([&]{m3d::Model::decode(invalid);});
    for(size_t i=0;i<b.size();++i)rejects([&]{m3d::Model::decode(prj::Bytes(b.begin(),b.begin()+i));});
    b[group+52]=0;check(m3d::Model::decode(b).batches[0].vertices[2].position.x==2,"Non-pivot group");
    check(m3d::render_flags("_kTree")==20 && m3d::render_flags("_7water")==7,"Filename flags");
    auto doc=prj::Document::blank(8,8);doc.add_instance(0,1,2,3);doc.set_field(0,0x20,1024);
    auto p=math3d::transform(math3d::instance(doc,0),{1,0,0});check(std::abs(p.x-1)<0.0001 && p.y==2 && std::abs(p.z-4)<0.0001,"Signed yaw and translation");
    auto view=math3d::look_at({0,0,-10},{0,0,0});
    auto screen_right=math3d::transform(view,{1,0,0});
    check(screen_right.x==1 && screen_right.y==0 && screen_right.z==-10,"Direct3D +X must appear right while GL depth stays negative");
    auto basis=math3d::camera_basis({0,0,-10},{0,0,0});
    check(basis.right.x==1 && basis.up.y==1 && basis.forward.z==1,"Picking/panning must share Direct3D camera handedness");
    float hit=100;check(math3d::ray_triangle({0.25f,1,0.25f},{0,-1,0},{0,0,0},{1,0,0},{0,0,1},hit) && hit==1,"Picking ray");
    size_t files=0,triangles=0;
    for(int arg=1;arg<argc;++arg)for(auto& entry:std::filesystem::recursive_directory_iterator(argv[arg])) {
        auto ext=entry.path().extension().string();if(ext!=".M3D" && ext!=".M3X" && ext!=".m3d" && ext!=".m3x")continue;
        try {auto m=m3d::Model::open(entry.path());triangles+=m.triangles;++files;}
        catch(const std::exception& e) {throw std::runtime_error(entry.path().string()+": "+e.what());}
    }
    std::cout<<"M3D tests passed; "<<files<<" installed meshes, "<<triangles<<" triangles checked.\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
