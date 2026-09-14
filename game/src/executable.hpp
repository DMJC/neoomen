#pragma once
#include "formats.hpp"
namespace neo {
// Read-only PE32 image. Virtual addresses are translated into bounded byte offsets.
class Executable {
public:
    explicit Executable(const std::filesystem::path&);
    uint32_t word(uint32_t address) const;
    std::string string(uint32_t address,size_t limit=4096) const;
    prj::Bytes bytes(uint32_t address,size_t count) const;
private:
    prj::Bytes data;
    struct Section {uint32_t address,size,offset;};
    std::vector<Section> sections;
    size_t offset(uint32_t,size_t) const;
};
}
