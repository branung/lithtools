// Render styles. Layout (little-endian, MSVC's layout of the structures:
// bools are a byte, padded to the next 4), as the three games' Load_LTBData
// reads it (NOLF2's Lithtech.exe 0x515ce0, Tron 2.0's and Contract
// J.A.C.K.'s the same):
//   LTB_Header (20): u8 file type (5), pad, u16 version (3), u8 reserved,
//     pad 3, u32 reserved x 3
//   u32 total size (refused over 1,000,000), u32 style count (over 32)
//   per style: u32 size, LightingMaterial (68), u32 pass count (asserted at
//     most 4), per pass RenderPassOp (532) and u8 iHasRSD3DRP; if set,
//     RSD3DRenderPass: u8 bUseVertexShader, u8 bExpandForSkinning, then the
//     160 bytes from ConstVector_ConstReg1 on; and with a vertex shader,
//     four blocks of a u32 size and that many bytes, which it skips
// TextureStageOps (120): 11 enums (u32), bool UVTransform_Enable at 44,
// 16 floats at 48, bool ProjectTexCoord at 112, u32 TexCoordCount at 116.
// RenderPassOp (532): 4 stages, then at 480 BlendMode, ZBufferMode,
// CullMode, TextureFactor, AlphaRef, bool DynamicLight (500),
// ZBufferTestMode (504), AlphaTestMode, FillMode, bool bUseBumpEnvMap
// (516), BumpEnvMapStage (520), fBumpEnvMap_Scale, fBumpEnvMap_Offset.
// Build 69's later engine reads shader IDs instead of RSD3DRenderPass and
// an RSD3DOptions after each style; none of the games' files have those.

#include "lithtech/renderstyle.hpp"

#include <cstring>

namespace lithtech::renderstyle {

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

Colour colour(const uint8_t* p) { return {rdf(p), rdf(p + 4), rdf(p + 8), rdf(p + 12)}; }

TextureStage stage(const uint8_t* p) {
    TextureStage s;
    uint32_t* e[] = {&s.texture,   &s.colour_op, &s.colour_arg1, &s.colour_arg2, &s.alpha_op, &s.alpha_arg1,
                     &s.alpha_arg2, &s.uv_source, &s.u_address,  &s.v_address,   &s.filter};
    for (int i = 0; i < 11; ++i) *e[i] = rd32(p + 4 * i);
    s.uv_transform = p[44] != 0;
    for (int i = 0; i < 16; ++i) s.uv_matrix[std::size_t(i)] = rdf(p + 48 + 4 * i);
    s.project_tex_coord = p[112] != 0;
    s.tex_coord_count = rd32(p + 116);
    return s;
}

Pass pass(const uint8_t* p) {
    Pass r;
    for (int i = 0; i < 4; ++i) r.stages[std::size_t(i)] = stage(p + kStageSize * std::size_t(i));
    const uint8_t* q = p + 480;
    r.blend = rd32(q), r.z_buffer = rd32(q + 4), r.cull = rd32(q + 8);
    r.texture_factor = rd32(q + 12), r.alpha_ref = rd32(q + 16);
    r.dynamic_light = q[20] != 0;
    r.z_test = rd32(q + 24), r.alpha_test = rd32(q + 28), r.fill = rd32(q + 32);
    r.bump_env_map = q[36] != 0;
    r.bump_env_map_stage = rd32(q + 40);
    r.bump_env_map_scale = rdf(q + 44), r.bump_env_map_offset = rdf(q + 48);
    return r;
}

std::string name(uint32_t v, std::initializer_list<const char*> names) {
    if (v < names.size()) return names.begin()[v];
    return "#" + std::to_string(v);
}

}  // namespace

bool looks_like(const uint8_t* d, std::size_t n) { return n >= kHeaderSize && d[0] == kFileType; }

