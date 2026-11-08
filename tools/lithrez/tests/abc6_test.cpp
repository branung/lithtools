// lithtech::abc6 on a .abc v6 made here, checked value by value against
// numbers worked out by hand from the rules in abc6.hpp: a root and a
// "deformation" child with one vertex animated, two keys (the second
// moving the root 10 along z and turning the child 90 degrees about z,
// stored conjugated and three times too long, as thousands of shipped keys
// are) and three vertices, one on the root, one on the child, one
// animated. Then a file that is not v6, and a truncated one.
//   abc6_test
#include <lithtech/abc6.hpp>

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

struct W {
    std::vector<uint8_t> b;
    void u8(unsigned v) { b.push_back(uint8_t(v)); }
    void u16(unsigned v) { u8(v & 0xff), u8(v >> 8); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) u8(v >> (8 * i)); }
    void f32(float f) { uint32_t u; std::memcpy(&u, &f, 4); u32(u); }
    void vec(float x, float y, float z) { f32(x), f32(y), f32(z); }
    void str(const std::string& s) { u16(unsigned(s.size())); b.insert(b.end(), s.begin(), s.end()); }
};

static std::vector<uint8_t> chunks(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& cs) {
    W w;
    for (std::size_t i = 0; i < cs.size(); ++i) {
        w.str(cs[i].first);
        bool last = i + 1 == cs.size();
        w.u32(last ? 0xFFFFFFFFu : uint32_t(w.b.size() + 4 + cs[i].second.size()));
        w.b.insert(w.b.end(), cs[i].second.begin(), cs[i].second.end());
    }
    return w.b;
}

static std::vector<uint8_t> model() {
    const float s = std::sqrt(0.5f);
    W head, geo, nodes, anim, dims;
    head.str("MonolithExport Model File v6");
    head.str("");
    geo.vec(-1, -1, -1);
    geo.vec(1, 1, 1);
    geo.u32(0);                 // no reduced-detail vertices
    geo.u16(0);                 // VertexStartNum
    geo.u32(1);                 // one triangle
    float uv[6] = {0, 0, 1, 0, 0, 1};
    for (float f : uv) geo.f32(f);
    geo.u16(0), geo.u16(1), geo.u16(2);
    geo.u8(0), geo.u8(0), geo.u8(127);
    geo.u32(3), geo.u32(3);     // vertices, full-detail vertices
    struct V { float x, y, z; unsigned node; } vs[3] = {{1, 2, 3, 1}, {0, 0, 0, 1}, {0, 1, 0, 0}};
    for (auto& v : vs) {
        geo.vec(v.x, v.y, v.z);
        geo.u8(0), geo.u8(0), geo.u8(127);
        geo.u8(v.node);
        geo.u16(0), geo.u16(0);
    }
    // root (one child), then arm: a deformation node animating vertex 1.
    nodes.vec(0, 0, 0), nodes.vec(0, 0, 0), nodes.str("root"), nodes.u16(0), nodes.u8(1), nodes.u32(0), nodes.u32(1);
    nodes.vec(0, 0, 0), nodes.vec(0, 0, 0), nodes.str("arm"), nodes.u16(1), nodes.u8(4 | 2), nodes.u32(1), nodes.u16(1),
        nodes.u32(0);
    anim.u32(1);
    anim.str("base");
    anim.u32(100);
    anim.vec(0, 0, 0), anim.vec(0, 0, 0);
    anim.u32(2);
    anim.u32(0), anim.vec(0, 0, 0), anim.vec(0, 0, 0), anim.str("");
    anim.u32(100), anim.vec(0, 0, 0), anim.vec(0, 0, 0), anim.str("fire");
    // root: (0,0,0) then (0,0,10), no turn.
    anim.vec(0, 0, 0), anim.f32(0), anim.f32(0), anim.f32(0), anim.f32(1);
    anim.vec(0, 0, 10), anim.f32(0), anim.f32(0), anim.f32(0), anim.f32(1);
    anim.vec(1, 1, 1), anim.vec(0, 0, 0);
    // arm: at (5,0,0), then turned 90 degrees about z, stored conjugated,
    // at length 3.
    anim.vec(5, 0, 0), anim.f32(0), anim.f32(0), anim.f32(0), anim.f32(1);
    anim.vec(5, 0, 0), anim.f32(0), anim.f32(0), anim.f32(3 * s), anim.f32(3 * s);
    anim.u8(10), anim.u8(20), anim.u8(30);   // vertex 1 at key 0
    anim.u8(20), anim.u8(20), anim.u8(30);   // and key 1
    anim.vec(0.5f, 0.5f, 0.5f), anim.vec(1, 1, 1);   // byte * 0.5 + 1
    dims.vec(1, 2, 3);
    return chunks({{"Header", head.b}, {"Geometry", geo.b}, {"Nodes", nodes.b}, {"Animation", anim.b}, {"AnimDims", dims.b}});
}

