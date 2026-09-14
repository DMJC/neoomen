#include "scene.hpp"
#include "editor.hpp"
#include <epoxy/gl.h>
#include <fstream>
#include <sstream>
#include <set>
#include <iostream>
using namespace math3d;
namespace {
GLuint shader(GLenum type,const char* source) {
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) {char text[4096];glGetShaderInfoLog(s,sizeof(text),nullptr,text);glDeleteShader(s);throw std::runtime_error(text);}return s;
}
GLuint make_program() {
    const char* vertex=R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
uniform mat4 mvp;
uniform mat4 model;
out vec2 uv;
out vec3 n;
void main() {gl_Position=mvp*vec4(position,1);uv=texcoord;n=mat3(model)*normal;}
)";
    const char* fragment=R"(#version 330 core
in vec2 uv;
in vec3 n;
uniform sampler2D image;
uniform float selected;
uniform float opacity;
out vec4 color;
void main() {
    vec4 tex=texture(image,uv);
    if(tex.a<0.45) discard;
    float light=0.72+0.28*abs(dot(normalize(n+vec3(0,0.00001,0)),normalize(vec3(0.3,1,0.4))));
    color=vec4(mix(tex.rgb*light,vec3(1,0.72,0.12),selected*0.35),tex.a*opacity);
}
)";
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=0,p=0;
    try {
        fs=shader(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
        GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok) {char text[4096];glGetProgramInfoLog(p,sizeof(text),nullptr,text);throw std::runtime_error(text);}
    } catch(...) {glDeleteShader(vs);if(fs)glDeleteShader(fs);if(p)glDeleteProgram(p);throw;}
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
std::string terrain_name(std::string s) {std::filesystem::path p(s);p.replace_extension(".m3x");return p.string();}
}
SceneView::SceneView(Editor& e):app(e) {
    set_required_version(3,3);set_use_es(false);set_has_depth_buffer(true);set_auto_render(true);set_can_focus(true);
    add_events(Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::SMOOTH_SCROLL_MASK);
}
SceneView::~SceneView() {if(get_realized()) {make_current();if(!has_error()) destroy_gl();}}
void SceneView::sync() {
    std::string next=app.asset_root()+"\n"+app.doc.mesh()+"\n"+app.doc.mesh(true);
    if(camera_key!=next) {camera_key=next;auto_fit=true;}
    for(auto& name:app.doc.catalog()) next+='\n'+name;
    if(key!=next) {key=next;rebuild=true;}
    queue_render();
}
void SceneView::reload() {rebuild=true;queue_render();}
void SceneView::on_realize() {
    Gtk::GLArea::on_realize();make_current();
    if(has_error()) {info="OpenGL unavailable. Use the 2D terrain editor.";message.emit(info);return;}
    try {
        program=make_program();matrix_location=glGetUniformLocation(program,"mvp");model_location=glGetUniformLocation(program,"model");
        tint_location=glGetUniformLocation(program,"selected");opacity_location=glGetUniformLocation(program,"opacity");
        glUseProgram(program);glUniform1i(glGetUniformLocation(program,"image"),0);glUseProgram(0);
        glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);const uint8_t pixel[]={210,205,190,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        rebuild=true;
    } catch(const std::exception& e) {info=std::string("OpenGL initialization: ")+e.what();message.emit(info);}
}
void SceneView::destroy_meshes() {
    for(auto& pair:assets) for(auto& b:pair.second.batches) {glDeleteBuffers(1,&b.vbo);glDeleteVertexArrays(1,&b.vao);}
    for(auto id:textures) glDeleteTextures(1,&id);
    textures.clear();assets.clear();triangles=0;
}
void SceneView::destroy_gl() {destroy_meshes();if(white)glDeleteTextures(1,&white);if(program)glDeleteProgram(program);white=program=0;}
void SceneView::on_unrealize() {make_current();if(!has_error())destroy_gl();Gtk::GLArea::on_unrealize();}
unsigned SceneView::texture(const std::filesystem::path& path,bool color_key) {
    auto image=Gdk::Pixbuf::create_from_file(path.string());int w=image->get_width(),h=image->get_height();
    if(w>8192 || h>8192) throw std::runtime_error("Texture exceeds 8192 pixels: "+path.string());
    std::array<uint8_t,3> key_color={0,0,0};
    if(color_key) {
        std::ifstream f(path,std::ios::binary);prj::Bytes header(138,0);f.read(reinterpret_cast<char*>(header.data()),header.size());
        if(header[0]=='B' && header[1]=='M') {
            auto dib=prj::u32(header,14);size_t palette=14+dib;
            // Indexed BMP palette entry zero is the cutout color.
            if((header[28]==8 || header[28]==4 || header[28]==1) && palette+4<=header.size()) key_color={header[palette+2],header[palette+1],header[palette]};
        }
    }
    prj::Bytes rgba(size_t(w)*h*4);int channels=image->get_n_channels(),stride=image->get_rowstride();auto source=image->get_pixels();
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const auto* p=source+y*stride+x*channels;auto* out=rgba.data()+(size_t(y)*w+x)*4;
        out[0]=p[0];out[1]=p[1];out[2]=p[2];out[3]=channels==4?p[3]:255;
        if(color_key && p[0]==key_color[0] && p[1]==key_color[1] && p[2]==key_color[2]) out[3]=0;
    }
    unsigned id=0;glGenTextures(1,&id);textures.push_back(id);glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glGenerateMipmap(GL_TEXTURE_2D);return id;
}
void SceneView::load_scene() {
    destroy_meshes();rebuild=false;std::vector<std::string> issues;std::map<std::string,unsigned> texture_cache;
    auto root=std::filesystem::path(app.asset_root());base_key=terrain_name(app.doc.mesh());water_key=app.doc.mesh(true).empty()?"":terrain_name(app.doc.mesh(true));
    std::set<std::string> names;names.insert(base_key);if(!water_key.empty())names.insert(water_key);
    for(auto& name:app.doc.catalog()) names.insert(name);
    for(const auto& name:names) {
        if(name.empty())continue;
        auto path=m3d::resolve(root,name);
        if(path.empty()) {issues.push_back("Missing mesh: "+name);continue;}
        try {
            auto model=m3d::Model::open(path);auto& asset=assets[name];asset.mesh=std::move(model);
            for(auto& batch:asset.mesh.batches) {
                GpuBatch gpu;bool keying=((batch.flags|m3d::render_flags(name))&16)!=0;
                gpu.translucent=name==water_key || ((batch.flags|m3d::render_flags(name))&1)!=0;
                gpu.texture=white;
                if(batch.material>=0) {
                    const auto& filename=asset.mesh.textures.at(size_t(batch.material));auto texture_file=m3d::texture_path(root,filename);
                    std::string cache_key=texture_file.string()+"|"+std::to_string(keying);
                    if(texture_file.empty()) {issues.push_back("Missing texture: "+filename);}
                    else {
                        if(!texture_cache.count(cache_key)) {
                            try {texture_cache[cache_key]=texture(texture_file,keying);} catch(const Glib::Error& e) {issues.push_back(filename+": "+e.what());texture_cache[cache_key]=white;}
                            catch(const std::exception& e) {issues.push_back(filename+": "+e.what());texture_cache[cache_key]=white;}
                        }
                        gpu.texture=texture_cache[cache_key];
                    }
                }
                gpu.count=int(batch.vertices.size());
                for(auto& v:batch.vertices)gpu.center=gpu.center+v.position;
                gpu.center=gpu.center*(1.0f/std::max(1,gpu.count));
                glGenVertexArrays(1,&gpu.vao);glGenBuffers(1,&gpu.vbo);glBindVertexArray(gpu.vao);glBindBuffer(GL_ARRAY_BUFFER,gpu.vbo);
                glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(batch.vertices.size()*sizeof(m3d::Vertex)),batch.vertices.data(),GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,position)));
                glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,normal)));
                glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(m3d::Vertex),reinterpret_cast<void*>(offsetof(m3d::Vertex,u)));
                asset.batches.push_back(gpu);
            }
            triangles+=asset.mesh.triangles;
        } catch(const std::exception& e) {issues.push_back(name+": "+e.what());}
    }
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    std::sort(issues.begin(),issues.end());issues.erase(std::unique(issues.begin(),issues.end()),issues.end());
    info=std::to_string(assets.size())+" meshes • "+std::to_string(triangles)+" unique triangles • "+std::to_string(textures.size())+" textures";
    if(!issues.empty()) {
        info+=" • "+std::to_string(issues.size())+" asset issues: "+issues.front();
        for(auto& issue:issues) std::cerr<<"3D: "<<issue<<'\n';
    }
    message.emit(info);
    if(auto_fit) {fit();auto_fit=false;}
}
Vec3 SceneView::eye() const {return target+Vec3{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)}*distance;}
void SceneView::fit() {
    auto it=assets.find(base_key);
    if(it!=assets.end()) {target=(it->second.mesh.minimum+it->second.mesh.maximum)*0.5f;radius=std::max(1.0f,length(it->second.mesh.maximum-it->second.mesh.minimum)*0.5f);}
    else {target={app.doc.width()*0.5f,0,app.doc.height()*0.5f};radius=std::max(app.doc.width(),app.doc.height())*0.7f;}
    float aspect=float(std::max(1,get_allocated_width()))/std::max(1,get_allocated_height());
    float angle=std::min(0.42f,std::atan(std::tan(0.42f)*aspect));distance=radius/std::sin(angle)*1.08f;queue_render();
}
bool SceneView::on_render(const Glib::RefPtr<Gdk::GLContext>&) {
    if(!program)return false;
    if(rebuild)load_scene();
    glViewport(0,0,get_allocated_width()*get_scale_factor(),get_allocated_height()*get_scale_factor());
    glClearColor(0.075f,0.10f,0.13f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);
    glUseProgram(program);glActiveTexture(GL_TEXTURE0);
    float aspect=float(std::max(1,get_allocated_width()))/std::max(1,get_allocated_height());
    Mat4 vp=perspective(0.84f,aspect,std::max(0.02f,distance/10000),std::max(1000.0f,distance+radius*12))*look_at(eye(),target);
    struct Draw {const GpuBatch* batch;Mat4 model;bool selected;float depth;};std::vector<Draw> opaque,transparent;
    auto enqueue=[&](const std::string& name,const Mat4& model,bool selected) {
        auto it=assets.find(name);if(it==assets.end())return;
        for(auto& b:it->second.batches) (b.translucent?transparent:opaque).push_back({&b,model,selected,length(transform(model,b.center)-eye())});
    };
    enqueue(base_key,Mat4::identity(),false);if(!water_key.empty())enqueue(water_key,Mat4::identity(),false);
    auto catalog=app.doc.catalog();
    if(app.show_objects.get_active())for(uint32_t i=0;i<app.doc.instance_count();++i) {
        auto slot=app.doc.field(i,0x40);if(slot && slot<=catalog.size())enqueue(catalog[slot-1],instance(app.doc,i),int(i)==app.selected);
    }
    auto draw=[&](const Draw& d) {
        auto mvp=vp*d.model;glUniformMatrix4fv(matrix_location,1,GL_FALSE,mvp.v);glUniformMatrix4fv(model_location,1,GL_FALSE,d.model.v);
        glUniform1f(tint_location,d.selected?1:0);glUniform1f(opacity_location,d.batch->translucent?0.65f:1);
        glBindTexture(GL_TEXTURE_2D,d.batch->texture);glBindVertexArray(d.batch->vao);glDrawArrays(GL_TRIANGLES,0,d.batch->count);
    };
    glDisable(GL_BLEND);glDepthMask(GL_TRUE);for(auto& d:opaque)draw(d);
    std::sort(transparent.begin(),transparent.end(),[](const Draw& a,const Draw& b){return a.depth>b.depth;});
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);for(auto& d:transparent)draw(d);
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,0);glUseProgram(0);++frames;frame_rendered.emit();return true;
}
bool SceneView::on_button_press_event(GdkEventButton* e) {
    if(e->button>3)return false;
    grab_focus();drag_button=e->button;last_x=down_x=e->x;last_y=down_y=e->y;dragged=false;return true;
}
bool SceneView::on_button_release_event(GdkEventButton* e) {
    if(e->button==drag_button) {if(e->button==1 && !dragged)pick(e->x,e->y);drag_button=0;}return true;
}
bool SceneView::on_motion_notify_event(GdkEventMotion* e) {
    if(!drag_button)return false;
    float dx=float(e->x-last_x),dy=float(e->y-last_y);last_x=e->x;last_y=e->y;
    if(std::hypot(e->x-down_x,e->y-down_y)>3)dragged=true;
    if(!dragged)return true;
    if(drag_button==1) {yaw-=dx*0.008f;pitch=std::clamp(pitch+dy*0.008f,-1.50f,1.50f);}
    else {
        auto basis=camera_basis(eye(),target);Vec3 right=basis.right,up=basis.up;
        float scale=2*distance*std::tan(0.42f)/std::max(1,get_allocated_height());target=target+right*(-dx*scale)+up*(dy*scale);
    }
    queue_render();return true;
}
bool SceneView::on_scroll_event(GdkEventScroll* e) {
    double delta=e->direction==GDK_SCROLL_UP?-1:e->direction==GDK_SCROLL_DOWN?1:e->delta_y;
    distance=std::clamp(distance*float(std::pow(1.15,delta)),0.2f,std::max(2000.0f,radius*30));queue_render();return true;
}
bool SceneView::on_key_press_event(GdkEventKey* e) {
    if(e->state&GDK_CONTROL_MASK)return false;
    float step=distance*0.04f;Vec3 right{-std::cos(yaw),0,std::sin(yaw)},forward{-std::sin(yaw),0,-std::cos(yaw)};
    switch(gdk_keyval_to_lower(e->keyval)) {
        case GDK_KEY_f:fit();return true;
        case GDK_KEY_a:target=target-right*step;break;
        case GDK_KEY_d:target=target+right*step;break;
        case GDK_KEY_w:target=target+forward*step;break;
        case GDK_KEY_s:target=target-forward*step;break;
        case GDK_KEY_Left:yaw-=0.1f;break;
        case GDK_KEY_Right:yaw+=0.1f;break;
        case GDK_KEY_Up:pitch=std::min(1.5f,pitch+0.1f);break;
        case GDK_KEY_Down:pitch=std::max(-1.5f,pitch-0.1f);break;
        default:return Gtk::GLArea::on_key_press_event(e);
    }
    queue_render();return true;
}
void SceneView::pick(double x,double y) {
    Vec3 origin=eye();auto basis=camera_basis(origin,target);
    Vec3 forward=basis.forward,right=basis.right,up=basis.up;
    float w=float(std::max(1,get_allocated_width())),h=float(std::max(1,get_allocated_height()));
    Vec3 ray=normal(forward+right*(float(2*x/w-1)*w/h*std::tan(0.42f))+up*(float(1-2*y/h)*std::tan(0.42f)));
    float distance=1e30f;int picked=-1;bool terrain_hit=false;
    auto hit=[&](const std::string& name,const Mat4& model,int index) {
        auto it=assets.find(name);if(it==assets.end())return;
        for(auto& b:it->second.mesh.batches)for(size_t v=0;v+2<b.vertices.size();v+=3) {
            if(ray_triangle(origin,ray,transform(model,b.vertices[v].position),transform(model,b.vertices[v+1].position),transform(model,b.vertices[v+2].position),distance)) {picked=index;terrain_hit=index==-1;}
        }
    };
    hit(base_key,Mat4::identity(),-1);
    bool place=app.tool.get_active_row_number()==1;auto catalog=app.doc.catalog();
    if(!place && app.show_objects.get_active())for(uint32_t i=0;i<app.doc.instance_count();++i) {
        auto slot=app.doc.field(i,0x40);if(slot && slot<=catalog.size())hit(catalog[slot-1],instance(app.doc,i),int(i));
    }
    if(place && terrain_hit) {
        int slot=app.mesh_choice.get_active_row_number()+1;
        if(slot<=0) {app.error("Add a furniture mesh filename before placing an instance.");return;}
        Vec3 p=origin+ray*distance;app.mutate([&]{app.doc.add_instance(slot,p.x,p.y,p.z);app.selected=int(app.doc.instance_count())-1;});
    } else if(!place)app.select(picked);
    queue_render();
}
