// lithtech::renderstyle on render styles made here with the layout the
// games' Load_LTBData reads: the header, a lighting material, a pass's
// texture stages and states, a pass with Direct3D options and a vertex
// shader's four blocks; the enumerators' names; and files that must be
// refused (another LTB type, version 2, a size that disagrees, more than
// four passes, a file cut short).
//   renderstyle_test
#include <lithtech/renderstyle.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lithtech;
using namespace lithtech::renderstyle;

static int failures = 0;
#define CHECK(c)                                                             \
    do {                                                                     \
        if (!(c)) {                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void put32(std::vector<uint8_t>& f, std::size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) f[at + std::size_t(i)] = uint8_t(v >> (8 * i));
}
static void putf(std::vector<uint8_t>& f, std::size_t at, float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    put32(f, at, u);
}
static void append32(std::vector<uint8_t>& f, uint32_t v) {
    f.resize(f.size() + 4);
    put32(f, f.size() - 4, v);
}

// A RenderPassOp: stage 0 MODULATE(TEXTURE, DIFFUSE) of texture 1 from a
// reflection vector, clamped, with a UV transform; alpha blended, read-only
// z, no culling, texture factor 0x80ff0000.
static std::vector<uint8_t> pass_op(uint32_t blend) {
    std::vector<uint8_t> p(kPassSize, 0);
    uint32_t stage0[11] = {1, 3, 2, 1, 3, 2, 1, 6, 1, 1, 2};
    for (int i = 0; i < 11; ++i) put32(p, 4 * std::size_t(i), stage0[i]);
    p[44] = 1;
    for (int i = 0; i < 16; ++i) putf(p, 48 + 4 * std::size_t(i), i % 5 == 0 ? 1.0f : 0.0f);
    putf(p, 48 + 4 * 12, 0.5f);   // a translation
    p[112] = 1;
    put32(p, 116, 2);
    put32(p, 480, blend);
    put32(p, 484, 1), put32(p, 488, 0), put32(p, 492, 0x80ff0000u), put32(p, 496, 128);
    p[500] = 1;
    put32(p, 504, 0), put32(p, 508, 3), put32(p, 512, 1);
    p[516] = 1;
    put32(p, 520, 1), putf(p, 524, 0.25f), putf(p, 528, -0.5f);
    return p;
}

static std::vector<uint8_t> make(uint16_t version = 3, bool shader = true) {
    std::vector<uint8_t> f(28, 0);
    f[0] = 5;
    f[2] = uint8_t(version), f[3] = uint8_t(version >> 8);
    f[4] = 3;   // reserved, as the packer's stack left it
    put32(f, 8, 22);
    std::vector<uint8_t> style(kMaterialSize + 4, 0);
    putf(style, 16, 0.75f);          // diffuse r
    putf(style, 64, 20.0f);          // specular power
    put32(style, kMaterialSize, 2);  // passes
    auto a = pass_op(3);
    style.insert(style.end(), a.begin(), a.end());
    style.push_back(0);
    auto b = pass_op(1);
    style.insert(style.end(), b.begin(), b.end());
    style.push_back(1);   // iHasRSD3DRP
    std::vector<uint8_t> d3d(kD3DPassSize, 0);
    d3d[0] = shader, d3d[1] = 1;
    put32(d3d, 2, 4);              // ConstVector_ConstReg1
    putf(d3d, 2 + 8, 0.5f);        // its param's y
    put32(d3d, 2 + 20, 0xffffffffu);
    put32(d3d, 2 + 84, 3);         // Light_Count
    put32(d3d, 2 + 124, 26);       // Last_ConstReg
    d3d[2 + 128] = 1;              // stream 0 has positions
    put32(d3d, 2 + 144, 1);        // and one UV set
    style.insert(style.end(), d3d.begin(), d3d.end());
    if (shader)
        for (uint32_t blk = 0; blk < 4; ++blk) {
            append32(style, blk + 1);
            for (uint32_t k = 0; k <= blk; ++k) style.push_back(uint8_t(0xa0 + blk));
        }
    put32(f, 20, uint32_t(style.size()));
    put32(f, 24, 1);
    append32(f, uint32_t(style.size()));
    f.insert(f.end(), style.begin(), style.end());
    return f;
}

static bool refused(const std::vector<uint8_t>& f) {
    try {
        read(f);
    } catch (const Error&) {
        return true;
    }
    return false;
}

int main() {
    {
        auto f = make();
        CHECK(looks_like(f.data(), f.size()));
        auto r = read(f);
        CHECK(r.version == 3 && r.reserved1 == 3 && r.reserved[0] == 22 && r.trailing == 0);
        CHECK(r.styles.size() == 1);
        const Style& s = r.styles[0];
        CHECK(s.material.diffuse.r == 0.75f && s.material.specular_power == 20.0f);
        CHECK(s.passes.size() == 2);
        const Pass& p = s.passes[0];
        const TextureStage& t = p.stages[0];
        CHECK(t.texture == 1 && t.colour_op == 3 && t.colour_arg1 == 2 && t.colour_arg2 == 1 && t.uv_source == 6);
        CHECK(t.u_address == 1 && t.filter == 2 && t.uv_transform && t.uv_matrix[0] == 1 && t.uv_matrix[12] == 0.5f);
        CHECK(t.project_tex_coord && t.tex_coord_count == 2 && p.stages[1].colour_op == 0);
        CHECK(p.blend == 3 && p.z_buffer == 1 && p.texture_factor == 0x80ff0000u && p.alpha_ref == 128 && p.dynamic_light);
        CHECK(p.alpha_test == 3 && p.fill == 1 && p.bump_env_map && p.bump_env_map_stage == 1);
        CHECK(p.bump_env_map_scale == 0.25f && p.bump_env_map_offset == -0.5f && !p.has_d3d);
        const Pass& q = s.passes[1];
        CHECK(q.blend == 1 && q.has_d3d && q.d3d.use_vertex_shader && q.d3d.expand_for_skinning);
        CHECK(q.d3d.const_reg[0] == 4 && q.d3d.const_param[0].y == 0.5f && q.d3d.const_reg[1] == -1);
        CHECK(q.d3d.light_count == 3 && q.d3d.last_reg == 26 && q.d3d.stream_position[0] && !q.d3d.stream_normal[0]);
        CHECK(q.d3d.stream_uv_count[0] == 1);
        CHECK(q.d3d.blocks.size() == 4 && q.d3d.blocks[3].size() == 4 && q.d3d.blocks[2][0] == 0xa2);

        auto ns = read(make(3, false));
        CHECK(ns.styles[0].passes[1].has_d3d && ns.styles[0].passes[1].d3d.blocks.empty());
    }
    {
        CHECK(colour_op_name(3) == "COLOROP_MODULATE" && colour_op_name(15) == "COLOROP_ADDMODINVALPHA");
        CHECK(colour_op_name(19) == "#19" && alpha_op_name(9) == "ALPHAOP_SUBTRACT");
        CHECK(blend_name(10) == "BLEND_MUL_DSTCOL_ZERO" && uv_source_name(9) == "UVFROM_WORLDSPACEREFLTVECT");
        CHECK(texture_param_name(4) == "USE_TEXTURE4" && test_name(6) == "ALPHATEST_NOTEQUAL" && z_buffer_name(2) == "NOZ");
    }
    {
        auto f = make();
        CHECK(refused({}));
        CHECK(refused(make(2)));
        auto bad = f;
        bad[0] = 1;
        CHECK(refused(bad));
        CHECK(!looks_like(bad.data(), bad.size()));
        bad = f;
        bad[28] ^= 1;   // the style's size
        CHECK(refused(bad));
        bad = f;
        put32(bad, 28 + 4 + kMaterialSize, 5);   // five passes
        CHECK(refused(bad));
        CHECK(refused(std::vector<uint8_t>(f.begin(), f.end() - 1)));
        f.push_back(0);
        CHECK(read(f).trailing == 1);
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::printf("renderstyle_test: all passed\n");
    return failures ? 1 : 0;
}