static bool near(const abc::Vec3& a, float x, float y, float z, float tol = 1e-4f) {
    return std::fabs(a[0] - x) < tol && std::fabs(a[1] - y) < tol && std::fabs(a[2] - z) < tol;
}

static std::vector<abc::Vec3> posed(const abc::Model& m, float t) {
    auto M = abc::pose(m, m.animations[0], t);
    abc::LOD lod = m.pieces[0].lods[0];
    abc::animate(m, m.animations[0], t, lod);
    std::vector<abc::Vec3> out;
    for (auto& v : lod.vertices) out.push_back(abc::skin(v, M));
    return out;
}

int main() {
    auto file = model();
    CHECK(abc6::is_model(file));
    abc6::Model raw = abc6::read(file);
    CHECK(raw.nodes.size() == 2 && raw.nodes[1].parent == 0 && raw.nodes[1].animated_vertices.size() == 1);
    abc::Model m = abc6::to_abc(raw);
    CHECK(m.pieces.size() == 1 && m.pieces[0].lods.size() == 1 && m.pieces[0].lods[0].vertices.size() == 3);
    CHECK(m.nodes.size() == 2 && m.nodes[1].parent == 0 && m.nodes[1].animated_vertices == std::vector<uint32_t>{1});
    CHECK(abc::has_vertex_animation(m));
    CHECK(m.animations.size() == 1 && m.animations[0].keyframes.size() == 2 &&
          m.animations[0].keyframes[1].command == "fire" && near(m.animations[0].dims, 1, 2, 3));
    const auto& L = m.pieces[0].lods[0];
    // The bind, frame 0: z mirrored; the animated vertex at its frame-0 bytes.
    CHECK(near(L.vertices[0].location, 6, 2, -3));
    CHECK(near(L.vertices[1].location, 11, 11, -16));
    CHECK(near(L.vertices[2].location, 0, 1, 0));
    CHECK(near(L.vertices[2].normal, 0, 0, -1));
    CHECK(L.triangles.size() == 1 && L.triangles[0].uv[1][0] == 1.0f && L.triangles[0].uv[2][1] == 1.0f);
    auto at0 = posed(m, 0);
    CHECK(near(at0[0], 6, 2, -3) && near(at0[1], 11, 11, -16) && near(at0[2], 0, 1, 0));
    // Key 1: the root at (0,0,-10); the arm turned -90 about z in the drawing
    // space, (x,y) -> (y,-x); the animated vertex at its key-1 bytes.
    auto at1 = posed(m, 100);
    CHECK(near(at1[0], 7, -1, -13));
    CHECK(near(at1[1], 16, -11, -26));
    CHECK(near(at1[2], 0, 1, -10));
    // Halfway: the root at -5 and the vertex's bytes lerped. The arm's turn
    // is LithTech 1's quat_Slerp on the raw values: their dot, 3 * sqrt(1/2),
    // passes 1, so it lerps them to (0, 0, -1.5s, 0.5 + 1.5s), and the
    // matrix normalizes them: about -68.4 degrees, not the -45 a unit key gives.
    const float s = std::sqrt(0.5f);
    const float th = 2 * std::atan2(-1.5f * s, 0.5f + 1.5f * s);
    const float px = 8.5f, py = 11;
    auto mid = posed(m, 50);
    CHECK(std::fabs(th * 180 / 3.14159265f + 68.42f) < 0.05f);
    CHECK(near(mid[1], 5 + px * std::cos(th) - py * std::sin(th), px * std::sin(th) + py * std::cos(th), -21));
    CHECK(near(mid[2], 0, 1, -5));
    // Not v6, and truncated.
    std::vector<uint8_t> v12 = chunks({{"Header", std::vector<uint8_t>{12, 0, 0, 0}}});
    CHECK(!abc6::is_model(v12));
    std::vector<uint8_t> cut(file.begin(), file.begin() + std::ptrdiff_t(file.size() - 20));
    bool threw = false;
    try {
        abc6::read(cut);
    } catch (const abc::Error&) {
        threw = true;
    }
    CHECK(threw);
    if (failures) std::printf("%d failed\n", failures);
    else std::printf("abc6_test: all passed\n");
    return failures ? 1 : 0;
}
