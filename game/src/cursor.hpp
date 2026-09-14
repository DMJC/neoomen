#pragma once
#include <SDL.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace neo {
class CursorTheme {
public:
    explicit CursorTheme(const std::filesystem::path&);
    ~CursorTheme();
    CursorTheme(const CursorTheme&)=delete;
    CursorTheme& operator=(const CursorTheme&)=delete;
    void update(const std::string& name,bool original,uint64_t milliseconds);
private:
    struct Frame {SDL_Cursor* cursor=nullptr;uint32_t duration=167;};
    std::filesystem::path root;
    std::map<std::string,std::vector<Frame>> cursors;
    SDL_Cursor* system=nullptr;SDL_Cursor* current=nullptr;
    std::string active;uint64_t started=0;
    const std::vector<Frame>& load(const std::string&);
};
}
