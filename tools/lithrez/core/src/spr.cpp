// Sprites. Layout (little-endian), as spr_Create reads it (Jupiter
// Engine/runtime/client/src/sprite.cpp; AvP2 1.0.9.6 0x00483a60):
//   u32 frame count, u32 frame rate (per second), u32 keyed,
//   u32 translucent, u32 colour key
//   per frame: u16 name length (at most 1000), the name (no NUL)

#include "lithtech/spr.hpp"

namespace lithtech::spr {

namespace {

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

}  // namespace

uint32_t Sprite::length_ms() const {
    if (frame_rate == 0) return 0;
    return (1000 / frame_rate) * uint32_t(frames.size());
}

int Sprite::frame_at(uint32_t ms) const {
    uint32_t len = length_ms();
    if (len == 0) return -1;
    return int((ms % len) / (1000 / frame_rate) % frames.size());
}

Sprite read(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < 20) throw Error("too short for a sprite header");
    Sprite s;
    uint32_t count = rd32(d);
    s.frame_rate = rd32(d + 4);
    s.keyed = rd32(d + 8);
    s.translucent = rd32(d + 12);
    s.colour_key = rd32(d + 16);
    // every frame takes at least its length
    if (count > (n - 20) / 2) throw Error("frame count " + std::to_string(count) + " too big for the file");
    std::size_t p = 20;
    s.frames.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (p + 2 > n) throw Error("ends in frame " + std::to_string(i));
        std::size_t len = std::size_t(d[p] | d[p + 1] << 8);
        p += 2;
        if (len > kMaxNameLength) throw Error("frame " + std::to_string(i) + "'s name is over 1000 bytes");
        if (p + len > n) throw Error("ends in frame " + std::to_string(i) + "'s name");
        s.frames.emplace_back(reinterpret_cast<const char*>(d + p), len);
        p += len;
    }
    s.trailing = n - p;
    return s;
}

}  // namespace lithtech::spr
