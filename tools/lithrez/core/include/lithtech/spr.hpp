// LithTech .spr sprites (NOLF, AvP2, NOLF2, Contract J.A.C.K., Tron 2.0):
// an animation of .dtx frames named by path. Every engine from NOLF to
// Jupiter reads the same layout (spr_Create; DEdit's CSpriteFile writes
// it); see docs/SPR.md.

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::spr {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// spr_Create refuses a frame name longer than this.
constexpr std::size_t kMaxNameLength = 1000;

struct Sprite {
    // Frames per second (the engine's SpriteAnim::m_MsFrameRate, which is
    // per second despite the name).
    uint32_t frame_rate = 0;
    // As stored: the engine keeps the low byte of the first two (to uint8)
    // and all of the colour key.
    uint32_t keyed = 0, translucent = 0, colour_key = 0;
    std::vector<std::string> frames;   // texture paths, as stored
    std::size_t trailing = 0;          // bytes after the last frame name

    // spr_Create's m_MsAnimLength: (1000 / rate) * frames, in integers. 0
    // (no frames, or a rate over 1000) stops the animation on no frame; a
    // rate of 0 the engine would divide by.
    uint32_t length_ms() const;
    // The frame spr_UpdateTracker shows at a time (looping), -1 for none.
    int frame_at(uint32_t ms) const;
};

Sprite read(const uint8_t* data, std::size_t size);
inline Sprite read(const std::vector<uint8_t>& data) { return read(data.data(), data.size()); }

}  // namespace lithtech::spr
