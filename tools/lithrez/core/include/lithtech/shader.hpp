// Compiled shaders the Jupiter games ship: pixel shaders (.ps; NOLF2,
// Contract J.A.C.K., Tron 2.0), read by the renderer's
// CPixelShaderMgr::LoadPixelShader (nolf2src's pixelshadermgr.cpp), and
// assembled vertex shaders (.ash; Tron 2.0), written by
// RenderStylesEditor's "Save Assembled" (Build 69's
// tools/RenderStylesEditor/VertexShaderDlg.h). Both hold Direct3D 8 shader
// tokens; see docs/RENDERSTYLE.md.

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::shader {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr uint32_t kPixelShaderId = 0x5350544c;   // PIXELSHADER_FILE_ID: the file starts "LTPS"

struct PixelShader {
    uint32_t version = 1;
    std::vector<uint8_t> code;
    std::size_t trailing = 0;
};

// u32 'LTPS', u32 version (1; the engine refuses others), u32 size, the
// shader tokens.
PixelShader read_pixel_shader(const uint8_t* data, std::size_t size);

struct AssembledVertexShader {
    uint32_t version = 1;
    bool expand_for_skinning = false;
    // one shader for rigid meshes, or with expand_for_skinning one per
    // VERTSHADER_TYPES_COUNT (4): rigid, then skinned
    std::vector<std::vector<uint8_t>> code;
    std::size_t trailing = 0;
};

// u32 version (1), u8 bExpandForSkinning, then per shader a u32 size and
// the tokens (D3DXAssembleShader's output).
AssembledVertexShader read_assembled_vertex_shader(const uint8_t* data, std::size_t size);

// What a Direct3D 8 token stream is: its version ("vs.1.1", "ps.1.1"),
// its size in tokens and the text of its comment tokens (the assembler's
// name); "not shader code" if it doesn't start with a version token.
std::string describe(const std::vector<uint8_t>& code);

}  // namespace lithtech::shader
