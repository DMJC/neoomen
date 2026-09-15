#pragma once
#include "battle.hpp"
#include "animation.hpp"
#include <SDL.h>
#include <epoxy/gl.h>
#include <map>
namespace neo {
class Renderer {
public:
    explicit Renderer(SDL_Window*);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void load(const std::filesystem::path&);
    void configure(Battle&) const;
    void draw(const Battle&,bool paused);
    void fit();
    void screen_image(const SpriteFrame&);
    void ui_begin(bool clear=false);
    // Original FNT text, centered on the fixed 640-pixel menu canvas.
    void screen_font_label(const std::filesystem::path&,float y,const std::string&);
    void ui_image(const std::filesystem::path&,unsigned frame,float x,float y,float w=0,float h=0);
    void ui_text(float x,float y,const std::string&,math3d::Vec3 color={1,1,1},float size=1);
    void ui_rect(float x,float y,float w,float h,math3d::Vec3);
    math3d::Vec3 ui_mouse(float x,float y) const;
    bool project(math3d::Vec3,float& x,float& y) const;
    void portrait(const std::filesystem::path&,unsigned head,float x,float y,float w,float h,float mouth,double time);
    void screen_label(float y,const std::string&,math3d::Vec3);
    void load_units(const std::filesystem::path&,const Battle&);
    void overlay(const std::string&,const std::vector<std::string>&);
    void orbit(float dx,float dy);
    void zoom(float amount);
    void pan(float right,float forward,float dt);
    void center_on(math3d::Vec3);
    bool ground(float x,float y,math3d::Vec3&) const;
    float elevation(float x,float z) const;
    int pick(const Battle&,float x,float y) const;
    void screenshot(const std::string&) const;
    math3d::Vec3 center{64,0,64};
    float radius=90;
    size_t triangles=0;
private:
    struct Batch {GLuint vao=0,vbo=0,texture=0;GLsizei count=0;bool alpha=false,water=false,rotor=false;float opacity=1;math3d::Vec3 center;};
    struct Asset {m3d::Model cpu;std::vector<Batch> gpu;};
    struct Sprite {UnitAnimation animation;std::vector<SpriteFrame> frames;std::vector<GLuint> gpu;};
    std::map<unsigned,Sprite> sprites;
    SDL_Window* window;
    GLuint shadow_texture=0;unsigned shadow_width=0,shadow_height=0;
    bool scene_shadows=false;
    double water_time=0;uint64_t water_clock=0;
    GLuint screen_texture=0;
    GLuint program=0,white=0,dynamic_vao=0,dynamic_vbo=0;
    std::map<std::string,Asset> assets;
    struct UiImage {GLuint texture=0;unsigned width=0,height=0;};
    std::map<std::string,UiImage> ui_images;
    UiImage bolt_projectile,cannon_projectile;
    struct FontGlyph {int x=0,y=0,width=0,height=0,advance=0,atlas_x=0,atlas_y=0;};
    struct Font {GLuint texture=0;unsigned width=0,height=0;int spacing=0,line_height=0;std::array<FontGlyph,256> glyphs{};};
    std::map<std::string,Font> fonts;
    struct Head {std::vector<Batch> gpu;std::vector<unsigned> parts;math3d::Vec3 center;float radius=25;};
    std::map<unsigned,Head> heads;
    math3d::Mat4 ui_matrix() const;
    UiImage& ui_texture(const std::filesystem::path&,unsigned);
    Font& font(const std::filesystem::path&);
    std::vector<GLuint> textures;
    prj::Document document;
    std::string terrain,water;
    math3d::Vec3 target{64,0,64};
    float yaw=-.6f,pitch=.85f,distance=260;
    math3d::Vec3 eye() const;
    math3d::Vec3 ray(float x,float y) const;
    math3d::Mat4 view_projection() const;
    void upload(Batch&,const std::vector<m3d::Vertex>&);
    GLuint texture(const std::filesystem::path&,bool key);
    void render_batch(const Batch&,const math3d::Mat4&,const math3d::Mat4&,math3d::Vec3,float=1);
    void immediate(const std::vector<m3d::Vertex>&,const math3d::Mat4&,math3d::Vec3);
    void text(float x,float y,const std::string&,math3d::Vec3);
};
}
