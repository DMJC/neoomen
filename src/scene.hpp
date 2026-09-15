#pragma once
#include "math3d.hpp"
#include <gtkmm.h>
#include <map>
class Editor;
class SceneView : public Gtk::GLArea {
public:
    explicit SceneView(Editor&);
    ~SceneView() override;
    void sync();
    void reload();
    void fit();
    sigc::signal<void,const std::string&> message;
    sigc::signal<void> frame_rendered;
    size_t triangle_count() const {return triangles;}
    unsigned frame_count() const {return frames;}
    uint64_t heightmap_revision() const {return heightmap_key;}
    const std::string& diagnostic() const {return info;}
protected:
    void on_realize() override;
    void on_unrealize() override;
    bool on_render(const Glib::RefPtr<Gdk::GLContext>&) override;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_button_release_event(GdkEventButton*) override;
    bool on_motion_notify_event(GdkEventMotion*) override;
    bool on_scroll_event(GdkEventScroll*) override;
    bool on_key_press_event(GdkEventKey*) override;
private:
    struct GpuBatch {unsigned vao=0,vbo=0,texture=0;int count=0;bool translucent=false;m3d::Vec3 center;};
    struct Asset {m3d::Model mesh;std::vector<GpuBatch> batches;};
    Editor& app;
    std::map<std::string,Asset> assets;
    std::vector<unsigned> textures;
    unsigned program=0,white=0;
    int matrix_location=-1,model_location=-1,tint_location=-1,opacity_location=-1;
    bool rebuild=true,auto_fit=true;
    std::string key,camera_key,base_key,water_key,info;
    uint64_t heightmap_key=0;
    size_t triangles=0;
    unsigned frames=0;
    math3d::Vec3 target{64,0,64};
    float yaw=-0.6f,pitch=0.75f,distance=240,radius=100;
    unsigned drag_button=0;
    double last_x=0,last_y=0,down_x=0,down_y=0;
    bool dragged=false;
    math3d::Vec3 eye() const;
    void destroy_meshes();
    void destroy_gl();
    void load_scene();
    void apply_heightmap(m3d::Model&);
    unsigned texture(const std::filesystem::path&,bool color_key);
    void pick(double x,double y);
};
