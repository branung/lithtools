// Render styles (LTB file type 5; RS/*.ltb in NOLF2, Contract J.A.C.K. and
// Tron 2.0): how a model piece is drawn, as up to four render passes of
// texture stages and states, with a lighting material. Written by
// RenderStyle_Packer from RenderStylesEditor's .lta and read by the
// renderer's CD3DRenderStyle::Load_LTBData (nolf2src's
// runtime/render_a/src/sys/d3d/d3d_renderstyle.cpp, checked against the
// three games' Lithtech.exe), which reads the structures of
// sdk/inc/ltrenderstyle.h as MSVC lays them out; see docs/RENDERSTYLE.md.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::renderstyle {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr uint8_t kFileType = 5;      // LTB_D3D_RENDERSTYLE_FILE (6 is the PS2's)
constexpr uint16_t kVersion = 3;      // RENDERSTYLE_D3D_VERSION
constexpr std::size_t kHeaderSize = 20;     // LTB_Header
constexpr std::size_t kMaterialSize = 68;   // LightingMaterial
constexpr std::size_t kStageSize = 120;     // TextureStageOps
constexpr std::size_t kPassSize = 532;      // RenderPassOp
constexpr std::size_t kD3DPassSize = 162;   // RSD3DRenderPass as stored (no padding after its two bools)

// Whether the bytes start like a render style (an LTB header of type 5).
bool looks_like(const uint8_t* data, std::size_t size);

struct Colour {
    float r = 0, g = 0, b = 0, a = 0;
};

struct Material {   // LightingMaterial
    Colour ambient, diffuse, emissive, specular;
    float specular_power = 0;
};

struct TextureStage {   // TextureStageOps: the enums as stored
    uint32_t texture = 0;   // ERenStyle_TextureParam: 0 none, 1-4 the piece's texture 1-4
    uint32_t colour_op = 0, colour_arg1 = 0, colour_arg2 = 0;
    uint32_t alpha_op = 0, alpha_arg1 = 0, alpha_arg2 = 0;
    uint32_t uv_source = 0, u_address = 0, v_address = 0, filter = 0;
    bool uv_transform = false;
    std::array<float, 16> uv_matrix{};
    bool project_tex_coord = false;
    uint32_t tex_coord_count = 0;
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
};

// RSD3DRenderPass: a vertex shader's settings (constant registers, -1 for
// unused, and its vertex declaration). The games no longer run vertex
// shaders ("These are no longer supported"); they skip the shader blocks.
struct D3DPass {
    bool use_vertex_shader = false, expand_for_skinning = false;
    int32_t const_reg[3] = {-1, -1, -1};
    Vec4 const_param[3];
    int32_t world_view_reg = -1;
    uint32_t world_view_count = 0;
    int32_t proj_reg = -1, world_view_proj_reg = -1, view_proj_reg = -1, cam_pos_reg = -1;
    uint32_t light_count = 0;
    int32_t light_pos_model_reg = -1, light_pos_camera_reg = -1, light_colour_reg = -1, light_att_reg = -1;
    int32_t material_amb_dif_em_reg = -1, material_specular_reg = -1, ambient_light_reg = -1;
    int32_t prev_world_view_reg = -1;
    uint32_t prev_world_view_count = 0;
    int32_t last_reg = -1;
    std::array<bool, 4> stream_position{}, stream_normal{}, stream_uvsets{}, stream_basis{};
    std::array<int32_t, 4> stream_uv_count{};
    // with a vertex shader: four blocks the engine skips (each a uint32
    // size and the bytes; the shipped ones hold D3D8 vertex shader code)
    std::vector<std::vector<uint8_t>> blocks;
};

struct Pass {   // RenderPassOp, and RSD3DRenderPass if it has one
    std::array<TextureStage, 4> stages;
    uint32_t blend = 0, z_buffer = 0, cull = 0;
    uint32_t texture_factor = 0, alpha_ref = 0;   // TextureFactor as ARGB
    bool dynamic_light = false;
    uint32_t z_test = 0, alpha_test = 0, fill = 0;
    bool bump_env_map = false;
    uint32_t bump_env_map_stage = 0;
    float bump_env_map_scale = 0, bump_env_map_offset = 0;

    bool has_d3d = false;   // iHasRSD3DRP
    D3DPass d3d;
};

struct Style {
    uint32_t size = 0;   // as stored: the material, the pass count and the passes
    Material material;
    std::vector<Pass> passes;
};

struct File {
    uint8_t type = kFileType;
    uint16_t version = kVersion;
    // the header's reserved fields: whatever was on the packer's stack
    // (LTB_Header's constructor sets only the type and version)
    uint8_t reserved1 = 0;
    std::array<uint32_t, 3> reserved{};
    uint32_t total_size = 0;   // as stored: the styles' sizes added up
    std::vector<Style> styles;   // the engine takes the first one the device supports
    std::size_t trailing = 0;
};

File read(const uint8_t* data, std::size_t size);
inline File read(const std::vector<uint8_t>& data) { return read(data.data(), data.size()); }

// The games' ltrenderstyle.h's enumerators (NOLF2's and Tron 2.0's are the
// same; Build 69's adds MODULATETEXALPHA to the colour and alpha ops),
// without RENDERSTYLE_ ("COLOROP_MODULATE", "BLEND_ADD", ...); "#<n>" for
// a value past the list.
std::string texture_param_name(uint32_t v);
std::string colour_op_name(uint32_t v);
std::string colour_arg_name(uint32_t v);
std::string alpha_op_name(uint32_t v);
std::string alpha_arg_name(uint32_t v);
std::string uv_source_name(uint32_t v);
std::string uv_address_name(uint32_t v);
std::string filter_name(uint32_t v);
std::string blend_name(uint32_t v);
std::string z_buffer_name(uint32_t v);
std::string cull_name(uint32_t v);
std::string test_name(uint32_t v);
std::string fill_name(uint32_t v);

}  // namespace lithtech::renderstyle
