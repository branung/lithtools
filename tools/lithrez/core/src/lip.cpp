// Lip-sync files. Layout (little-endian), as LipCompiler's CompileFile
// writes it and HandleNodeControlLipSync reads it:
//   u32 file id (LIPSYNC_FILE_ID, bytes "SPIL"), u32 version (1)
//   u32 node count, u32 keyframe count
//   per node: u32 name length, the name (no NUL)
//   per node, per keyframe: 3 floats translation, 4 floats rotation
//     (x, y, z, w): 28 bytes, each node's keyframes in one run

#include "lithtech/lip.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lithtech::lip {

namespace {

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
float rdf(const uint8_t* p) {
    uint32_t u = rd32(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

using Quat = std::array<float, 4>;

// quat_Mul (ltquatbase.h): a * b.
Quat mul(const Quat& a, const Quat& b) {
    return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

// LTRotation(axis, angle)
Quat axis_angle(float x, float y, float z, float radians) {
    float s = std::sin(radians * 0.5f);
    return {x * s, y * s, z * s, std::cos(radians * 0.5f)};
}

constexpr float kDegToRad = 0.0174532925199432957f;   // LipCompiler's DEG2RAD

}  // namespace

uint32_t compiled_keyframes(int ms) { return uint32_t((ms / 100) * 3 + (ms % 100) / 33); }

std::array<float, 4> from_euler_degrees(float x, float y, float z) {
    // rRotation.Rotate(rRotation.Up(), y) and so on: Rotate is
    // *this = LTRotation(axis, angle) * *this, about the turned axes,
    // which comes to Ry * Rx * Rz.
    Quat q = axis_angle(0, 1, 0, y * kDegToRad);
    q = mul(q, axis_angle(1, 0, 0, x * kDegToRad));
    return mul(q, axis_angle(0, 0, 1, z * kDegToRad));
}

std::array<float, 3> euler_degrees(const std::array<float, 4>& q) {
    double x = q[0], y = q[1], z = q[2], w = q[3];
    // the matrix of Ry * Rx * Rz: m12 = -sin(ax), m02 / m22 the yaw,
    // m10 / m11 the roll
    double m02 = 2 * (x * z + w * y), m22 = 1 - 2 * (x * x + y * y);
    double m12 = 2 * (y * z - w * x);
    double m10 = 2 * (x * y + w * z), m11 = 1 - 2 * (x * x + z * z);
    double ax = std::asin(std::max(-1.0, std::min(1.0, -m12)));
    double ay = std::atan2(m02, m22);
    double az = std::atan2(m10, m11);
    const double r = 180.0 / 3.14159265358979323846;
    return {float(ax * r), float(ay * r), float(az * r)};
}

Key sample(const Node& node, double seconds) {
    if (node.keys.empty()) return {};
    double f = seconds * kKeyframesPerSecond;
    if (f <= 0) return node.keys.front();
    std::size_t i = std::size_t(f);
    if (i >= node.keys.size()) return node.keys.back();
    std::size_t j = std::min(node.keys.size() - 1, i + 1);
    float u = float(f - double(i));
    const Key& a = node.keys[i];
    const Key& b = node.keys[j];
    Key k;
    for (int c = 0; c < 3; ++c) k.translation[c] = a.translation[c] * (1.0f - u) + b.translation[c] * u;
    // quat_Slerp (ltquatbase.cpp)
    float cosom = a.rotation[0] * b.rotation[0] + a.rotation[1] * b.rotation[1] + a.rotation[2] * b.rotation[2] +
                  a.rotation[3] * b.rotation[3];
    Quat to = b.rotation;
    if (cosom < 0) {
        cosom = -cosom;
        for (float& c : to) c = -c;
    }
    float s0 = 1.0f - u, s1 = u;
    if (1.0f - cosom > 0.0001f) {
        float omega = std::acos(cosom), oosinom = 1.0f / std::sin(omega);
        s0 = std::sin((1.0f - u) * omega) * oosinom;
        s1 = std::sin(u * omega) * oosinom;
    }
    for (int c = 0; c < 4; ++c) k.rotation[c] = s0 * a.rotation[c] + s1 * to[c];
    return k;
}

LipSync read(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < 16) throw Error("too short for a lip-sync header");
    if (rd32(d) != kFileId) throw Error("not a lip-sync file (no \"SPIL\" id)");
    LipSync l;
    l.version = rd32(d + 4);
    if (l.version != kVersion)
        throw Error("version " + std::to_string(l.version) + " (the games take only 1)");
    uint32_t nodes = rd32(d + 8);
    l.keyframe_count = rd32(d + 12);
    std::size_t p = 16;
    // each node takes at least its name's length
    if (nodes > (n - p) / 4) throw Error("node count " + std::to_string(nodes) + " too big for the file");
    l.nodes.resize(nodes);
    for (uint32_t i = 0; i < nodes; ++i) {
        if (p + 4 > n) throw Error("ends in node " + std::to_string(i) + "'s name");
        uint32_t len = rd32(d + p);
        p += 4;
        if (len > n - p) throw Error("ends in node " + std::to_string(i) + "'s name");
        l.nodes[i].name.assign(reinterpret_cast<const char*>(d + p), len);
        p += len;
    }
    const std::size_t per = 28;
    if (nodes && uint64_t(l.keyframe_count) * nodes * per > n - p)
        throw Error(std::to_string(nodes) + " nodes of " + std::to_string(l.keyframe_count) +
                    " keyframes don't fit in the file");
    for (auto& node : l.nodes) {
        node.keys.resize(l.keyframe_count);
        for (auto& k : node.keys) {
            for (int c = 0; c < 3; ++c) k.translation[c] = rdf(d + p + 4 * c);
            for (int c = 0; c < 4; ++c) k.rotation[c] = rdf(d + p + 12 + 4 * c);
            p += per;
        }
    }
    l.trailing = n - p;
    return l;
}

}  // namespace lithtech::lip
