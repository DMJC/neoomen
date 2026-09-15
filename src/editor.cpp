#include "editor.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {
constexpr double pi=3.14159265358979323846;
void spin(Gtk::SpinButton& w,double lo,double hi,double step,unsigned digits=0,double value=0) {
    w.set_range(lo,hi); w.set_increments(step,step*8); w.set_digits(digits); w.set_value(value); w.set_numeric(true);
}
std::string display_name(const std::string& s) { return Glib::ustring(s).make_valid().raw(); }
double position(const prj::Document& d,int i,size_t off) { return int32_t(d.field(size_t(i),off))/1024.0; }
void trim_history(std::deque<prj::Bytes>& h) {
    size_t bytes=0; for(auto& b:h) bytes+=b.size();
    while(h.size()>1 && (h.size()>32 || bytes>64*1024*1024)) { bytes-=h.front().size(); h.pop_front(); }
}
}
void Editor::add_button(Gtk::Box& box,const std::string& text,const std::function<void()>& cb) {
    auto b=Gtk::manage(new Gtk::Button(text)); box.pack_start(*b,Gtk::PACK_SHRINK); b->signal_clicked().connect(cb);
}
void Editor::labeled(Gtk::Box& box,const std::string& text,Gtk::Widget& widget) {
    auto label=Gtk::manage(new Gtk::Label(text)); label->set_xalign(0); box.pack_start(*label,Gtk::PACK_SHRINK); box.pack_start(widget,Gtk::PACK_SHRINK);
}
Editor::Editor(const std::string& path):doc(prj::Document::blank(128,128)),canvas(*this),scene(*this) {
    set_default_size(1250,850); set_title("neoomen-editor — Dark Omen level editor");
    root.set_border_width(8); toolbar.set_margin_bottom(8); add(root); root.pack_start(toolbar,Gtk::PACK_SHRINK);
    add_button(toolbar,"New",[this]{new_dialog();}); add_button(toolbar,"Open…",[this]{load_dialog();});
    add_button(toolbar,"Save",[this]{save(false);}); add_button(toolbar,"Save As…",[this]{save(true);});
    add_button(toolbar,"Undo",[this]{history(false);}); add_button(toolbar,"Redo",[this]{history(true);});
    add_button(toolbar,"Fit",[this]{if(views.get_current_page()==0)scene.fit();else canvas.fit();});
    for(auto s:{"Select / move furniture","Place furniture","Paint attributes","Raise terrain","Lower terrain"}) tool.append(s);
    tool.set_active(0); toolbar.pack_start(tool,Gtk::PACK_SHRINK);
    root.pack_start(split); split.pack1(views,true,false); split.pack2(tabs,false,false); split.set_position(840);
    tabs.set_size_request(330,-1);
    views.append_page(scene_panel,"3D scene"); views.append_page(canvas,"2D terrain editor");
    views.signal_switch_page().connect([this](Gtk::Widget*,guint page){
        if(page==0) {canvas.finish();scene.sync();scene.reload();status("3D scene rebuilt from the current heightmap");}
    });
    scene_help.set_text("Left drag: orbit | Middle / right drag: pan | Wheel: zoom | F: fit | WASD: move | Click: select / place");
    scene_help.set_xalign(0); scene_help.set_line_wrap(true); scene_panel.pack_start(scene_help,Gtk::PACK_SHRINK);
    scene_panel.pack_start(scene);scene_info.set_xalign(0);scene_info.set_ellipsize(Pango::ELLIPSIZE_END);scene_panel.pack_start(scene_info,Gtk::PACK_SHRINK);
    scene.message.connect([this](const std::string& text){scene_info.set_text(text);scene_info.set_tooltip_text(text);});
    for(auto p:{&terrain_panel,&objects_panel,&project_panel}) p->set_border_width(10);
    tabs.append_page(terrain_panel,"Terrain"); tabs.append_page(objects_panel,"Furniture"); tabs.append_page(project_panel,"Project");
    terrain_info.set_xalign(0); terrain_info.set_line_wrap(true); terrain_panel.pack_start(terrain_info,Gtk::PACK_SHRINK);
    for(auto s:{"Height + attributes","Height only","Attributes only"}) view_choice.append(s);
    view_choice.set_active(0); labeled(terrain_panel,"Display",view_choice);
    layer_choice.append("Layer A"); layer_choice.append("Layer B"); layer_choice.set_active(0); labeled(terrain_panel,"Height layer",layer_choice);
    spin(brush,0,12,1,0,0); labeled(terrain_panel,"Brush radius (cells)",brush);
    spin(height_step,128,32640,128,0,128); labeled(terrain_panel,"Height change (raw units, multiples of 128)",height_step);
    spin(flags,0,15,1,0,2); labeled(terrain_panel,"Paint value (0–15)",flags);
    auto legend=Gtk::manage(new Gtk::Label("0  Passable ground\n2  Blocked (red)\n8  Water (blue)\n10  Blocked water (purple)\nFlag values 1 and 4 have unknown meanings."));
    legend->set_xalign(0); terrain_panel.pack_start(*legend,Gtk::PACK_SHRINK);
    terrain_panel.pack_start(nibble_order,Gtk::PACK_SHRINK); terrain_panel.pack_start(show_objects,Gtk::PACK_SHRINK); show_objects.set_active(true);
    auto hint=Gtk::manage(new Gtk::Label("Left drag: active tool\nMiddle / right drag: pan\nScroll: zoom at cursor\n\nHeight edits affect the selected data layer.\nThe visible M3X mesh is a separate asset."));
    hint->set_line_wrap(true); hint->set_xalign(0); terrain_panel.pack_start(*hint,Gtk::PACK_SHRINK);
    for(auto c:{&view_choice,&layer_choice}) c->signal_changed().connect([this]{canvas.invalidate();});
    nibble_order.signal_toggled().connect([this]{canvas.invalidate();}); show_objects.signal_toggled().connect([this]{canvas.queue_draw();scene.queue_render();});
    labeled(objects_panel,"Mesh for placement",mesh_choice);
    add_button(objects_panel,"Add furniture mesh…",[this]{add_catalog();});
    object_store=Gtk::ListStore::create(columns); object_list.set_model(object_store);
    object_list.append_column("#",columns.index); object_list.append_column("Mesh",columns.name);
    object_scroll.add(object_list); object_scroll.set_policy(Gtk::POLICY_AUTOMATIC,Gtk::POLICY_AUTOMATIC); object_scroll.set_size_request(-1,150);
    objects_panel.pack_start(object_scroll,Gtk::PACK_SHRINK);
    object_list.get_selection()->signal_changed().connect([this]{
        if(updating) return;
        auto row=object_list.get_selection()->get_selected(); selected=row?int((*row)[columns.index]):-1;
        sync_properties(); canvas.queue_draw(); scene.queue_render();
    });
    auto buttons=Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,6)); objects_panel.pack_start(*buttons,Gtk::PACK_SHRINK);
    add_button(*buttons,"Duplicate",[this]{if(selected>=0) mutate([this]{doc.duplicate_instance(selected); selected=int(doc.instance_count())-1;});});
    add_button(*buttons,"Delete",[this]{if(selected>=0) mutate([this]{doc.remove_instance(selected); selected=-1;});});
    properties.set_row_spacing(4); properties.set_column_spacing(8);
    struct Spec { const char* name; size_t offset; double scale; bool sign; };
    const Spec specs[]={
        {"Mesh slot (1-based)",0x40,1,false},{"X",0x10,1024,true},{"Y (up)",0x14,1024,true},{"Z",0x18,1024,true},
        {"Rotation X (degrees)",0x1c,4096.0/360,true},{"Rotation Y (stored °)",0x20,4096.0/360,true},{"Rotation Z (degrees)",0x24,4096.0/360,true},
        {"Min extent X",0x28,1024,true},{"Min extent Y",0x2c,1024,true},{"Min extent Z",0x30,1024,true},
        {"Max extent X",0x34,1024,true},{"Max extent Y",0x38,1024,true},{"Max extent Z",0x3c,1024,true},
        {"Exclude terrain",0x0c,1,false},{"Attackable",0x48,1,false},{"Toughness",0x4c,1,false},{"Wounds",0x50,1,false},
        {"Owner unit",0x58,1,false},{"Burning",0x5c,1,false},{"SFX",0x60,1,false},{"GFX",0x64,1,false},{"Locked",0x68,1,false},
        {"Exclude shadow",0x6c,1,false},{"Exclude walk",0x70,1,false},{"Magic item",0x74,1,false},{"Particle effect",0x78,1,false},
        {"Destroyed mesh slot",0x7c,1,false},{"Light",0x84,1,false},{"Light radius",0x88,1,false},{"Light ambient",0x8c,1,false}
    };
    int row=0;
    for(auto& s:specs) {
        auto label=Gtk::manage(new Gtk::Label(s.name)); label->set_xalign(0);
        auto value=Gtk::manage(new Gtk::SpinButton());
        spin(*value,s.sign?double(INT32_MIN)/s.scale:0,s.sign?double(INT32_MAX)/s.scale:double(UINT32_MAX),1/s.scale,s.scale==1?0:6);
        value->set_width_chars(12); properties.attach(*label,0,row,1,1); properties.attach(*value,1,row++,1,1);
        property_fields.push_back({s.offset,s.scale,s.sign,value});
    }
    property_scroll.add(properties); property_scroll.set_policy(Gtk::POLICY_AUTOMATIC,Gtk::POLICY_AUTOMATIC); objects_panel.pack_start(property_scroll);
    add_button(objects_panel,"Apply furniture properties",[this]{apply_properties();});
    labeled(project_panel,"3D asset folder",asset_entry); asset_entry.set_editable(false);
    add_button(project_panel,"Choose asset folder…",[this]{choose_assets();});
    add_button(project_panel,"Reload 3D assets",[this]{scene.reload();});
    labeled(project_panel,"Terrain mesh reference (game uses .m3x)",base_entry);
    labeled(project_panel,"Water mesh reference (optional)",water_entry); labeled(project_panel,"Music cue",music_entry);
    add_button(project_panel,"Apply project properties",[this]{apply_project();});
    spin(mapping_scale,0.001,1024,0.25,3,1); spin(mapping_x,-2097152,2097151,1,3,0); spin(mapping_z,-2097152,2097151,1,3,0);
    labeled(project_panel,"World units per grid cell (view / placement)",mapping_scale);
    labeled(project_panel,"Grid origin X (view / placement)",mapping_x); labeled(project_panel,"Grid origin Z (view / placement)",mapping_z);
    for(auto p:{&mapping_scale,&mapping_x,&mapping_z}) p->signal_value_changed().connect([this]{canvas.queue_draw();});
    file_info.set_xalign(0); file_info.set_line_wrap(true); project_panel.pack_start(file_info,Gtk::PACK_SHRINK);
    auto note=Gtk::manage(new Gtk::Label("For playable levels, open an existing PRJ and Save As into a copy of its mission folder.\n\nNew creates a terrain-data draft. Meshes, lighting, camera tracks and mission logic are supplied separately.\n\nGrid/world mapping and nibble order are view settings; verify against your assets."));
    note->set_line_wrap(true); note->set_xalign(0); project_panel.pack_start(*note,Gtk::PACK_SHRINK);
    status_label.set_xalign(0); status_label.set_ellipsize(Pango::ELLIPSIZE_END); status_label.set_margin_top(8); root.pack_start(status_label,Gtk::PACK_SHRINK);
    if(!path.empty()) { doc=prj::Document::open(path); filename=path;asset_directory=std::filesystem::absolute(path).parent_path().string(); }
    saved=doc.encode(); refresh(); show_all_children();
    if(path.empty()) views.set_current_page(1);
    Glib::signal_idle().connect_once([this]{canvas.fit();});
}
unsigned Editor::layer() const { return unsigned(std::max(0,layer_choice.get_active_row_number())); }
bool Editor::high_first() const { return nibble_order.get_active(); }
double Editor::cell_size() const { return mapping_scale.get_value(); }
double Editor::origin_x() const { return mapping_x.get_value(); }
double Editor::origin_z() const { return mapping_z.get_value(); }
bool Editor::dirty() const { return doc.encode()!=saved; }
void Editor::update_title() { set_title((dirty()?"* ":"")+(filename.empty()?"Untitled PRJ":std::filesystem::path(filename).filename().string())+" — neoomen-editor"); }
void Editor::status(const std::string& s) { status_label.set_text(s); }
void Editor::error(const std::string& s) { Gtk::MessageDialog dialog(*this,"Unable to complete operation",false,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_OK,true); dialog.set_secondary_text(s); dialog.run(); }
void Editor::changed(const prj::Bytes& before) {
    if(before!=doc.encode()) { undo_stack.push_back(before); trim_history(undo_stack); redo_stack.clear(); }
    refresh();
}
void Editor::mutate(const std::function<void()>& fn) {
    auto before=doc.encode(); int old_selection=selected;
    try { fn(); changed(before); }
    catch(const std::exception& e) { doc=prj::Document::decode(before); selected=old_selection; refresh(); error(e.what()); }
}
void Editor::refresh() {
    updating=true;
    int old_mesh=mesh_choice.get_active_row_number(); mesh_choice.remove_all(); auto meshes=doc.catalog();
    for(size_t i=0;i<meshes.size();++i) mesh_choice.append(std::to_string(i+1)+"  "+display_name(meshes[i]));
    if(!meshes.empty()) mesh_choice.set_active(std::clamp(old_mesh,0,int(meshes.size())-1));
    object_store->clear();
    if(selected>=int(doc.instance_count())) selected=-1;
    for(uint32_t i=0;i<doc.instance_count();++i) {
        auto it=object_store->append(); auto row=*it; auto slot=doc.field(i,0x40);
        row[columns.index]=int(i); row[columns.name]=slot && slot<=meshes.size()?display_name(meshes[slot-1]):"(no / unknown mesh)";
        if(int(i)==selected) object_list.get_selection()->select(it);
    }
    base_entry.set_text(display_name(doc.mesh())); water_entry.set_text(display_name(doc.mesh(true))); music_entry.set_text(display_name(doc.music())); music_entry.set_sensitive(doc.has_music());
    terrain_info.set_text(std::to_string(doc.width())+" × "+std::to_string(doc.height())+" height cells\n"+std::to_string(doc.attr_width())+" × "+std::to_string(doc.attr_height())+" attribute cells\nTwo independent height layers");
    auto trace=doc.trace();
    file_info.set_text(std::to_string(doc.instance_count())+" furniture instances\n"+std::to_string(doc.tail.size())+" preserved trailer bytes"+(trace?"\n"+std::to_string(trace->data.size())+" TRAC camera-track bytes":""));
    updating=false; sync_properties(); canvas.invalidate(); scene.sync(); asset_entry.set_text(asset_directory); update_title();
    status("Ready • "+std::to_string(doc.width())+" × "+std::to_string(doc.height())+" • Select a tool or open a PRJ");
}
void Editor::select(int i) {
    selected=i; updating=true; object_list.get_selection()->unselect_all();
    for(auto it=object_store->children().begin();it!=object_store->children().end();++it) if(int((*it)[columns.index])==i) {
        object_list.get_selection()->select(it); object_list.scroll_to_row(object_store->get_path(it)); break;
    }
    updating=false; sync_properties(); canvas.queue_draw(); scene.queue_render();
}
void Editor::sync_properties() {
    properties.set_sensitive(selected>=0);
    for(auto& f:property_fields) {
        double value=0;
        if(selected>=0) { auto raw=doc.field(selected,f.offset); value=(f.signed_value?double(int32_t(raw)):double(raw))/f.scale; }
        f.widget->set_value(value);
    }
}
void Editor::apply_properties() {
    if(selected<0) return;
    mutate([this]{ for(auto& f:property_fields) {
        f.widget->update(); auto n=std::llround(f.widget->get_value()*f.scale);
        if((f.offset==0x40 || f.offset==0x7c) && uint64_t(n)>doc.catalog().size()) throw std::runtime_error("Mesh slot is outside the furniture catalog");
        if((f.signed_value && (n<INT32_MIN || n>INT32_MAX)) || (!f.signed_value && (n<0 || uint64_t(n)>UINT32_MAX))) throw std::runtime_error("Property is out of range");
        doc.set_field(selected,f.offset,uint32_t(n));
    }});
}
void Editor::choose_assets() {
    Gtk::FileChooserDialog chooser(*this,"Choose mission asset folder",Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    chooser.add_button("Cancel",Gtk::RESPONSE_CANCEL);chooser.add_button("Choose",Gtk::RESPONSE_OK);
    if(!asset_directory.empty())chooser.set_current_folder(asset_directory);
    if(chooser.run()!=Gtk::RESPONSE_OK)return;
    asset_directory=chooser.get_filename();asset_entry.set_text(asset_directory);scene.sync();scene.reload();
}
void Editor::apply_project() {
    mutate([this]{
        if(base_entry.get_text()!=display_name(doc.mesh())) doc.set_mesh(base_entry.get_text());
        if(water_entry.get_text()!=display_name(doc.mesh(true))) doc.set_mesh(water_entry.get_text(),true);
        if(doc.has_music() && music_entry.get_text()!=display_name(doc.music())) doc.set_music(music_entry.get_text());
    });
}
bool Editor::discard() {
    if(!dirty()) return true;
    Gtk::MessageDialog dialog(*this,"Save changes to this PRJ?",false,Gtk::MESSAGE_QUESTION,Gtk::BUTTONS_NONE,true);
    dialog.add_button("Cancel",Gtk::RESPONSE_CANCEL); dialog.add_button("Discard",Gtk::RESPONSE_REJECT); dialog.add_button("Save",Gtk::RESPONSE_ACCEPT);
    int r=dialog.run(); dialog.hide(); return r==Gtk::RESPONSE_REJECT || (r==Gtk::RESPONSE_ACCEPT && save(false));
}
bool Editor::on_delete_event(GdkEventAny*) { return !discard(); }
void Editor::load_dialog() {
    if(!discard()) return;
    Gtk::FileChooserDialog chooser(*this,"Open Dark Omen PRJ",Gtk::FILE_CHOOSER_ACTION_OPEN);
    chooser.add_button("Cancel",Gtk::RESPONSE_CANCEL); chooser.add_button("Open",Gtk::RESPONSE_OK);
    auto filter=Gtk::FileFilter::create(); filter->set_name("Dark Omen projects (*.PRJ)"); filter->add_pattern("*.[pP][rR][jJ]"); chooser.add_filter(filter);
    if(chooser.run()!=Gtk::RESPONSE_OK) return;
    auto path=chooser.get_filename(); chooser.hide();
    try {
        auto loaded=prj::Document::open(path); doc=std::move(loaded); filename=path;asset_directory=std::filesystem::absolute(path).parent_path().string(); saved=doc.encode(); selected=-1; undo_stack.clear(); redo_stack.clear(); refresh(); canvas.fit(); scene.reload(); views.set_current_page(0);
    } catch(const std::exception& e) { error(e.what()); }
}
bool Editor::save(bool as) {
    auto path=filename;
    if(as || path.empty()) {
        Gtk::FileChooserDialog chooser(*this,"Save Dark Omen PRJ",Gtk::FILE_CHOOSER_ACTION_SAVE);
        chooser.add_button("Cancel",Gtk::RESPONSE_CANCEL); chooser.add_button("Save",Gtk::RESPONSE_OK); chooser.set_do_overwrite_confirmation(true);
        chooser.set_current_name(path.empty()?"NEWLEVEL.PRJ":std::filesystem::path(path).filename().string());
        if(!path.empty()) chooser.set_current_folder(std::filesystem::path(path).parent_path().string());
        if(chooser.run()!=Gtk::RESPONSE_OK) return false;
        path=chooser.get_filename();
    }
    try {
        doc.save(path);filename=path;asset_directory=std::filesystem::absolute(path).parent_path().string();
        auto mesh=std::filesystem::path(doc.mesh());mesh.replace_extension(".m3x");if(mesh.is_absolute())mesh=mesh.filename();
        auto generated=std::filesystem::path(asset_directory)/mesh;bool created=m3d::resolve(asset_directory,mesh.generic_string()).empty() && m3d::create_terrain_m3x(generated,doc,float(cell_size()),float(origin_x()),float(origin_z()));
        saved=doc.encode();update_title();asset_entry.set_text(asset_directory);scene.sync();scene.reload();status("Saved "+path+(created?" • Created "+generated.filename().string():""));return true;
    }
    catch(const std::exception& e) { error(e.what()); return false; }
}
void Editor::new_dialog() {
    if(!discard()) return;
    Gtk::Dialog dialog("New terrain-data draft",*this,true); Gtk::SpinButton w,h;
    spin(w,8,4096,8,0,128); spin(h,8,4096,8,0,128);
    labeled(*dialog.get_content_area(),"Width (multiple of 8)",w); labeled(*dialog.get_content_area(),"Height (multiple of 8)",h);
    Gtk::Label info("A blank PRJ needs companion assets before use in game.\nOpen an existing PRJ to preserve mission camera / editor data.");
    dialog.get_content_area()->pack_start(info); dialog.add_button("Cancel",Gtk::RESPONSE_CANCEL); dialog.add_button("Create",Gtk::RESPONSE_OK); dialog.show_all_children();
    if(dialog.run()!=Gtk::RESPONSE_OK) return;
    dialog.hide();
    try { auto next=prj::Document::blank(w.get_value_as_int(),h.get_value_as_int()); doc=std::move(next); filename.clear(); asset_directory.clear(); saved.clear(); selected=-1; undo_stack.clear(); redo_stack.clear(); refresh(); canvas.fit(); views.set_current_page(1); }
    catch(const std::exception& e) { error(e.what()); }
}
void Editor::history(bool redo) {
    auto& from=redo?redo_stack:undo_stack; auto& to=redo?undo_stack:redo_stack;
    if(from.empty()) return;
    to.push_back(doc.encode()); trim_history(to); doc=prj::Document::decode(from.back()); from.pop_back(); selected=-1; refresh();
}
void Editor::add_catalog() {
    Gtk::FileChooserDialog chooser(*this,"Add furniture mesh",Gtk::FILE_CHOOSER_ACTION_OPEN);
    chooser.add_button("Cancel",Gtk::RESPONSE_CANCEL);chooser.add_button("Add",Gtk::RESPONSE_OK);
    auto filter=Gtk::FileFilter::create();filter->set_name("Dark Omen furniture meshes (*.m3d)");filter->add_pattern("*.m3d");filter->add_pattern("*.M3D");chooser.add_filter(filter);
    if(!asset_directory.empty())chooser.set_current_folder(asset_directory);
    if(chooser.run()!=Gtk::RESPONSE_OK)return;
    auto path=std::filesystem::path(chooser.get_filename());chooser.hide();
    auto extension=path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(extension!=".m3d"){error("Choose a Dark Omen .m3d furniture mesh.");return;}
    std::string name=path.generic_string();
    if(!asset_directory.empty()) {
        std::error_code ec;auto relative=std::filesystem::relative(path,asset_directory,ec);
        if(!ec)name=relative.generic_string();
    }
    mutate([this,&name]{doc.add_mesh(name);});mesh_choice.set_active(int(doc.catalog().size())-1);
}
bool Editor::on_key_press_event(GdkEventKey* e) {
    if(e->state&GDK_CONTROL_MASK) {
        canvas.finish();
        switch(gdk_keyval_to_lower(e->keyval)) {
            case GDK_KEY_s: save(e->state&GDK_SHIFT_MASK); return true;
            case GDK_KEY_o: load_dialog(); return true;
            case GDK_KEY_n: new_dialog(); return true;
            case GDK_KEY_z: history(e->state&GDK_SHIFT_MASK); return true;
            case GDK_KEY_y: history(true); return true;
        }
    }
    return Gtk::Window::on_key_press_event(e);
}
MapView::MapView(Editor& e):app(e) {
    set_can_focus(true); add_events(Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::SMOOTH_SCROLL_MASK);
}
void MapView::fit() {
    zoom=std::max(0.05,std::min((get_allocated_width()-48.0)/app.doc.width(),(get_allocated_height()-48.0)/app.doc.height()));
    ox=(get_allocated_width()-app.doc.width()*zoom)/2; oy=(get_allocated_height()-app.doc.height()*zoom)/2; queue_draw();
}
void MapView::invalidate() { surface.clear(); queue_draw(); }
bool MapView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    auto& d=app.doc; auto w=d.width(),h=d.height();
    cr->set_source_rgb(0.075,0.085,0.10); cr->paint();
    if(!surface) {
        surface=Cairo::ImageSurface::create(Cairo::FORMAT_RGB24,int(w),int(h)); auto data=surface->get_data(); auto stride=surface->get_stride();
        int32_t low=INT32_MAX,high=INT32_MIN;
        for(uint32_t y=0;y<h;++y) for(uint32_t x=0;x<w;++x) { auto v=d.elevation(app.layer(),x,y); low=std::min(low,v); high=std::max(high,v); }
        for(uint32_t y=0;y<h;++y) for(uint32_t x=0;x<w;++x) {
            double v=double(int64_t(d.elevation(app.layer(),x,y))-low)/std::max<int64_t>(1,int64_t(high)-low);
            double r=0.20+v*0.55,g=0.25+v*0.56,b=0.18+v*0.48;
            int mode=app.view_choice.get_active_row_number(); if(mode==2) {r=0.22;g=0.25;b=0.22;}
            if(mode!=1 && x<d.attr_width() && y<d.attr_height()) {
                auto a=d.attribute(x,y,app.high_first()); double blend=mode==2?0.90:0.65;
                if(a) {
                    double ar=(a&2)?0.95:0.1,ag=(a&2)?0.22:0.5,ab=(a&8)?0.98:0.18;
                    if(a&5) {ar=0.95;ag=0.70;}
                    r=r*(1-blend)+ar*blend; g=g*(1-blend)+ag*blend; b=b*(1-blend)+ab*blend;
                }
            }
            auto pixels=reinterpret_cast<uint32_t*>(data+y*stride); pixels[x]=(uint32_t(r*255)<<16)|(uint32_t(g*255)<<8)|uint32_t(b*255);
        }
        surface->mark_dirty();
    }
    cr->save(); cr->translate(ox,oy); cr->scale(zoom,zoom); cr->set_source(surface,0,0);
    auto pattern=Cairo::RefPtr<Cairo::SurfacePattern>::cast_dynamic(cr->get_source()); if(pattern) pattern->set_filter(Cairo::FILTER_NEAREST);
    cr->rectangle(0,0,w,h); cr->fill();
    if(zoom>=6) {
        cr->set_source_rgba(0,0,0,0.22); cr->set_line_width(1/zoom);
        for(uint32_t x=0;x<=w;x+=8) {cr->move_to(x,0);cr->line_to(x,h);}
        for(uint32_t y=0;y<=h;y+=8) {cr->move_to(0,y);cr->line_to(w,y);} cr->stroke();
    }
    if(app.show_objects.get_active()) for(uint32_t i=0;i<d.instance_count();++i) {
        double x=(position(d,i,0x10)-app.origin_x())/app.cell_size(),z=(position(d,i,0x18)-app.origin_z())/app.cell_size();
        bool selected=int(i)==app.selected;
        cr->set_source_rgb(selected?1:0.95,selected?0.8:0.95,selected?0.15:0.9); cr->set_line_width((selected?2:1)/zoom);
        cr->arc(x,z,5/zoom,0,2*pi); cr->stroke();
        double angle=-int32_t(d.field(i,0x20))*2*pi/4096;
        cr->move_to(x,z); cr->line_to(x+std::sin(angle)*12/zoom,z+std::cos(angle)*12/zoom); cr->stroke();
        if(selected) {
            double minx=position(d,i,0x28)/app.cell_size(),minz=position(d,i,0x30)/app.cell_size();
            double maxx=position(d,i,0x34)/app.cell_size(),maxz=position(d,i,0x3c)/app.cell_size();
            cr->save(); cr->translate(x,z); cr->rotate(-angle); cr->rectangle(minx,minz,maxx-minx,maxz-minz); cr->stroke(); cr->restore();
        }
    }
    cr->restore(); cr->set_source_rgb(0.8,0.83,0.86); cr->move_to(12,20); cr->set_font_size(12);
    cr->show_text("TOP | X right / Z down     "+std::to_string(int(zoom*100))+"%"); return true;
}
void MapView::paint(int x,int y) {
    if(x==previous_x && y==previous_y) return;
    previous_x=x; previous_y=y; int radius=app.brush.get_value_as_int(); int tool=app.tool.get_active_row_number();
    int step=std::max(128,(app.height_step.get_value_as_int()/128)*128);
    for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
        int xx=x+dx,yy=y+dy; if(dx*dx+dy*dy>radius*radius || xx<0 || yy<0) continue;
        if(tool==2) {
            if(uint32_t(xx)<app.doc.attr_width() && uint32_t(yy)<app.doc.attr_height()) app.doc.set_attribute(xx,yy,app.flags.get_value_as_int(),app.high_first());
        } else if(uint32_t(xx)<app.doc.width() && uint32_t(yy)<app.doc.height()) {
            int64_t h=int64_t(app.doc.elevation(app.layer(),xx,yy))+(tool==3?step:-step);
            if(h<INT32_MIN || h>INT32_MAX) throw std::runtime_error("Height out of range");
            app.doc.set_elevation(app.layer(),xx,yy,int32_t(h));
        }
    }
    invalidate();
}
void MapView::finish() {
    if(painting || moving) {painting=false;moving=false;app.changed(before);before.clear();}
}
bool MapView::on_button_press_event(GdkEventButton* e) {
    grab_focus(); last_x=e->x;last_y=e->y;
    if(e->button==2 || e->button==3) {pan=true;return true;}
    if(e->button!=1 || e->type!=GDK_BUTTON_PRESS) return false;
    int x=int(std::floor((e->x-ox)/zoom)),y=int(std::floor((e->y-oy)/zoom)); auto& d=app.doc;
    if(x<0 || y<0 || uint32_t(x)>=d.width() || uint32_t(y)>=d.height()) return true;
    int tool=app.tool.get_active_row_number();
    if(tool==0) {
        double nearest=12; int found=-1;
        for(uint32_t i=0;i<d.instance_count();++i) {
            double px=ox+(position(d,i,0x10)-app.origin_x())/app.cell_size()*zoom,pz=oy+(position(d,i,0x18)-app.origin_z())/app.cell_size()*zoom;
            auto distance=std::hypot(px-e->x,pz-e->y); if(distance<nearest) {nearest=distance;found=int(i);}
        }
        app.select(found); if(found>=0 && !d.field(found,0x68)) {before=d.encode();moving=true;}
    } else if(tool==1) {
        int slot=app.mesh_choice.get_active_row_number()+1;
        if(slot<=0) {app.error("Add a furniture mesh filename before placing an instance.");return true;}
        app.mutate([&]{d.add_instance(slot,app.origin_x()+(x+0.5)*app.cell_size(),0,app.origin_z()+(y+0.5)*app.cell_size());app.selected=int(d.instance_count())-1;});
        app.status("Furniture placed at Y=0. Set its height and bounds in Furniture properties.");
    } else {
        before=d.encode();painting=true;previous_x=-1;previous_y=-1;
        try {paint(x,y);} catch(const std::exception& ex) {d=prj::Document::decode(before);painting=false;invalidate();app.error(ex.what());}
    }
    return true;
}
bool MapView::on_button_release_event(GdkEventButton* e) {
    if(e->button==2 || e->button==3) pan=false;
    if(e->button==1) finish();
    return true;
}
bool MapView::on_motion_notify_event(GdkEventMotion* e) {
    if(pan) {ox+=e->x-last_x;oy+=e->y-last_y;queue_draw();}
    int x=int(std::floor((e->x-ox)/zoom)),y=int(std::floor((e->y-oy)/zoom));
    try {
        if(painting) {
            int sx=previous_x,sy=previous_y;
            int steps=std::max(std::abs(x-sx),std::abs(y-sy));
            for(int i=1;i<=steps;++i) paint(sx+int(std::lround(double(x-sx)*i/steps)),sy+int(std::lround(double(y-sy)*i/steps)));
        }
        if(moving && app.selected>=0) {
            double px=position(app.doc,app.selected,0x10)+(e->x-last_x)/zoom*app.cell_size();
            double pz=position(app.doc,app.selected,0x18)+(e->y-last_y)/zoom*app.cell_size();
            if(px<-2097152 || px>=2097152 || pz<-2097152 || pz>=2097152) throw std::runtime_error("Furniture position out of range");
            app.doc.set_field(app.selected,0x10,uint32_t(int32_t(std::llround(px*1024))));
            app.doc.set_field(app.selected,0x18,uint32_t(int32_t(std::llround(pz*1024)))); queue_draw();
        }
    } catch(const std::exception& ex) {app.doc=prj::Document::decode(before);painting=false;moving=false;app.refresh();app.error(ex.what());}
    last_x=e->x;last_y=e->y;
    if(x>=0 && y>=0 && uint32_t(x)<app.doc.width() && uint32_t(y)<app.doc.height()) {
        std::ostringstream s; s<<"Cell "<<x<<", "<<y<<" • Height "<<app.doc.elevation(app.layer(),x,y)<<" raw";
        if(uint32_t(x)<app.doc.attr_width() && uint32_t(y)<app.doc.attr_height()) s<<" • Attributes "<<unsigned(app.doc.attribute(x,y,app.high_first()));
        app.status(s.str());
    }
    return true;
}
bool MapView::on_scroll_event(GdkEventScroll* e) {
    double delta=e->direction==GDK_SCROLL_UP?-1:e->direction==GDK_SCROLL_DOWN?1:e->delta_y;
    double next=std::clamp(zoom*std::pow(1.2,-delta),0.05,64.0);
    ox=e->x-(e->x-ox)*next/zoom;oy=e->y-(e->y-oy)*next/zoom;zoom=next;queue_draw();return true;
}
