#pragma once
#include "formats.hpp"
#include <memory>
namespace neo {
class Movie {
public:
    explicit Movie(const std::filesystem::path&,bool mute=false,bool output_audio=true);
    ~Movie();
    Movie(const Movie&)=delete;
    Movie& operator=(const Movie&)=delete;
    // Monotonic elapsed seconds since playback began. Decoding stays bounded.
    bool update(double seconds); // true when video and audio have ended
    void mute(bool);
    const SpriteFrame& frame() const;
    uint64_t video_frames() const;
    uint64_t audio_samples() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
