#include "renderer.hpp"
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace math3d;
namespace neo {
namespace {
constexpr const char* windmill_sails="../../Furnture/BATTLE/_GSAILS.M3D";
std::string lower(std::string value) {for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;}
GLuint shader(GLenum type,const char* source) {
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) {char log[4096];glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(log);}return s;
}
GLuint make_program() {
    const char* vertex=R"(#version 430 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
uniform mat4 mvp;uniform mat4 model;uniform bool animateWater;uniform float waterTime;
out vec2 uv;out vec3 n;out vec3 world;
void main(){
vec3 p=position;vec3 surfaceNormal=normal;uv=texcoord;
if(animateWater){
    // _7WATER.M3X's groups are permanently combined.  Dark Omen records
    // bit 1 as an animated-UV texture flag; it does not deform water vertices.
    uv+=vec2(-.024,.036)*waterTime;
}
gl_Position=mvp*vec4(p,1);n=mat3(model)*surfaceNormal;world=(model*vec4(p,1)).xyz;
})";
    const char* fragment=R"(#version 430 core
in vec2 uv;in vec3 n;in vec3 world;uniform sampler2D shadowMap;uniform vec2 shadowSize;uniform bool useShadows;uniform int spritePass;uniform sampler2D image;uniform vec3 tint;uniform float opacity;out vec4 color;
void main(){vec4 tex=texture(image,uv);if(tex.a<0.45)discard;
if(spritePass==1 && tex.a>.75)discard;
if(spritePass==2 && tex.a<.75)discard;
float light=length(n)<0.1?1.0:0.45+0.55*max(0.0,dot(normalize(n),normalize(vec3(.3,1,.4))));
float visibility=1.0;
if(useShadows){
    // SHD is an occluder-height grid in world units, not an opacity image.
    for(int i=1;i<=48;++i){vec3 q=world+vec3(.6,2.0,.8)*float(i);
        if(any(lessThan(q.xz,vec2(0))) || any(greaterThanEqual(q.xz,shadowSize)))break;
        if(texture(shadowMap,(q.xz+vec2(.5))/shadowSize).r>q.y+.5){visibility=.6;break;}
    }
}
color=vec4(tex.rgb*tint*light*visibility,tex.a*opacity);})";
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=0,p=0;
    try {fs=shader(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
        GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[4096];glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(log);}
    }catch(...){glDeleteShader(vs);if(fs)glDeleteShader(fs);if(p)glDeleteProgram(p);throw;}
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
void triangle(std::vector<m3d::Vertex>& v,Vec3 a,Vec3 b,Vec3 c) {
    Vec3 n=normal(cross(b-a,c-a));v.push_back({a,n,0,0});v.push_back({b,n,1,0});v.push_back({c,n,0,1});
}
void rectangle(std::vector<m3d::Vertex>& v,float x,float y,float w,float h) {
    for(Vec3 p: {Vec3{x,y,0},Vec3{x+w,y,0},Vec3{x+w,y+h,0},Vec3{x,y,0},Vec3{x+w,y+h,0},Vec3{x,y+h,0}})v.push_back({p,{},0,0});
}
Mat4 rotate_about_x(Vec3 pivot,float angle) {
    Mat4 m=Mat4::identity();float c=std::cos(angle),s=std::sin(angle);
    m.v[5]=c;m.v[6]=s;m.v[9]=-s;m.v[10]=c;
    m.v[13]=pivot.y-c*pivot.y+s*pivot.z;
    m.v[14]=pivot.z-s*pivot.y-c*pivot.z;
    return m;
}
Mat4 translate(Vec3 offset) {auto m=Mat4::identity();m.v[12]=offset.x;m.v[13]=offset.y;m.v[14]=offset.z;return m;}
const std::map<char,std::string> glyphs={
{'A',"01110100011000111111100011000110001"},{'B',"11110100011000111110100011000111110"},
{'C',"01111100001000010000100001000001111"},{'D',"11110100011000110001100011000111110"},
{'E',"11111100001000011110100001000011111"},{'F',"11111100001000011110100001000010000"},
{'G',"01111100001000010111100011000101111"},{'H',"10001100011000111111100011000110001"},
{'I',"11111001000010000100001000010011111"},{'J',"00111000100001000010000101001001100"},
{'K',"10001100101010011000101001001010001"},{'L',"10000100001000010000100001000011111"},
{'M',"10001110111010110101100011000110001"},{'N',"10001110011010110011100011000110001"},
{'O',"01110100011000110001100011000101110"},{'P',"11110100011000111110100001000010000"},
{'Q',"01110100011000110001101011001001101"},{'R',"11110100011000111110101001001010001"},
{'S',"01111100001000001110000010000111110"},{'T',"11111001000010000100001000010000100"},
{'U',"10001100011000110001100011000101110"},{'V',"10001100011000110001100010101000100"},
{'W',"10001100011000110101101011101110001"},{'X',"10001100010101000100010101000110001"},
{'Y',"10001100010101000100001000010000100"},{'Z',"11111000010001000100010001000011111"},
{'0',"01110100011001110101110011000101110"},{'1',"00100011000010000100001000010001110"},
{'2',"01110100010000100010001000100011111"},{'3',"11110000010000101110000010000111110"},
{'4',"00010001100101010010111110001000010"},{'5',"11111100001000011110000010000111110"},
{'6',"01110100001000011110100011000101110"},{'7',"11111000010001000100010000100001000"},
{'8',"01110100011000101110100011000101110"},{'9',"01110100011000101111000010000101110"},
{'-',"00000000000000011111000000000000000"},{':',"00000001000010000000001000010000000"},
{'/',"00001000010001000100010001000010000"},{'.',"00000000000000000000000000011000110"},
{'#',"01010010101111101010111110101001010"}
};
}
Renderer::Renderer(SDL_Window* w):window(w) {
    if(epoxy_gl_version()<43)throw std::runtime_error("OpenGL 4.3 core is required");
    program=make_program();glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);uint32_t pixel=0xffffffff;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&pixel);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glGenVertexArrays(1,&dynamic_vao);glGenBuffers(1,&dynamic_vbo);
    std::cout<<"OpenGL "<<glGetString(GL_VERSION)<<" / "<<glGetString(GL_RENDERER)<<'\n';
}
Renderer::~Renderer() {
    for(auto& p:ui_images)glDeleteTextures(1,&p.second.texture);
    for(auto& p:fonts)glDeleteTextures(1,&p.second.texture);
    for(auto& p:heads)for(auto& b:p.second.gpu){glDeleteBuffers(1,&b.vbo);glDeleteVertexArrays(1,&b.vao);}
    if(shadow_texture)glDeleteTextures(1,&shadow_texture);
    if(screen_texture)glDeleteTextures(1,&screen_texture);
    for(auto& pair:assets)for(auto& b:pair.second.gpu){glDeleteBuffers(1,&b.vbo);glDeleteVertexArrays(1,&b.vao);}
    for(auto t:textures)glDeleteTextures(1,&t);
    glDeleteTextures(1,&white);glDeleteBuffers(1,&dynamic_vbo);glDeleteVertexArrays(1,&dynamic_vao);glDeleteProgram(program);
}
void Renderer::upload(Batch& b,const std::vector<m3d::Vertex>& vertices) {
    if(!b.vao)glGenVertexArrays(1,&b.vao);
    if(!b.vbo)glGenBuffers(1,&b.vbo);
    glBindVertexArray(b.vao);glBindBuffer(GL_ARRAY_BUFFER,b.vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(m3d::Vertex)),vertices.data(),b.vao==dynamic_vao?GL_STREAM_DRAW:GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,position)));
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,normal)));
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,u)));
    b.count=GLsizei(vertices.size());
}
GLuint Renderer::texture(const std::filesystem::path& p,bool key) {
    using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
    Surface source(SDL_LoadBMP(p.c_str()),SDL_FreeSurface);if(!source)throw std::runtime_error(SDL_GetError());
    if(source->w>8192 || source->h>8192)throw std::runtime_error("Texture exceeds 8192 pixels");
    if(key)SDL_SetColorKey(source.get(),SDL_TRUE,source->format->palette?0:SDL_MapRGB(source->format,0,0,0));
    Surface rgba(SDL_ConvertSurfaceFormat(source.get(),SDL_PIXELFORMAT_RGBA32,0),SDL_FreeSurface);if(!rgba)throw std::runtime_error(SDL_GetError());
    GLuint id=0;glGenTextures(1,&id);textures.push_back(id);glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,rgba->pitch/4);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,rgba->w,rgba->h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba->pixels);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glGenerateMipmap(GL_TEXTURE_2D);return id;
}
void Renderer::load(const std::filesystem::path& file) {
    for(auto& pair:assets)for(auto& b:pair.second.gpu){glDeleteBuffers(1,&b.vbo);glDeleteVertexArrays(1,&b.vao);}
    assets.clear();sprites.clear();for(auto t:textures)glDeleteTextures(1,&t);textures.clear();triangles=0;
    if(shadow_texture)glDeleteTextures(1,&shadow_texture);
    shadow_texture=0;shadow_width=shadow_height=0;water_time=0;water_clock=0;
    document=prj::Document::open(file.string());auto root=file.parent_path();
    auto shadow_path=m3d::resolve(root,file.stem().string()+".SHD");
    if(!shadow_path.empty()){
        auto shadow=ShadowMap::decode(read_file(shadow_path));shadow_width=shadow.width;shadow_height=shadow.height;
        glGenTextures(1,&shadow_texture);glBindTexture(GL_TEXTURE_2D,shadow_texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,shadow.width,shadow.height,0,GL_RED,GL_FLOAT,shadow.heights.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        std::cout<<"Loaded SHD "<<shadow.width<<"x"<<shadow.height<<" occluder grid\n";
    }
    auto mesh_name=[](std::string name){std::filesystem::path p(name);p.replace_extension(".M3X");return p.string();};
    terrain=mesh_name(document.mesh());water=document.mesh(true).empty()?"":mesh_name(document.mesh(true));
    auto names=document.catalog();names.push_back(terrain);if(!water.empty())names.push_back(water);names.push_back(windmill_sails);
    std::map<std::string,GLuint> cache;
    for(const auto& name:names) {
        if(assets.count(name))continue;
        auto path=m3d::resolve(root,name);
        if(path.empty()){if(name==terrain)throw std::runtime_error("Missing terrain "+name);std::cerr<<"Missing mesh: "<<name<<'\n';continue;}
        auto& a=assets[name];a.cpu=m3d::Model::open(path);
        if(name!=terrain && name!=water)for(auto& batch:a.cpu.batches) {
            for(size_t i=0;i+2<batch.vertices.size();i+=3)std::swap(batch.vertices[i+1],batch.vertices[i+2]);
            for(auto& vertex:batch.vertices)vertex.normal=vertex.normal*-1.f;
        }
        for(const auto& batch:a.cpu.batches) {
            Batch gpu;gpu.texture=white;unsigned flags=batch.flags|m3d::render_flags(path.filename().string());
            gpu.alpha=(flags&(1|4))!=0;gpu.opacity=(flags&1)?.65f:1.f;gpu.water=(flags&2)!=0;
            if(batch.material>=0) {
                auto material=lower(a.cpu.textures.at(size_t(batch.material)));
                // Windmill models keep the body and the sails in one M3D.  The
                // sail material identifies the detachable rotating sub-part.
                gpu.rotor=material.find("sails")!=std::string::npos || name==windmill_sails;
                auto image=m3d::texture_path(path.parent_path(),a.cpu.textures.at(size_t(batch.material)));
                if(!image.empty()) {std::string k=image.string()+((flags&16)?"|key":"|opaque");if(!cache.count(k))cache[k]=texture(image,(flags&16)!=0);gpu.texture=cache[k];}
                else std::cerr<<"Missing texture: "<<a.cpu.textures.at(size_t(batch.material))<<'\n';
            }
            for(const auto& v:batch.vertices)gpu.center=gpu.center+v.position;
            gpu.center=gpu.center*(1.f/std::max<size_t>(1,batch.vertices.size()));
            upload(gpu,batch.vertices);a.gpu.push_back(gpu);
        }
        triangles+=a.cpu.triangles;
    }
    auto bounds=assets.at(terrain).cpu;
    if(auto it=assets.find(water);it!=assets.end()){
        bounds.minimum={std::min(bounds.minimum.x,it->second.cpu.minimum.x),std::min(bounds.minimum.y,it->second.cpu.minimum.y),std::min(bounds.minimum.z,it->second.cpu.minimum.z)};
        bounds.maximum={std::max(bounds.maximum.x,it->second.cpu.maximum.x),std::max(bounds.maximum.y,it->second.cpu.maximum.y),std::max(bounds.maximum.z,it->second.cpu.maximum.z)};
    }
    center=(bounds.minimum+bounds.maximum)*.5f;radius=std::max(1.f,length(bounds.maximum-bounds.minimum)*.5f);fit();
    std::cout<<"Loaded "<<file.filename()<<": "<<triangles<<" unique mesh triangles, "<<document.instance_count()<<" furniture instances\n";
}
void Renderer::configure(Battle& battle) const {
    const auto width=document.attr_width(),height=document.attr_height();
    auto mesh=assets.find(terrain);
    if(!width || !height || mesh==assets.end() || document.width()!=width || document.height()!=height)return;
    std::vector<uint8_t> attributes;std::vector<float> heights;
    attributes.reserve(size_t(width)*height);heights.reserve(size_t(width)*height);
    for(unsigned z=0;z<height;++z)for(unsigned x=0;x<width;++x) {
        attributes.push_back(document.attribute(x,z));
        heights.push_back(float(document.elevation(0,x,z))/1024.f);
    }
    battle.set_terrain(width,height,mesh->second.cpu.minimum.x,mesh->second.cpu.maximum.x,
                       mesh->second.cpu.minimum.z,mesh->second.cpu.maximum.z,
                       std::move(attributes),std::move(heights));
}
Vec3 Renderer::eye() const {return target+Vec3{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)}*distance;}
void Renderer::fit() {target=center;distance=radius*2.8f;}
void Renderer::orbit(float dx,float dy) {yaw-=dx*.007f;pitch=std::clamp(pitch+dy*.007f,.15f,1.5f);}
void Renderer::zoom(float amount) {distance=std::clamp(distance*std::pow(1.12f,-amount),2.f,radius*15);}
void Renderer::pan(float right,float forward,float dt) {
    auto b=camera_basis(eye(),target);Vec3 f=normal(Vec3{b.forward.x,0,b.forward.z});target=target+(b.right*right+f*forward)*(distance*.5f*dt);
}
void Renderer::center_on(Vec3 point) {target.x=point.x;target.z=point.z;}
Mat4 Renderer::view_projection() const {
    int w,h;SDL_GetWindowSize(window,&w,&h);return perspective(.84f,float(std::max(1,w))/std::max(1,h),std::max(.02f,distance/10000),distance+radius*20)*look_at(eye(),target);
}
Vec3 Renderer::ray(float x,float y) const {
    int w,h;SDL_GetWindowSize(window,&w,&h);h=std::max(1,h);w=std::max(1,w);auto b=camera_basis(eye(),target);
    return normal(b.forward+b.right*((2*x/w-1)*float(w)/h*std::tan(.42f))+b.up*((1-2*y/h)*std::tan(.42f)));
}
bool Renderer::ground(float x,float y,Vec3& point) const {
    Vec3 origin=eye(),dir=ray(x,y);float best=1e30f;
    auto it=assets.find(terrain);
    if(it!=assets.end()) {
        for(const auto& b:it->second.cpu.batches)for(size_t i=0;i+2<b.vertices.size();i+=3)ray_triangle(origin,dir,b.vertices[i].position,b.vertices[i+1].position,b.vertices[i+2].position,best);
    } else if(std::abs(dir.y)>1e-6f) {float t=-origin.y/dir.y;if(t>0)best=t;}
    if(best==1e30f)return false;
    point=origin+dir*best;return true;
}
float Renderer::elevation(float x,float z) const {
    auto it=assets.find(terrain);if(it==assets.end())return 0;
    float height=-1e30f;
    for(const auto& batch:it->second.cpu.batches)for(size_t i=0;i+2<batch.vertices.size();i+=3) {
        Vec3 a=batch.vertices[i].position,b=batch.vertices[i+1].position,c=batch.vertices[i+2].position;
        if(x<std::min({a.x,b.x,c.x}) || x>std::max({a.x,b.x,c.x}) || z<std::min({a.z,b.z,c.z}) || z>std::max({a.z,b.z,c.z}))continue;
        float det=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);if(std::abs(det)<1e-8f)continue;
        float u=((b.z-c.z)*(x-c.x)+(c.x-b.x)*(z-c.z))/det;
        float v=((c.z-a.z)*(x-c.x)+(a.x-c.x)*(z-c.z))/det;
        if(u>=0 && v>=0 && u+v<=1)height=std::max(height,u*a.y+v*b.y+(1-u-v)*c.y);
    }
    return height==-1e30f?center.y:height;
}
int Renderer::pick(const Battle& battle,float x,float y) const {
    Vec3 origin=eye(),dir=ray(x,y);int selected=-1;float best=1e30f;
    for(size_t i=0;i<battle.units.size();++i) {
        auto& u=battle.units[i];if(!u.regiment.alive)continue;
        float t=dot(u.position-origin,dir);if(t<0)continue;
        float d=length(origin+dir*t-u.position);if(d<5 && t<best){selected=int(i);best=t;}
    }
    return selected;
}
void Renderer::render_batch(const Batch& b,const Mat4& model,const Mat4& vp,Vec3 tint,float opacity) {
    auto mvp=vp*model;glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"mvp"),1,GL_FALSE,mvp.v);glUniformMatrix4fv(glGetUniformLocation(program,"model"),1,GL_FALSE,model.v);
    glUniform3f(glGetUniformLocation(program,"tint"),tint.x,tint.y,tint.z);glUniform1f(glGetUniformLocation(program,"opacity"),opacity);
    glUniform1i(glGetUniformLocation(program,"animateWater"),b.water);
    glUniform1f(glGetUniformLocation(program,"waterTime"),float(water_time));
    glUniform1i(glGetUniformLocation(program,"useShadows"),scene_shadows && shadow_texture!=0);
    glUniform2f(glGetUniformLocation(program,"shadowSize"),float(shadow_width),float(shadow_height));
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,shadow_texture);glUniform1i(glGetUniformLocation(program,"shadowMap"),1);
    glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(program,"image"),0);glBindTexture(GL_TEXTURE_2D,b.texture);glBindVertexArray(b.vao);glDrawArrays(GL_TRIANGLES,0,b.count);
}
void Renderer::immediate(const std::vector<m3d::Vertex>& vertices,const Mat4& vp,Vec3 tint) {
    if(vertices.empty())return;
    Batch b;b.vao=dynamic_vao;b.vbo=dynamic_vbo;b.texture=white;upload(b,vertices);render_batch(b,Mat4::identity(),vp,tint);
}
void Renderer::text(float x,float y,const std::string& value,Vec3 tint) {
    std::vector<m3d::Vertex> vertices;
    for(unsigned char c:value) {auto it=glyphs.find(char(std::toupper(c)));if(it!=glyphs.end())for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(it->second[size_t(row*5+col)]=='1')rectangle(vertices,x+col*2,y+row*2,2,2);x+=12;}
    int w,h;SDL_GetWindowSize(window,&w,&h);Mat4 ortho=Mat4::identity();ortho.v[0]=2.f/std::max(1,w);ortho.v[5]=-2.f/std::max(1,h);ortho.v[12]=-1;ortho.v[13]=1;immediate(vertices,ortho,tint);
}
void Renderer::draw(const Battle& battle,bool paused) {
    const auto now=SDL_GetTicks64();
    if(water_clock && !paused)water_time+=std::min(.1,double(now-water_clock)/1000.0);
    water_clock=now;
    int w,h;SDL_GL_GetDrawableSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.06f,.08f,.11f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    scene_shadows=true;glUseProgram(program);glUniform1i(glGetUniformLocation(program,"spritePass"),0);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);auto vp=view_projection();
    struct Draw {const Batch* batch;Mat4 model;float depth;};std::vector<Draw> translucent;
    auto draw_asset=[&](const std::string& name,const Mat4& model,unsigned phase=0) {
        auto it=assets.find(name);if(it==assets.end())return;
        for(const auto& b:it->second.gpu) {
            auto transformed=b.rotor?model*rotate_about_x(b.center,float(water_time)*7.5f+float((phase*977)%512)*6.2831853f/512.f):model;
            if(b.alpha)translucent.push_back({&b,transformed,length(transform(transformed,b.center)-eye())});else render_batch(b,transformed,vp,{1,1,1});
        }
    };
    if(assets.empty()) {
        std::vector<m3d::Vertex> floor;
        for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
            float px=x*8,pz=z*8;triangle(floor,{px,0,pz},{px+8,0,pz+8},{px+8,0,pz});triangle(floor,{px,0,pz},{px,0,pz+8},{px+8,0,pz+8});
        }
        immediate(floor,vp,{.22f,.3f,.18f});
    } else {
        draw_asset(terrain,Mat4::identity());draw_asset(water,Mat4::identity());auto catalog=document.catalog();
        // INST keeps a destroyed mesh slot, but it is an alternate state.  It
        // must not be visible until battle damage marks that furniture destroyed.
        for(unsigned i=0;i<document.instance_count();++i){auto slot=document.field(i,0x40);if(slot && slot<=catalog.size()){draw_asset(catalog[slot-1],instance(document,i),i);if(lower(catalog[slot-1])=="_4windm2.m3d")draw_asset(windmill_sails,instance(document,i)*translate({0,19,0}),i);}}
    }
    if(battle.phase==Phase::Deployment){
        std::vector<m3d::Vertex> lines;
        for(const auto& region:battle.deployment)for(size_t i=0;i<region.points.size();++i){
            auto a=region.points[i],b=region.points[(i+1)%region.points.size()];Vec3 start{a[0],0,a[1]},end{b[0],0,b[1]};unsigned steps=std::max(1u,unsigned(std::ceil(length(end-start))));
            for(unsigned k=0;k<steps;++k){Vec3 p=start+(end-start)*(float(k)/steps),q=start+(end-start)*(float(k+1)/steps);p.y=elevation(p.x,p.z)+.3f;q.y=elevation(q.x,q.z)+.3f;Vec3 n=normal(cross(q-p,{0,1,0}))*.22f;triangle(lines,p-n,p+n,q+n);triangle(lines,p-n,q+n,q-n);}
        }immediate(lines,vp,{1,.8f,.1f});
    }
    for(const auto& u:battle.units) {
        if(!u.regiment.alive)continue;
        std::vector<m3d::Vertex> troops;
        for(unsigned i=0;i<u.regiment.alive;++i) {
            Vec3 p=u.position+Vec3{(float(i%5)-2)*1.2f,.3f,float(i/5)*1.2f};
            p.y=elevation(p.x,p.z)+.3f;
            auto sprite=sprites.find(u.regiment.sprite);
            if(sprite!=sprites.end()) {
                auto& sp=sprite->second;
                unsigned direction=unsigned(int(std::lround((u.heading-yaw)*8/6.2831853f))+64)&7;
                unsigned frame=sp.animation.frame(u.moving,u.target>=0,u.animation_tick+i%3,direction);
                if(frame>=sp.frames.size())throw std::runtime_error("Animation frame exceeds sprite frame table");
                auto& f=sp.frames[frame];Vec3 right{std::cos(yaw),0,-std::sin(yaw)};
                // SPR stores signed top-left offsets from the ground origin, in
                // screen coordinates (Y down). Convert to world Y up only once.
                auto point=[&](float x,float y){return p+right*((x+f.anchor_x)*.08f)+Vec3{0,-(y+f.anchor_y)*.08f,0};};
                Vec3 a=point(0,0),b=point(float(f.width),0),c=point(float(f.width),float(f.height)),d=point(0,float(f.height));
                std::vector<m3d::Vertex> quad={{a,{},0,0},{b,{},1,0},{c,{},1,1},{a,{},0,0},{c,{},1,1},{d,{},0,1}};
                Batch batch;batch.vao=dynamic_vao;batch.vbo=dynamic_vbo;batch.texture=sp.gpu[frame];upload(batch,quad);
                glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
                glUniform1i(glGetUniformLocation(program,"spritePass"),1);render_batch(batch,Mat4::identity(),vp,{1,1,1});
                glDisable(GL_BLEND);glDepthMask(GL_TRUE);glUniform1i(glGetUniformLocation(program,"spritePass"),2);
                render_batch(batch,Mat4::identity(),vp,{1,1,1});glUniform1i(glGetUniformLocation(program,"spritePass"),0);continue;
            }
            Vec3 a=p+Vec3{-.45f,0,-.45f},b=p+Vec3{.45f,0,-.45f},c=p+Vec3{.45f,0,.45f},d=p+Vec3{-.45f,0,.45f},top=p+Vec3{0,2,0};
            triangle(troops,a,b,top);triangle(troops,b,c,top);triangle(troops,c,d,top);triangle(troops,d,a,top);
        }
        Vec3 color=u.routing?Vec3{.6f,.6f,.6f}:u.enemy?Vec3{.85f,.2f,.12f}:Vec3{.2f,.55f,.95f};
        immediate(troops,vp,color);
        if(u.selected) {
            std::vector<m3d::Vertex> ring;
            for(int i=0;i<32;++i){float a=i*6.2831853f/32,b=(i+1)*6.2831853f/32;auto point=[&](float t,float r){return u.position+Vec3{std::cos(t)*r,.15f,std::sin(t)*r};};triangle(ring,point(a,4),point(b,4),point(b,4.25f));triangle(ring,point(a,4),point(b,4.25f),point(a,4.25f));}
            immediate(ring,vp,{1,.8f,.2f});
        }
    }
    // Original projectile artwork is a camera-facing billboard along the simulated flight arc.
    for(const auto& shot:battle.projectiles){
        const auto& art=shot.artillery?cannon_projectile:bolt_projectile;if(!art.texture)continue;
        float size=shot.artillery?1.7f:1.f;Vec3 right{std::cos(yaw),0,-std::sin(yaw)},up{0,size,0};Vec3 p=shot.position;
        std::vector<m3d::Vertex> quad={{p-right*size+up,{},0,0},{p+right*size+up,{},1,0},{p+right*size-up,{},1,1},{p-right*size+up,{},0,0},{p+right*size-up,{},1,1},{p-right*size-up,{},0,1}};
        Batch batch;batch.vao=dynamic_vao;batch.vbo=dynamic_vbo;batch.texture=art.texture;upload(batch,quad);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);render_batch(batch,Mat4::identity(),vp,shot.magic?Vec3{.55f,.35f,1.f}:Vec3{1,1,1});glDisable(GL_BLEND);glDepthMask(GL_TRUE);
    }
    std::sort(translucent.begin(),translucent.end(),[](const Draw& a,const Draw& b){return a.depth>b.depth;});
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    for(const auto& d:translucent)render_batch(*d.batch,d.model,vp,{1,1,1},d.batch->opacity);
    scene_shadows=false;glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
    glBindVertexArray(0);glUseProgram(0);
}
void Renderer::load_units(const std::filesystem::path& root,const Battle& battle) {
    if(root.empty())return;
    auto load_projectile=[&](const char* name,UiImage& destination){auto path=m3d::resolve(root,std::string("Graphics/Sprites/")+name);if(!path.empty())destination=ui_texture(path,0);};
    load_projectile("BOLT.SPR",bolt_projectile);load_projectile("CANNON.SPR",cannon_projectile);
    Executable exe(m3d::resolve(root,"PRG_ENG/DarkOmen.exe"));
    for(const auto& u:battle.units) {
        unsigned id=u.regiment.sprite;if(!id || sprites.count(id))continue;
        Sprite sp;sp.animation=UnitAnimation::load(exe,id);
        auto path=m3d::resolve(root,"Graphics/Sprites/"+sp.animation.filename+".SPR");
        auto bytes=read_file(path);unsigned count=prj::u32(bytes,28);
        for(unsigned i=0;i<count;++i) {
            auto f=decode_sprite(bytes,i);GLuint texture=0;glGenTextures(1,&texture);textures.push_back(texture);
            glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,f.width,f.height,0,GL_RGBA,GL_UNSIGNED_BYTE,f.rgba.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            sp.frames.push_back(std::move(f));sp.gpu.push_back(texture);
        }
        sprites.emplace(id,std::move(sp));
    }
}
void Renderer::overlay(const std::string& title,const std::vector<std::string>& lines) {
    int w,h;SDL_GetWindowSize(window,&w,&h);glDisable(GL_DEPTH_TEST);
    Mat4 ortho=Mat4::identity();ortho.v[0]=2.f/std::max(1,w);ortho.v[5]=-2.f/std::max(1,h);ortho.v[12]=-1;ortho.v[13]=1;
    std::vector<m3d::Vertex> panel;rectangle(panel,30,90,float(w-60),float(h-210));immediate(panel,ortho,{.04f,.05f,.07f});
    text(50,110,title,{1,.8f,.4f});float y=150;
    size_t columns=size_t(std::max(10,(w-110)/12));
    for(auto line:lines) {do {text(50,y,line.substr(0,columns),{.9f,.9f,.9f});y+=24;line=line.substr(std::min(columns,line.size()));}while(!line.empty());y+=12;}
}
void Renderer::screen_image(const SpriteFrame& frame) {
    int w,h;SDL_GL_GetDrawableSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    if(!frame.width || !frame.height)return;
    if(!screen_texture)glGenTextures(1,&screen_texture);
    glBindTexture(GL_TEXTURE_2D,screen_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,frame.width,frame.height,0,GL_RGBA,GL_UNSIGNED_BYTE,frame.rgba.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    float scale=std::min(float(w)/frame.width,float(h)/frame.height),x=frame.width*scale/std::max(w,1),y=frame.height*scale/std::max(h,1);
    std::vector<m3d::Vertex> quad={{{-x,y,0},{},0,0},{{x,y,0},{},1,0},{{x,-y,0},{},1,1},{{-x,y,0},{},0,0},{{x,-y,0},{},1,1},{{-x,-y,0},{},0,1}};
    Batch b;b.vao=dynamic_vao;b.vbo=dynamic_vbo;b.texture=screen_texture;upload(b,quad);render_batch(b,Mat4::identity(),Mat4::identity(),{1,1,1});
}
void Renderer::screen_label(float y,const std::string& label,Vec3 color) {
    int w,h;SDL_GetWindowSize(window,&w,&h);float scale=std::min(w/640.f,h/480.f);
    text((w-label.size()*12.f)/2,(h-480*scale)/2+y*scale,label,color);
}
Renderer::Font& Renderer::font(const std::filesystem::path& file) {
    auto key=file.string();auto found=fonts.find(key);if(found!=fonts.end())return found->second;
    auto bytes=read_file(file);
    if(bytes.size()<0x1090 || std::memcmp(bytes.data(),"FONT",4))throw std::runtime_error("Invalid FNT font");
    auto u16=[&](size_t at){return uint16_t(bytes.at(at)|uint16_t(bytes.at(at+1))<<8);};
    auto i16=[&](size_t at){return int16_t(u16(at));};
    auto u32=[&](size_t at){return uint32_t(bytes.at(at)|uint32_t(bytes.at(at+1))<<8|uint32_t(bytes.at(at+2))<<16|uint32_t(bytes.at(at+3))<<24);};
    Font result;result.spacing=i16(4);result.line_height=u16(10);auto bitmap_data=u32(12);
    if(bitmap_data>=bytes.size())throw std::runtime_error("FNT bitmap section outside file");
    std::array<std::array<uint8_t,4>,16> palette{};
    for(unsigned i=0;i<palette.size();++i){auto value=u32(16+i*4);palette[i]={uint8_t(value>>16),uint8_t(value>>8),uint8_t(value),uint8_t(i?255:0)};}
    constexpr int atlas_width=512;int x=0,y=0,row=0;
    for(unsigned c=0;c<result.glyphs.size();++c){
        auto at=0x90+c*16;auto& glyph=result.glyphs[c];glyph.x=i16(at);glyph.y=i16(at+2);glyph.width=u16(at+4);glyph.advance=u16(at+6);glyph.height=u16(at+8);
        auto offset=u32(at+12);if(!glyph.width || !glyph.height)continue;
        if(glyph.width>atlas_width || uint64_t(bitmap_data)+offset+(uint64_t(glyph.width)*glyph.height+1)/2>bytes.size())throw std::runtime_error("FNT glyph outside bitmap data");
        if(x+glyph.width>atlas_width){x=0;y+=row;row=0;}glyph.atlas_x=x;glyph.atlas_y=y;x+=glyph.width;row=std::max(row,glyph.height);
    }
    result.width=atlas_width;result.height=unsigned(std::max(1,y+row));std::vector<uint8_t> pixels(size_t(result.width)*result.height*4);
    for(unsigned c=0;c<result.glyphs.size();++c){const auto& glyph=result.glyphs[c];if(!glyph.width || !glyph.height)continue;auto offset=bitmap_data+u32(0x90+c*16+12);
        for(int py=0;py<glyph.height;++py)for(int px=0;px<glyph.width;++px){auto packed=bytes[offset+size_t(py*glyph.width+px)/2];auto index=(px&1)?packed&15:packed>>4;auto target=(size_t(glyph.atlas_y+py)*result.width+glyph.atlas_x+px)*4;std::copy(palette[index].begin(),palette[index].end(),pixels.begin()+target);}
    }
    glGenTextures(1,&result.texture);glBindTexture(GL_TEXTURE_2D,result.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,result.width,result.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return fonts.emplace(key,std::move(result)).first->second;
}
void Renderer::screen_font_label(const std::filesystem::path& file,float y,const std::string& text) {
    if(file.empty() || text.empty())return;
    auto& f=font(file);float width=0;for(unsigned char c:text)width+=f.glyphs[c].advance+f.spacing;float cursor=320-width*.5f;
    std::vector<m3d::Vertex> vertices;
    for(unsigned char c:text){const auto& g=f.glyphs[c];if(g.width && g.height){float left=cursor+g.x,top=y+g.y+f.line_height;
            float u0=float(g.atlas_x)/f.width,v0=float(g.atlas_y)/f.height,u1=float(g.atlas_x+g.width)/f.width,v1=float(g.atlas_y+g.height)/f.height;
            vertices.push_back({{left,top,0},{},u0,v0});vertices.push_back({{left+g.width,top,0},{},u1,v0});vertices.push_back({{left+g.width,top+g.height,0},{},u1,v1});vertices.push_back({{left,top,0},{},u0,v0});vertices.push_back({{left+g.width,top+g.height,0},{},u1,v1});vertices.push_back({{left,top+g.height,0},{},u0,v1});}
        cursor+=g.advance+f.spacing;
    }
    if(vertices.empty())return;
    Batch b;b.vao=dynamic_vao;b.vbo=dynamic_vbo;b.texture=f.texture;upload(b,vertices);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);render_batch(b,Mat4::identity(),ui_matrix(),{1,1,1});glDisable(GL_BLEND);
}
Mat4 Renderer::ui_matrix() const {
    int w,h;SDL_GetWindowSize(window,&w,&h);float s=std::min(w/640.f,h/480.f);Mat4 m=Mat4::identity();
    m.v[0]=2*s/std::max(1,w);m.v[5]=-2*s/std::max(1,h);m.v[12]=-640*s/std::max(1,w);m.v[13]=480*s/std::max(1,h);return m;
}
Vec3 Renderer::ui_mouse(float x,float y) const {int w,h;SDL_GetWindowSize(window,&w,&h);float s=std::max(.001f,std::min(w/640.f,h/480.f));return {(x-(w-640*s)/2)/s,(y-(h-480*s)/2)/s,0};}
void Renderer::ui_begin(bool clear){int w,h;SDL_GL_GetDrawableSize(window,&w,&h);glViewport(0,0,w,h);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);if(clear){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}}
void Renderer::ui_text(float x,float y,const std::string& value,Vec3 color,float size){std::vector<m3d::Vertex> v;for(unsigned char c:value){auto it=glyphs.find(char(std::toupper(c)));if(it!=glyphs.end())for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(it->second[size_t(row*5+col)]=='1')rectangle(v,x+col*size,y+row*size,size,size);x+=6*size;}immediate(v,ui_matrix(),color);}
void Renderer::ui_rect(float x,float y,float w,float h,Vec3 color){std::vector<m3d::Vertex> v;rectangle(v,x,y,w,h);immediate(v,ui_matrix(),color);}
Renderer::UiImage& Renderer::ui_texture(const std::filesystem::path& file,unsigned frame){
    auto key=file.string()+"#"+std::to_string(frame);auto found=ui_images.find(key);if(found!=ui_images.end())return found->second;
    SpriteFrame pixels;auto ext=file.extension().string();for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(ext==".spr")pixels=decode_sprite(read_file(file),frame);
    else {std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> src(SDL_LoadBMP(file.c_str()),SDL_FreeSurface);if(!src)throw std::runtime_error(SDL_GetError());
        std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> rgba(SDL_ConvertSurfaceFormat(src.get(),SDL_PIXELFORMAT_RGBA32,0),SDL_FreeSurface);if(!rgba)throw std::runtime_error(SDL_GetError());
        pixels={unsigned(rgba->w),unsigned(rgba->h),prj::Bytes(size_t(rgba->w)*rgba->h*4)};for(int y=0;y<rgba->h;++y)std::memcpy(pixels.rgba.data()+size_t(y)*rgba->w*4,static_cast<uint8_t*>(rgba->pixels)+y*rgba->pitch,size_t(rgba->w)*4);
    }
    // Book illustrations use the original magenta chroma key.
    if(file.string().find("Troops")!=std::string::npos || file.string().find("troops")!=std::string::npos)for(size_t i=0;i+3<pixels.rgba.size();i+=4)if(pixels.rgba[i]==255 && pixels.rgba[i+1]==0 && pixels.rgba[i+2]==255)pixels.rgba[i+3]=0;
    UiImage image;image.width=pixels.width;image.height=pixels.height;glGenTextures(1,&image.texture);glBindTexture(GL_TEXTURE_2D,image.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,pixels.width,pixels.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return ui_images.emplace(key,image).first->second;
}
void Renderer::ui_image(const std::filesystem::path& file,unsigned frame,float x,float y,float w,float h){if(file.empty())return;auto& t=ui_texture(file,frame);if(!t.width || !t.height)return;if(w==0)w=float(t.width);if(h==0)h=float(t.height);
    std::vector<m3d::Vertex> v={{{x,y,0},{},0,0},{{x+w,y,0},{},1,0},{{x+w,y+h,0},{},1,1},{{x,y,0},{},0,0},{{x+w,y+h,0},{},1,1},{{x,y+h,0},{},0,1}};
    Batch b;b.vao=dynamic_vao;b.vbo=dynamic_vbo;b.texture=t.texture;upload(b,v);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);render_batch(b,Mat4::identity(),ui_matrix(),{1,1,1});glDisable(GL_BLEND);
}
bool Renderer::project(Vec3 p,float& x,float& y) const {auto vp=view_projection();float clipw=vp.v[3]*p.x+vp.v[7]*p.y+vp.v[11]*p.z+vp.v[15];if(clipw<=0)return false;auto q=transform(vp,p);int w,h;SDL_GetWindowSize(window,&w,&h);auto logical=ui_mouse((q.x/clipw+1)*w/2,(1-q.y/clipw)*h/2);x=logical.x;y=logical.y;return x>=0 && x<=640 && y>=0 && y<=480;}
void Renderer::portrait(const std::filesystem::path& root,unsigned id,float x,float y,float width,float height,float mouth,double time){
    if(!heads.count(id)){
        auto db=read_file(m3d::resolve(root,"Graphics/PORTRAIT/SCRIPT/HEADS.DB"));if(db.empty() || id>=db[0] || db.size()!=1+size_t(db[0])*39)throw std::runtime_error("Invalid portrait database");
        const auto* record=db.data()+1+id*39;std::string prefix{char(record[0]),char(record[1])};Head head;Vec3 lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
        for(unsigned part=0;part<6;++part){unsigned at=part<2?13+part*4:23+(part-2)*4;unsigned mesh=record[at];if(!mesh)continue;auto model=m3d::Model::open(m3d::resolve(root,"Graphics/PORTRAIT/Meshes/"+std::to_string(mesh)+".M3D"));
            for(auto& batch:model.batches){Batch gpu;gpu.texture=white;for(auto& v:batch.vertices){v.position.x+=int8_t(record[at+1]);v.position.y-=int8_t(record[at+2]);v.position.z+=int8_t(record[at+3]);lo.x=std::min(lo.x,v.position.x);lo.y=std::min(lo.y,v.position.y);lo.z=std::min(lo.z,v.position.z);hi.x=std::max(hi.x,v.position.x);hi.y=std::max(hi.y,v.position.y);hi.z=std::max(hi.z,v.position.z);}
                if(batch.material>=0){auto name=std::filesystem::path(model.textures.at(size_t(batch.material))).filename().string();auto underscore=name.find('_');if(underscore!=std::string::npos)name=prefix+name.substr(underscore);name=std::filesystem::path(name).replace_extension(".BMP").string();auto texture=m3d::resolve(root,"Graphics/PORTRAIT/Textures/"+name);if(!texture.empty())gpu.texture=ui_texture(texture,0).texture;}
                upload(gpu,batch.vertices);head.gpu.push_back(gpu);head.parts.push_back(part);
            }
        }head.center=(lo+hi)*.5f;head.radius=std::max(10.f,length(hi-lo)*.5f);heads.emplace(id,std::move(head));
    }
    auto& head=heads.at(id);int w,h;SDL_GL_GetDrawableSize(window,&w,&h);float s=std::min(w/640.f,h/480.f);
    glViewport(int((w-640*s)/2+x*s),int((h-480*s)/2+(480-y-height)*s),int(width*s),int(height*s));glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);
    auto vp=perspective(.65f,width/height,1,1000)*look_at(head.center+Vec3{0,0,-head.radius*3.1f},head.center);
    for(size_t i=0;i<head.gpu.size();++i){auto model=Mat4::identity();if(head.parts[i]==2)model.v[13]=-mouth*1.5f;if(head.parts[i]==1)model.v[12]=std::sin(time*1.2)*.12f;render_batch(head.gpu[i],model,vp,{1,1,1});}
    ui_begin();
}
void Renderer::screenshot(const std::string& file) const {
    int w,h;SDL_GL_GetDrawableSize(window,&w,&h);prj::Bytes pixels(size_t(w)*h*4),flipped(pixels.size());glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<h;++y)std::copy_n(pixels.data()+size_t(y)*w*4,size_t(w)*4,flipped.data()+size_t(h-1-y)*w*4);
    SDL_Surface* s=SDL_CreateRGBSurfaceWithFormatFrom(flipped.data(),w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);if(!s)throw std::runtime_error(SDL_GetError());int result=SDL_SaveBMP(s,file.c_str());SDL_FreeSurface(s);if(result)throw std::runtime_error(SDL_GetError());
}
}
