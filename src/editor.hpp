#pragma once
#include "prj.hpp"
#include "scene.hpp"
#include <gtkmm.h>
#include <deque>
#include <functional>
#include <memory>

class Editor;
class MapView : public Gtk::DrawingArea {
public:
    explicit MapView(Editor&);
    void fit();
    void invalidate();
protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_button_release_event(GdkEventButton*) override;
    bool on_motion_notify_event(GdkEventMotion*) override;
    bool on_scroll_event(GdkEventScroll*) override;
private:
    Editor& app;
    double zoom=3, ox=24, oy=24, last_x=0, last_y=0;
    bool pan=false, painting=false, moving=false;
    int previous_x=-1,previous_y=-1;
    prj::Bytes before;
    Cairo::RefPtr<Cairo::ImageSurface> surface;
    void paint(int,int);
    void finish();
    friend class Editor;
};
class Editor : public Gtk::Window {
public:
    explicit Editor(const std::string& path = "");
    prj::Document doc;
    int selected=-1;
    MapView canvas;
    SceneView scene;
    const std::string& asset_root() const {return asset_directory;}
    unsigned layer() const;
    bool high_first() const;
    double cell_size() const;
    double origin_x() const;
    double origin_z() const;
    void changed(const prj::Bytes& before);
    void refresh();
    void select(int);
    void error(const std::string&);
    void mutate(const std::function<void()>&);
    void status(const std::string&);
    Gtk::ComboBoxText tool, layer_choice, view_choice;
    Gtk::SpinButton brush, flags, height_step, mapping_scale, mapping_x, mapping_z;
    Gtk::CheckButton nibble_order{"High nibble first"}, show_objects{"Show furniture"};
    Gtk::ComboBoxText mesh_choice;
protected:
    bool on_delete_event(GdkEventAny*) override;
    bool on_key_press_event(GdkEventKey*) override;
private:
    Gtk::Box root{Gtk::ORIENTATION_VERTICAL}, toolbar{Gtk::ORIENTATION_HORIZONTAL,6};
    Gtk::Paned split{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Notebook tabs, views;
    Gtk::Box scene_panel{Gtk::ORIENTATION_VERTICAL,4};
    Gtk::Label scene_info, scene_help;
    std::string asset_directory;
    Gtk::Entry asset_entry;
    Gtk::Box terrain_panel{Gtk::ORIENTATION_VERTICAL,8}, objects_panel{Gtk::ORIENTATION_VERTICAL,8}, project_panel{Gtk::ORIENTATION_VERTICAL,8};
    Gtk::Label status_label, terrain_info, file_info;
    Gtk::ScrolledWindow object_scroll, property_scroll;
    Gtk::Grid properties;
    Gtk::Entry base_entry, water_entry, music_entry;
    Gtk::TreeView object_list;
    struct Columns : Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<int> index;
        Gtk::TreeModelColumn<Glib::ustring> name;
        Columns() { add(index); add(name); }
    } columns;
    Glib::RefPtr<Gtk::ListStore> object_store;
    struct Property { size_t offset; double scale; bool signed_value; Gtk::SpinButton* widget; };
    std::vector<Property> property_fields;
    bool updating=false;
    std::string filename;
    prj::Bytes saved;
    std::deque<prj::Bytes> undo_stack, redo_stack;
    void load_dialog();
    bool save(bool as);
    bool discard();
    void new_dialog();
    void history(bool redo);
    void add_catalog();
    void apply_properties();
    void apply_project();
    void choose_assets();
    void sync_properties();
    void update_title();
    bool dirty() const;
    void add_button(Gtk::Box&,const std::string&,const std::function<void()>&);
    void labeled(Gtk::Box&,const std::string&,Gtk::Widget&);
    friend class MapView;
};