namespace {

D3DPass d3d_pass(const uint8_t* p) {
    D3DPass r;
    r.use_vertex_shader = p[0] != 0;
    r.expand_for_skinning = p[1] != 0;
    const uint8_t* q = p + 2;   // RSD3DRenderPass's offset 4 on
    for (int i = 0; i < 3; ++i) {
        r.const_reg[i] = int32_t(rd32(q + 20 * i));
        r.const_param[i] = {rdf(q + 20 * i + 4), rdf(q + 20 * i + 8), rdf(q + 20 * i + 12), rdf(q + 20 * i + 16)};
    }
    auto s32 = [&](int o) { return int32_t(rd32(q + o)); };
    r.world_view_reg = s32(60), r.world_view_count = rd32(q + 64);
    r.proj_reg = s32(68), r.world_view_proj_reg = s32(72), r.view_proj_reg = s32(76), r.cam_pos_reg = s32(80);
    r.light_count = rd32(q + 84);
    r.light_pos_model_reg = s32(88), r.light_pos_camera_reg = s32(92), r.light_colour_reg = s32(96), r.light_att_reg = s32(100);
    r.material_amb_dif_em_reg = s32(104), r.material_specular_reg = s32(108), r.ambient_light_reg = s32(112);
    r.prev_world_view_reg = s32(116), r.prev_world_view_count = rd32(q + 120);
    r.last_reg = s32(124);
    for (int i = 0; i < 4; ++i) {
        r.stream_position[std::size_t(i)] = q[128 + i] != 0;
        r.stream_normal[std::size_t(i)] = q[132 + i] != 0;
        r.stream_uvsets[std::size_t(i)] = q[136 + i] != 0;
        r.stream_basis[std::size_t(i)] = q[140 + i] != 0;
        r.stream_uv_count[std::size_t(i)] = s32(144 + 4 * i);
    }
    return r;
}

}  // namespace

File read(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < kHeaderSize + 8) throw Error("too short for a render style");
    File f;
    f.type = d[0];
    if (f.type != kFileType) throw Error("LTB file type " + std::to_string(f.type) + " is not a D3D render style");
    f.version = uint16_t(d[2] | d[3] << 8);
    if (f.version != kVersion)
        throw Error("render style version " + std::to_string(f.version) + ": the games read only version 3");
    f.reserved1 = d[4];
    for (int i = 0; i < 3; ++i) f.reserved[std::size_t(i)] = rd32(d + 8 + 4 * i);
    f.total_size = rd32(d + 20);
    uint32_t count = rd32(d + 24);
    if (f.total_size > 1000000) throw Error("total size " + std::to_string(f.total_size) + " (the engine refuses over 1,000,000)");
    if (count > 32) throw Error(std::to_string(count) + " styles (the engine refuses over 32)");
    std::size_t p = 28;
    auto need = [&](std::size_t k, const std::string& what) {
        if (n - p < k) throw Error("ends in " + what);
    };
    for (uint32_t i = 0; i < count; ++i) {
        std::string at = "style " + std::to_string(i);
        Style s;
        need(4 + kMaterialSize + 4, at);
        s.size = rd32(d + p);
        p += 4;
        std::size_t start = p;
        const uint8_t* m = d + p;
        s.material.ambient = colour(m), s.material.diffuse = colour(m + 16);
        s.material.emissive = colour(m + 32), s.material.specular = colour(m + 48);
        s.material.specular_power = rdf(m + 64);
        p += kMaterialSize;
        uint32_t passes = rd32(d + p);
        p += 4;
        if (passes > 4) throw Error(at + " has " + std::to_string(passes) + " passes (the engine asserts at most 4)");
        for (uint32_t k = 0; k < passes; ++k) {
            std::string pk = at + "'s pass " + std::to_string(k);
            need(kPassSize + 1, pk);
            Pass ps = pass(d + p);
            p += kPassSize;
            ps.has_d3d = d[p++] != 0;
            if (ps.has_d3d) {
                need(kD3DPassSize, pk + "'s Direct3D options");
                ps.d3d = d3d_pass(d + p);
                p += kD3DPassSize;
                if (ps.d3d.use_vertex_shader)
                    for (int b = 0; b < 4; ++b) {
                        need(4, pk + "'s shader block " + std::to_string(b));
                        uint32_t len = rd32(d + p);
                        p += 4;
                        need(len, pk + "'s shader block " + std::to_string(b));
                        ps.d3d.blocks.emplace_back(d + p, d + p + len);
                        p += len;
                    }
            }
            s.passes.push_back(std::move(ps));
        }
        if (p - start != s.size)
            throw Error(at + " is " + std::to_string(p - start) + " bytes, its size says " + std::to_string(s.size));
        f.styles.push_back(std::move(s));
    }
    f.trailing = n - p;
    return f;
}

