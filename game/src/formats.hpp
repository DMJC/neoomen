#pragma once
#include "prj.hpp"
#include <filesystem>
#include <array>
namespace neo {
prj::Bytes read_file(const std::filesystem::path&);
struct Regiment {
    uint32_t id=0, attributes=0;
    uint16_t sprite=0;
    uint8_t maximum=0, alive=0;
    std::string name, leader;
    uint32_t status=1;
    uint8_t unit_class=0,missile_weapon=0,race=0;
    uint16_t banner=0,cost=0,head=0,experience=0,magic_book=0;
    uint8_t wizard=0,armour=0;
    std::array<uint8_t,9> stats{};
    std::array<uint16_t,3> items{};
};
struct Army {
    uint8_t race=0;
    uint16_t gold=0;
    std::string name;
    std::vector<Regiment> regiments;
    static Army decode(const prj::Bytes&);
};
struct BtbChunk {uint32_t type;prj::Bytes payload;};
// Preserve numeric tags and opaque payloads alongside the interpreted setup.
std::vector<BtbChunk> decode_btb(const prj::Bytes&);
struct BattleNode {
    uint32_t flags=0,unit_id=0,script_id=0;
    int16_t x=0,z=0;
    uint32_t group=0;
    uint16_t heading=0,radius=48;
};
struct BattleRegion {
    std::string name;uint32_t flags=0;
    std::vector<std::array<float,2>> points;
    bool contains(float x,float z) const;
};
struct BattleSetup {
    uint32_t width=0,height=0;
    std::string player_army,enemy_army,script;
    std::vector<BattleNode> nodes;
    std::vector<BattleRegion> regions;
    static BattleSetup decode(const prj::Bytes&);
};
// anchor_x/y are signed top-left offsets relative to the sprite origin (Y down).
struct ShadowMap {
    unsigned width=0,height=0;
    std::vector<float> heights;
    static ShadowMap decode(const prj::Bytes&);
};
struct SpriteFrame {unsigned width=0,height=0;prj::Bytes rgba;int anchor_x=0,anchor_y=0;};
SpriteFrame decode_sprite(const prj::Bytes&,unsigned frame);
std::vector<std::array<float,2>> decode_travel_route(const prj::Bytes&,unsigned route);
struct Pcm {unsigned channels=0;std::vector<int16_t> samples;};
Pcm decode_adpcm(const prj::Bytes&,unsigned channels);
}
