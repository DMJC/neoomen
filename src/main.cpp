#include "editor.hpp"
#include <iostream>
int main(int argc,char** argv) {
    try {
        std::string path;
        if(argc>1) {
            if(std::string(argv[1])=="--help") {
                std::cout<<"Usage: neoomen-editor [level.PRJ]\nGTKmm Dark Omen terrain and furniture editor.\n"; return 0;
            }
            path=argv[1];
        }
        auto app=Gtk::Application::create("org.neoomen.editor");
        Editor editor(path); return app->run(editor);
    } catch(const std::exception& e) { std::cerr<<"neoomen-editor: "<<e.what()<<'\n'; return 1; }
}
