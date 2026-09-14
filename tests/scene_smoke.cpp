#include "editor.hpp"
#include <epoxy/gl.h>
#include <iostream>
#include <stdexcept>
#include <filesystem>
namespace {
void check(bool b,const char* text) {if(!b)throw std::runtime_error(text);}
void pointer(Gtk::Widget& w,GdkEventType type,double x,double y,unsigned button=1) {
    auto e=gdk_event_new(type);e->any.window=GDK_WINDOW(g_object_ref(w.get_window()->gobj()));e->any.send_event=true;
    if(type==GDK_MOTION_NOTIFY) {e->motion.x=x;e->motion.y=y;e->motion.state=GDK_BUTTON1_MASK;}
    else {e->button.x=x;e->button.y=y;e->button.button=button;}
    gtk_widget_event(w.gobj(),e);gdk_event_free(e);
}
void click(Gtk::Widget& w,double x,double y) {pointer(w,GDK_BUTTON_PRESS,x,y);pointer(w,GDK_BUTTON_RELEASE,x,y);}
}
int main(int argc,char** argv) {
    if(argc<2) {std::cerr<<"Usage: scene_smoke level.PRJ [screenshot.png]\n";return 2;}
    auto app=Gtk::Application::create("org.neoomen.scene.test");Editor editor(argv[1]);
    int result=0,stage=0,ticks=0;uint64_t hash=0,initial_hash=0;size_t colored=0;unsigned previous_frames=0;uint32_t initial_count=editor.doc.instance_count();
    editor.scene.frame_rendered.connect([&]{
        int w=editor.scene.get_allocated_width()*editor.scene.get_scale_factor(),h=editor.scene.get_allocated_height()*editor.scene.get_scale_factor();
        std::vector<uint8_t> pixels(size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        hash=1469598103934665603ull;colored=0;
        for(size_t i=0;i<pixels.size();i+=4) {
            for(int c=0;c<3;++c){hash^=pixels[i+c];hash*=1099511628211ull;}
            if(pixels[i+1]>pixels[i+2]+10)++colored;
        }
        if(glGetError()!=GL_NO_ERROR) {std::cerr<<"OpenGL error after frame\n";result=1;}
        if(argc>2 && stage==1) {
            auto pixbuf=Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,true,8,w,h);
            for(int y=0;y<h;++y)std::copy_n(pixels.data()+size_t(h-1-y)*w*4,w*4,pixbuf->get_pixels()+y*pixbuf->get_rowstride());
            pixbuf->save(argv[2],"png");
        }
    });
    Glib::signal_timeout().connect([&]{
        try {
            if(++ticks>100)throw std::runtime_error("3D render timed out: "+editor.scene.diagnostic());
            if(editor.scene.frame_count()<=previous_frames)return true;
            previous_frames=editor.scene.frame_count();
            auto& scene=editor.scene;double x=scene.get_allocated_width()/2.0,y=scene.get_allocated_height()/2.0;
            if(stage==0) {
                check(!scene.has_error(),"GL context error");check(scene.triangle_count()>0,"No geometry uploaded");check(colored>1000,"Framebuffer contains no textured terrain");
                check(scene.diagnostic().find("asset issues")==std::string::npos,"Missing assets");
                initial_hash=hash;pointer(scene,GDK_BUTTON_PRESS,x,y);pointer(scene,GDK_MOTION_NOTIFY,x+80,y+20);pointer(scene,GDK_BUTTON_RELEASE,x+80,y+20);
            } else if(stage==1) {
                check(hash!=initial_hash,"Orbit did not change rendered image");
                editor.tool.set_active(1);click(scene,x,y);
                check(editor.doc.instance_count()==initial_count+1,"3D surface placement failed");
                check(editor.selected==int(initial_count),"Placed object not selected");
                editor.tool.set_active(0);
                // Placement hits the base; select the barrel surface above it.
                click(scene,x,y-2);check(editor.selected>=0,"3D furniture picking failed");
                editor.doc.set_field(editor.selected,0x20,1024);editor.refresh();
            } else if(stage==2) {
                auto before=editor.doc.encode();editor.scene.reload();editor.scene.queue_render();
                check(editor.doc.encode()==before,"Reload changed document");
            } else {
                check(scene.triangle_count()>0,"Reload lost geometry");check(result==0,"OpenGL render error");
                std::cout<<"3D smoke passed: textured framebuffer, orbit, placement, picking, transform refresh and reload.\n";
                editor.hide();return false;
            }
            ++stage;return true;
        } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';result=1;editor.hide();return false;}
    },100);
    app->run(editor);return result;
}
