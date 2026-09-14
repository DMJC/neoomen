#pragma once
#include "prj.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace m3d {
struct Vec3 { float x=0,y=0,z=0; };
struct Vertex { Vec3 position, normal; float u=0,v=0; };
struct Batch { int material=-1; unsigned flags=0; std::vector<Vertex> vertices; };
struct Model {
    std::vector<std::string> textures;
    std::vector<Batch> batches;
    Vec3 minimum{1e30f,1e30f,1e30f}, maximum{-1e30f,-1e30f,-1e30f};
    size_t triangles=0;
    static Model decode(const prj::Bytes&);
    static Model open(const std::filesystem::path&);
};
unsigned render_flags(const std::string& name);
// Resolves each path component with ASCII case folding, ignoring stale build paths.
std::filesystem::path resolve(const std::filesystem::path& root, const std::string& relative);
std::filesystem::path texture_path(const std::filesystem::path& root,const std::string& name);
}
