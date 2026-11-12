// lithtech::lip on lip-sync files made here as LipCompiler writes them:
// header, node names, keyframes node by node; the compiler's keyframe
// count; its Euler angles to a rotation and back; sampling between
// keyframes as the game does; and files that must be refused (wrong id or
// version, cut short, counts too big).
//   lip_test
#include <lithtech/lip.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lithtech;

static int failures = 0;
#define CHECK(c)                                                             \
    do {                                                                     \
        if (!(c)) {                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void put32(std::vector<uint8_t>& f, uint32_t v) {
    for (int i = 0; i < 4; ++i) f.push_back(uint8_t(v >> (8 * i)));
}
static void putf(std::vector<uint8_t>& f, float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    put32(f, u);
}

// nodes x keys, keyframe k of node n: translation (n, k, 0), rotation
// about Y by 10 * k degrees.
static std::vector<uint8_t> make(const std::vector<std::string>& nodes, uint32_t keys) {
    std::vector<uint8_t> f;
    put32(f, lip::kFileId);
    put32(f, 1);
    put32(f, uint32_t(nodes.size()));
    put32(f, keys);
    for (auto& s : nodes) {
        put32(f, uint32_t(s.size()));
        f.insert(f.end(), s.begin(), s.end());
    }
    for (std::size_t n = 0; n < nodes.size(); ++n)
        for (uint32_t k = 0; k < keys; ++k) {
            putf(f, float(n)), putf(f, float(k)), putf(f, 0);
            auto q = lip::from_euler_degrees(0, 10.0f * float(k), 0);
            for (float c : q) putf(f, c);
        }
    return f;
}

static bool refused(const std::vector<uint8_t>& f) {
    try {
        lip::read(f);
    } catch (const lip::Error&) {
        return true;
    }
    return false;
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

// LipCompiler's rotation, step by step as ltrotation.h has it: Rotate(axis,
// angle) is *this = LTRotation(axis, angle) * *this, about Up(), then the
// turned Right(), then the turned Forward().
using Q = std::array<float, 4>;
static Q qmul(const Q& a, const Q& b) {
    return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}
static void rotate(Q& q, float ax, float ay, float az, float deg) {
    float h = deg * 0.0174532925199432957f * 0.5f;
    Q r{ax * std::sin(h), ay * std::sin(h), az * std::sin(h), std::cos(h)};
    q = qmul(r, q);
}
static Q compiler(float x, float y, float z) {
    Q q{0, 0, 0, 1};
    rotate(q, 2 * (q[0] * q[1] - q[3] * q[2]), 1 - 2 * (q[0] * q[0] + q[2] * q[2]), 2 * (q[1] * q[2] + q[3] * q[0]), y);   // Up()
    rotate(q, 1 - 2 * (q[1] * q[1] + q[2] * q[2]), 2 * (q[0] * q[1] + q[3] * q[2]), 2 * (q[0] * q[2] - q[3] * q[1]), x);   // Right()
    rotate(q, 2 * (q[0] * q[2] + q[3] * q[1]), 2 * (q[1] * q[2] - q[3] * q[0]), 1 - 2 * (q[0] * q[0] + q[1] * q[1]), z);   // Forward()
    return q;
}

int main() {
    {
        auto f = make({"Face_Jaw", "Face_LowerLip"}, 4);
        CHECK(std::memcmp(f.data(), "SPIL", 4) == 0);
        auto l = lip::read(f);
        CHECK(l.version == 1 && l.keyframe_count == 4 && l.trailing == 0);
        CHECK(l.nodes.size() == 2 && l.nodes[0].name == "Face_Jaw" && l.nodes[1].name == "Face_LowerLip");
        CHECK(l.nodes[1].keys.size() == 4);
        CHECK(l.nodes[1].keys[3].translation[0] == 1.0f && l.nodes[1].keys[3].translation[1] == 3.0f);
        CHECK(near(lip::euler_degrees(l.nodes[0].keys[2].rotation)[1], 20.0f));
        CHECK(near(float(l.length_seconds()), 4.0f / 30.0f));

        // halfway between keyframes 1 and 2 (1.5 / 30 s): translation
        // blended, rotation slerped (15 degrees about Y)
        auto k = lip::sample(l.nodes[1], 1.5 / 30.0);
        CHECK(near(k.translation[1], 1.5f));
        CHECK(near(lip::euler_degrees(k.rotation)[1], 15.0f));
        // before the start the first keyframe, past the end the last
        CHECK(near(lip::sample(l.nodes[1], -1).translation[1], 0));
        CHECK(near(lip::sample(l.nodes[1], 10).translation[1], 3));

        f.push_back(0);
        CHECK(lip::read(f).trailing == 1);
    }
    {
        // LipCompiler: three keyframes a 100 ms (at 0, 33 and 66)
        CHECK(lip::compiled_keyframes(100) == 3);
        CHECK(lip::compiled_keyframes(1000) == 30);
        CHECK(lip::compiled_keyframes(1066) == 32);
        CHECK(lip::compiled_keyframes(1032) == 30);
        CHECK(lip::compiled_keyframes(1033) == 31);
    }
    {
        // the compiler's angles come back, each alone and together
        for (auto a : {std::array<float, 3>{30, 0, 0}, {0, -45, 0}, {0, 0, 60}, {12, -34, 56}, {-5, 170, -20}}) {
            auto e = lip::euler_degrees(lip::from_euler_degrees(a[0], a[1], a[2]));
            CHECK(near(e[0], a[0]) && near(e[1], a[1]) && near(e[2], a[2]));
        }
        // and they are the compiler's: Ry * Rx * Rz
        for (auto a : {std::array<float, 3>{12, -34, 56}, {80, 45, -30}, {-5, 170, -20}}) {
            auto q = lip::from_euler_degrees(a[0], a[1], a[2]);
            auto c = compiler(a[0], a[1], a[2]);
            CHECK(near(q[0], c[0]) && near(q[1], c[1]) && near(q[2], c[2]) && near(q[3], c[3]));
            auto e = lip::euler_degrees(c);
            CHECK(near(e[0], a[0]) && near(e[1], a[1]) && near(e[2], a[2]));
        }
    }
    {
        auto f = make({"a"}, 2);
        CHECK(refused({}));
        CHECK(refused(std::vector<uint8_t>(f.begin(), f.begin() + 15)));
        CHECK(refused(std::vector<uint8_t>(f.begin(), f.end() - 1)));
        auto bad = f;
        bad[0] = 'L';
        CHECK(refused(bad));
        bad = f;
        bad[4] = 2;
        CHECK(refused(bad));
        bad = f;
        bad[8] = 0xff, bad[9] = 0xff;
        CHECK(refused(bad));
        bad = f;
        bad[12] = 0xff, bad[13] = 0xff, bad[14] = 0xff;
        CHECK(refused(bad));
        bad = f;
        bad[16] = 0xff;
        CHECK(refused(bad));
        // no nodes: a header only
        CHECK(lip::read(make({}, 5)).nodes.empty());
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::printf("lip_test: all passed\n");
    return failures ? 1 : 0;
}
