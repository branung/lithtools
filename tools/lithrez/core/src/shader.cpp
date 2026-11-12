// Compiled shaders: see shader.hpp. A Direct3D 8 token stream starts with
// a version token (0xFFFE0000 | major << 8 | minor for a vertex shader,
// 0xFFFF0000 | ... for a pixel shader) and ends with 0x0000FFFF; a comment
// token is 0xFFFE with its length in tokens in bits 16-30 (D3DX8's
// assembler puts one with its name after the version).

#include "lithtech/shader.hpp"

#include <cstring>

namespace lithtech::shader {

namespace {

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

}  // namespace

PixelShader read_pixel_shader(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < 12) throw Error("too short for a pixel shader");
    if (rd32(d) != kPixelShaderId) throw Error("not a pixel shader (no \"LTPS\" id)");
    PixelShader s;
    s.version = rd32(d + 4);
    if (s.version != 1) throw Error("version " + std::to_string(s.version) + " (the engine takes only 1)");
    uint32_t len = rd32(d + 8);
    if (len > n - 12) throw Error("the code's size, " + std::to_string(len) + ", runs past the end");
    s.code.assign(d + 12, d + 12 + len);
    s.trailing = n - 12 - len;
    return s;
}

AssembledVertexShader read_assembled_vertex_shader(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < 5) throw Error("too short for an assembled vertex shader");
    AssembledVertexShader s;
    s.version = rd32(d);
    if (s.version != 1) throw Error("version " + std::to_string(s.version) + " (RenderStylesEditor writes 1)");
    s.expand_for_skinning = d[4] != 0;
    std::size_t p = 5;
    int count = s.expand_for_skinning ? 4 : 1;
    for (int i = 0; i < count; ++i) {
        if (n - p < 4) throw Error("ends in shader " + std::to_string(i));
        uint32_t len = rd32(d + p);
        p += 4;
        if (len > n - p) throw Error("shader " + std::to_string(i) + "'s size runs past the end");
        s.code.emplace_back(d + p, d + p + len);
        p += len;
    }
    s.trailing = n - p;
    return s;
}

std::string describe(const std::vector<uint8_t>& c) {
    if (c.size() < 4) return "not shader code";
    uint32_t v = rd32(c.data());
    std::string out;
    if ((v & 0xffff0000u) == 0xfffe0000u) out = "vs.";
    else if ((v & 0xffff0000u) == 0xffff0000u) out = "ps.";
    else return "not shader code";
    out += std::to_string((v >> 8) & 0xff) + "." + std::to_string(v & 0xff);
    out += ", " + std::to_string(c.size() / 4) + " tokens";
    // the comments right after the version token (D3DX puts its own there)
    for (std::size_t i = 4; i + 4 <= c.size();) {
        uint32_t t = rd32(c.data() + i);
        if ((t & 0xffff) != 0xfffe) break;
        i += 4;
        std::size_t len = 4 * std::size_t((t >> 16) & 0x7fff);
        if (len > c.size() - i) break;
        std::string text(reinterpret_cast<const char*>(c.data() + i), len);
        text = text.substr(0, text.find('\0'));
        if (!text.empty()) out += ", \"" + text + "\"";
        i += len;
    }
    return out;
}

}  // namespace lithtech::shader
