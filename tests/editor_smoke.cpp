#include "editor.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool b,const char* text) { if(!b) throw std::runtime_error(text); }
void click(Gtk::Widget& widget,double x,double y) {
    for(auto type:{GDK_BUTTON_PRESS,GDK_BUTTON_RELEASE}) {
        auto event=gdk_event_new(type);
        event->button.window=GDK_WINDOW(g_object_ref(widget.get_window()->gobj()));
        event->button.send_event=true; event->button.time=GDK_CURRENT_TIME;
        event->button.x=x;event->button.y=y;event->button.button=1;
        gtk_widget_event(widget.gobj(),event);gdk_event_free(event);
    }
}
void button(Gtk::Widget& widget,const Glib::ustring& label) {
    if(auto b=dynamic_cast<Gtk::Button*>(&widget);b && b->get_label()==label) {b->clicked();return;}
    if(auto container=dynamic_cast<Gtk::Container*>(&widget)) for(auto child:container->get_children()) {
        try {button(*child,label);return;} catch(const std::out_of_range&) {}
    }
    throw std::out_of_range("Button not found");
}
}
int main() {
    auto app=Gtk::Application::create("org.neoomen.editor.test"); Editor editor;
    int result=0;
    Glib::signal_timeout().connect_once([&]{
        try {
            editor.canvas.fit();
            auto initial=editor.doc.encode();
            double x=editor.canvas.get_allocated_width()/2.0,y=editor.canvas.get_allocated_height()/2.0;
            editor.tool.set_active(2);editor.flags.set_value(10);click(editor.canvas,x,y);
            check(editor.doc.attribute(64,64)==10,"Paint click failed");
            button(editor,"Undo");check(editor.doc.encode()==initial,"Undo failed");
            button(editor,"Redo");check(editor.doc.attribute(64,64)==10,"Redo failed");
            auto heightmap=editor.scene.heightmap_revision();editor.tool.set_active(3);click(editor.canvas,x,y);
            check(editor.doc.elevation(0,64,64)==128,"Height click failed");
            check(editor.doc.elevation(1,64,64)==0,"Height layer isolation failed");
            check(editor.scene.heightmap_revision()!=heightmap,"Height edit did not mark 3D terrain for rebuild");
            editor.doc.add_mesh("tree.m3d");editor.refresh();editor.tool.set_active(1);click(editor.canvas,x,y);
            check(editor.doc.instance_count()==1 && editor.doc.field(0,0x40)==1,"Placement failed");
            button(editor,"Duplicate");check(editor.doc.instance_count()==2,"Duplicate failed");
            button(editor,"Delete");check(editor.doc.instance_count()==1,"Delete failed");
            button(editor,"Undo");check(editor.doc.instance_count()==2,"Deletion undo failed");
            std::cout<<"GTK interaction smoke test passed.\n";
        } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';result=1;}
        editor.hide();
    },250);
    app->run(editor);return result;
}
