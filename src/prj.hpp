#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace prj {
using Bytes = std::vector<uint8_t>;
uint32_t u32(const Bytes&, size_t);
void put32(Bytes&, size_t, uint32_t);
struct Document {
    Bytes banner, base, water, furniture, instances, terrain, attributes, tail;
    static Document decode(const Bytes&);
    static Document open(const std::string&);
    static Document blank(uint32_t width, uint32_t height);
    Bytes encode() const;
    void save(const std::string&) const;
    uint32_t width() const { return u32(terrain, 8); }
    uint32_t height() const { return u32(terrain, 12); }
    uint32_t attr_width() const { return u32(attributes, 8); }
    uint32_t attr_height() const { return u32(attributes, 12); }
    uint32_t patch_count() const { return u32(terrain, 20); }
    uint32_t instance_count() const { return u32(instances, 8); }
    uint32_t record_size() const { return u32(instances, 12); }
    std::string mesh(bool water_mesh = false) const;
    void set_mesh(const std::string&, bool water_mesh = false);
    std::vector<std::string> catalog() const;
    void add_mesh(const std::string&);
    uint32_t field(size_t instance, size_t offset) const;
    void set_field(size_t instance, size_t offset, uint32_t);
    void add_instance(uint32_t slot, double x, double y, double z);
    void duplicate_instance(size_t);
    void remove_instance(size_t);
    int32_t elevation(unsigned layer, uint32_t x, uint32_t y) const;
    // Exact, lossless patch recompression; rejects unrepresentable height ranges.
    void set_elevation(unsigned layer, uint32_t x, uint32_t y, int32_t value);
    uint8_t attribute(uint32_t x, uint32_t y, bool high_first = false) const;
    void set_attribute(uint32_t x, uint32_t y, uint8_t value, bool high_first = false);
    std::string music() const;
    bool has_music() const;
    void set_music(const std::string&);
};
}