std::string texture_param_name(uint32_t v) { return name(v, {"NOTEXTURE", "USE_TEXTURE1", "USE_TEXTURE2", "USE_TEXTURE3", "USE_TEXTURE4"}); }

std::string colour_op_name(uint32_t v) {
    return name(v, {"COLOROP_DISABLE", "COLOROP_SELECTARG1", "COLOROP_SELECTARG2", "COLOROP_MODULATE", "COLOROP_MODULATE2X",
                    "COLOROP_MODULATEALPHA", "COLOROP_MODULATETFACTOR", "COLOROP_ADD", "COLOROP_DOTPRODUCT3",
                    "COLOROP_BUMPENVMAP", "COLOROP_BUMPENVMAPLUM", "COLOROP_ADDSIGNED", "COLOROP_ADDSIGNED2X",
                    "COLOROP_SUBTRACT", "COLOROP_ADDMODALPHA", "COLOROP_ADDMODINVALPHA"});
}

std::string colour_arg_name(uint32_t v) { return name(v, {"COLORARG_CURRENT", "COLORARG_DIFFUSE", "COLORARG_TEXTURE", "COLORARG_TFACTOR"}); }

std::string alpha_op_name(uint32_t v) {
    return name(v, {"ALPHAOP_DISABLE", "ALPHAOP_SELECTARG1", "ALPHAOP_SELECTARG2", "ALPHAOP_MODULATE", "ALPHAOP_MODULATEALPHA",
                    "ALPHAOP_MODULATETFACTOR", "ALPHAOP_ADD", "ALPHAOP_ADDSIGNED", "ALPHAOP_ADDSIGNED2X", "ALPHAOP_SUBTRACT"});
}

std::string alpha_arg_name(uint32_t v) { return name(v, {"ALPHAARG_CURRENT", "ALPHAARG_DIFFUSE", "ALPHAARG_TEXTURE", "ALPHAARG_TFACTOR"}); }

std::string uv_source_name(uint32_t v) {
    return name(v, {"UVFROM_MODELDATA_UVSET1", "UVFROM_MODELDATA_UVSET2", "UVFROM_MODELDATA_UVSET3", "UVFROM_MODELDATA_UVSET4",
                    "UVFROM_CAMERASPACENORMAL", "UVFROM_CAMERASPACEPOSITION", "UVFROM_CAMERASPACEREFLTVECT",
                    "UVFROM_WORLDSPACENORMAL", "UVFROM_WORLDSPACEPOSITION", "UVFROM_WORLDSPACEREFLTVECT"});
}

std::string uv_address_name(uint32_t v) { return name(v, {"UVADDR_WRAP", "UVADDR_CLAMP", "UVADDR_MIRROR", "UVADDR_MIRRORONCE"}); }

std::string filter_name(uint32_t v) {
    return name(v, {"TEXFILTER_POINT", "TEXFILTER_BILINEAR", "TEXFILTER_TRILINEAR", "TEXFILTER_ANISOTROPIC", "TEXFILTER_POINT_PTMIP"});
}

std::string blend_name(uint32_t v) {
    return name(v, {"NOBLEND", "BLEND_ADD", "BLEND_SATURATE", "BLEND_MOD_SRCALPHA", "BLEND_MOD_SRCCOLOR", "BLEND_MOD_DSTCOLOR",
                    "BLEND_MUL_SRCCOL_DSTCOL", "BLEND_MUL_SRCCOL_ONE", "BLEND_MUL_SRCALPHA_ZERO", "BLEND_MUL_SRCALPHA_ONE",
                    "BLEND_MUL_DSTCOL_ZERO"});
}

std::string z_buffer_name(uint32_t v) { return name(v, {"ZRW", "ZRO", "NOZ"}); }
std::string cull_name(uint32_t v) { return name(v, {"CULL_NONE", "CULL_CCW", "CULL_CW"}); }

std::string test_name(uint32_t v) {
    return name(v, {"NOALPHATEST", "ALPHATEST_LESS", "ALPHATEST_LESSEQUAL", "ALPHATEST_GREATER", "ALPHATEST_GREATEREQUAL",
                    "ALPHATEST_EQUAL", "ALPHATEST_NOTEQUAL"});
}

std::string fill_name(uint32_t v) { return name(v, {"WIRE", "FILL"}); }

}  // namespace lithtech::renderstyle
