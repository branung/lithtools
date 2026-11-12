// Lip-sync files (.lip; NOLF2, Contract J.A.C.K., Tron 2.0): keyframes,
// 30 a second, for a character's face nodes, played beside the voice line
// of the same name (VOICE/<n>.WAV and VOICE/<n>.LIP). Written by
// LipCompiler (Build 69's Engine/tools/lipcompiler) and read by the
// games' CNodeController::HandleNodeControlLipSync; see docs/LIP.md.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::lip {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// LIPSYNC_FILE_ID, ('L' << 24) | ('I' << 16) | ('P' << 8) | 'S', written as
// a little-endian uint32: the file starts "SPIL".
constexpr uint32_t kFileId = 0x4c495053;
constexpr uint32_t kVersion = 1;
// UpdateLipSyncControl's kfKeyFrameTime: 1/30 s a keyframe.
constexpr double kKeyframesPerSecond = 30.0;

struct Key {
    std::array<float, 3> translation{};
    std::array<float, 4> rotation{{0, 0, 0, 1}};   // LTRotation: x, y, z, w
};

struct Node {
    std::string name;     // a model node (Face_Jaw, Face_LowerLip, ...)
    std::vector<Key> keys;
};

struct LipSync {
    uint32_t version = kVersion;
    uint32_t keyframe_count = 0;
    std::vector<Node> nodes;
    std::size_t trailing = 0;   // bytes after the last node's keyframes

    double length_seconds() const { return keyframe_count / kKeyframesPerSecond; }
};

LipSync read(const uint8_t* data, std::size_t size);
inline LipSync read(const std::vector<uint8_t>& data) { return read(data.data(), data.size()); }

// The keyframe count LipCompiler writes for an animation of `ms`
// milliseconds: three keyframes (0, 33, 66) a 100 ms.
uint32_t compiled_keyframes(int ms);

// A rotation as the angles LipCompiler built it from, in degrees: about
// Y, then the turned X, then the turned Z (rRotation.Rotate(Up, y),
// Rotate(Right, x), Rotate(Forward, z): q = Ry * Rx * Rz). In LithTech's
// axes, after the compiler's Maya conversion (y and z negated).
std::array<float, 3> euler_degrees(const std::array<float, 4>& q);
// And back, as the compiler does it.
std::array<float, 4> from_euler_degrees(float x, float y, float z);

// A node's pose at a time, as UpdateLipSyncControl plays it: keyframe
// t * 30 and the next, translation blended linearly and rotation slerped
// (LTRotation::Slerp). Before 0 the first keyframe; from the last on,
// the game stops updating the node (the last keyframe here).
Key sample(const Node& node, double seconds);

}  // namespace lithtech::lip
