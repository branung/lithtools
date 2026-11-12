// DTX reading and decoding, the C++ twin of plugins/blender's lithtech/dtx.py
// (which round-trips all 21,954 shipped textures). The integer maths are the
// engine's (FormatMgr scale tables, ConvertDXTGeneric). Versions -2 and -3
// (read_old) follow the Blood 2 and KISS Psycho Circus engines, as dtx.py's
// _read_old does.

#include "lithtech/dtx.hpp"

#include <iterator>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace lithtech::dtx {

namespace {

constexpr std::size_t kHeader = 164;
constexpr std::size_t kSectionHeader = 32;

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

std::string cstr(const uint8_t* p, std::size_t n) {
    std::size_t e = 0;
    while (e < n && p[e]) ++e;
    return std::string(reinterpret_cast<const char*>(p), e);
}

uint8_t s5(unsigned j) { return uint8_t(j * 255 / 31); }
uint8_t s6(unsigned j) { return uint8_t(j * 255 / 63); }

void rgb565(uint16_t v, int& r, int& g, int& b) {
    r = s5(v >> 11);
    g = s6((v >> 5) & 63);
    b = s5(v & 31);
}

}  // namespace

const char* bpp_name(Bpp b) {
    switch (b) {
        case BPP_8P: return "8P";
        case BPP_8: return "8";
        case BPP_16: return "16";
        case BPP_32: return "32";
        case BPP_S3TC_DXT1: return "DXT1";
        case BPP_S3TC_DXT3: return "DXT3";
        case BPP_S3TC_DXT5: return "DXT5";
        case BPP_32P: return "32P";
    }
    return "?";
}

std::size_t image_size(Bpp bpp, int w, int h) {
    std::size_t n = std::size_t(std::max(w, 0)) * std::size_t(std::max(h, 0));
    switch (bpp) {
        case BPP_S3TC_DXT1: return n >> 1;
        case BPP_S3TC_DXT3:
        case BPP_S3TC_DXT5: return n;
        case BPP_16: return n * 2;
        case BPP_32: return n * 4;
        default: return n;
    }
}

float Texture::detail_scale() const {
    float f;
    std::memcpy(&f, extra + 6, 4);
    return f + 1.0f;
}

int Texture::detail_angle() const { return int16_t(extra[10] | extra[11] << 8); }

const Section* Texture::find_section(const std::string& t) const {
    for (auto& s : sections)
        if (s.type == t) return &s;
    return nullptr;
}

float Texture::old_detail_scale() const {
    float f;
    std::memcpy(&f, extra + 4, 4);
    return f;
}

std::size_t old_mask_size(int w, int h) {
    std::size_t n = std::size_t(std::max(w, 0)) * std::size_t(std::max(h, 0));
    return n > 1 ? n >> 1 : n;
}

namespace {

// Sections after the pixels (DTX_SECTIONSFIXED), the same in every version:
// a 32-byte header (type, name, data length at 28), then the data.
void read_sections(const std::vector<uint8_t>& d, std::size_t pos, Texture& t) {
    if (t.iflags & DTX_SECTIONSFIXED) {
        for (int i = 0; i < t.section_count; ++i) {
            if (pos + kSectionHeader > d.size()) throw Error("section header past the end of the file");
            Section s;
            s.type = cstr(&d[pos], 15);
            s.name = cstr(&d[pos + 15], 10);
            uint32_t n = rd32(&d[pos + 28]);
            pos += kSectionHeader;
            if (pos + n > d.size()) throw Error("section data past the end of the file");
            s.data.assign(d.begin() + std::ptrdiff_t(pos), d.begin() + std::ptrdiff_t(pos + n));
            pos += n;
            t.sections.push_back(std::move(s));
        }
    }
}

// Versions -2 and -3: the LithTech 1.0 and 1.5 dtx_Create (Blood 2
// Client.exe 0x420d80, KISS client.exe 0x1cc8b40). The header (36 bytes;
// 164 in -3, which adds -5's 128-byte command string), an 8-byte palette
// id and 256 four-byte palette entries, every level's 8-bit pixels in one
// block, then (DTX_OLD_ALPHAMASKS) every level's 4-bit mask in one block.
Texture read_old(const std::vector<uint8_t>& d, Texture t, bool lenient) {
    const std::size_t header = t.version == -3 ? kHeader : 36;
    if (d.size() < header + 8 + 1024) throw Error("too short for a DTX header");
    t.width = rd16(&d[8]);
    t.height = rd16(&d[10]);
    t.mip_count = rd16(&d[12]);
    t.section_count = rd16(&d[14]);
    t.iflags = rd32(&d[16]);
    t.user_flags = rd32(&d[20]);
    std::memcpy(t.extra, &d[24], 12);
    if (t.version == -3) {
        t.command = cstr(&d[36], 128);
        for (char c : t.command)
            if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7e) { t.command.clear(); break; }
    }
    if (t.mip_count == 0 || t.mip_count > 15) throw Error("bad mip count " + std::to_string(t.mip_count));
    t.palette_id[0] = rd32(&d[header]);
    t.palette_id[1] = rd32(&d[header + 4]);
    t.palette.assign(d.begin() + std::ptrdiff_t(header + 8), d.begin() + std::ptrdiff_t(header + 8 + 1024));
    std::size_t pos = header + 8 + 1024;
    auto take = [&](std::size_t n) {
        if (pos + n > d.size()) {
            if (!lenient) throw Error("pixel data runs past the end of the file");
            t.truncated = true;
            n = d.size() > pos ? d.size() - pos : 0;
        }
        std::vector<uint8_t> v(d.begin() + std::ptrdiff_t(pos), d.begin() + std::ptrdiff_t(pos + n));
        pos += n;
        return v;
    };
    for (int i = 0; i < t.mip_count; ++i)
        t.mips.push_back(take(std::size_t(std::max(t.width >> i, 0)) * std::size_t(std::max(t.height >> i, 0))));
    if (t.iflags & DTX_OLD_ALPHAMASKS)
        for (int i = 0; i < t.mip_count; ++i) t.masks.push_back(take(old_mask_size(t.width >> i, t.height >> i)));
    if (t.truncated) return t;
    read_sections(d, pos, t);
    return t;
}

}  // namespace

Texture read(const std::vector<uint8_t>& d, bool lenient) {
    if (d.empty()) throw Error("empty file (0 bytes), not a texture");
    if (d.size() < 8) throw Error("too short for a DTX header");
    Texture t;
    t.res_type = rd32(&d[0]);
    t.version = int32_t(rd32(&d[4]));
    if (t.res_type != 0) throw Error("not a DTX (resource type " + std::to_string(t.res_type) + ")");
    if (t.version == -2 || t.version == -3) return read_old(d, std::move(t), lenient);
    if (t.version != -5) throw Error("unsupported DTX version " + std::to_string(t.version));
    if (d.size() < kHeader) throw Error("too short for a DTX header");
    t.width = rd16(&d[8]);
    t.height = rd16(&d[10]);
    t.mip_count = rd16(&d[12]);
    t.section_count = rd16(&d[14]);
    t.iflags = rd32(&d[16]);
    t.user_flags = rd32(&d[20]);
    std::memcpy(t.extra, &d[24], 12);
    t.command = cstr(&d[36], 128);
    if (t.mip_count == 0 || t.mip_count > 15) throw Error("bad mip count " + std::to_string(t.mip_count));
    std::size_t pos = kHeader;
    int w = t.width, h = t.height;
    for (int i = 0; i < t.mip_count; ++i, w >>= 1, h >>= 1) {
        std::size_t n = image_size(t.bpp(), w, h);
        if (pos + n > d.size()) {
            if (!lenient) throw Error("pixel data runs past the end of the file");
            t.truncated = true;
            n = d.size() > pos ? d.size() - pos : 0;
        }
        t.mips.emplace_back(d.begin() + std::ptrdiff_t(pos), d.begin() + std::ptrdiff_t(pos + n));
        pos += n;
    }
    if (t.truncated) return t;
    read_sections(d, pos, t);
    return t;
}

Texture read_file(const std::string& path, bool lenient) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("can't open " + path);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return read(d, lenient);
}

void ps2_palette_alpha(uint8_t* pal) {
    bool any = false;
    for (int i = 0; i < 256; ++i) any |= pal[4 * i + 3] != 0;
    for (int i = 0; i < 256; ++i) {
        uint8_t& a = pal[4 * i + 3];
        a = !any || a >= 0x7f ? 255 : uint8_t((a * 255 + 63) / 127);
    }
}

bool Texture::ps2_style_palette() const {
    const Section* p = find_section("PALLETE32");
    if (bpp() != BPP_32P || !p) return false;
    for (std::size_t i = 3; i < p->data.size() && i < 1024; i += 4)
        if (p->data[i] > 0x80) return false;
    return true;
}

Image decode_pixels(Bpp bpp, int width, int height, const uint8_t* data, std::size_t size,
                    const uint8_t* pal, uint32_t iflags, PaletteAlpha alpha) {
    uint8_t ps2pal[1024];
    if (pal && bpp == BPP_32P && alpha == PaletteAlpha::Ps2) {
        std::memcpy(ps2pal, pal, 1024);
        ps2_palette_alpha(ps2pal);
        pal = ps2pal;
    }
    Image img;
    img.width = std::max(width, 0);
    img.height = std::max(height, 0);
    std::size_t n = std::size_t(img.width) * img.height;
    img.rgba.assign(n * 4, 0);
    if (!n) return img;
    auto byte = [&](std::size_t i) -> uint8_t { return i < size ? data[i] : 0; };
    uint8_t* o = img.rgba.data();

    switch (bpp) {
        case BPP_32:
            for (std::size_t i = 0; i < n; ++i) {
                o[4 * i + 0] = byte(4 * i + 2);
                o[4 * i + 1] = byte(4 * i + 1);
                o[4 * i + 2] = byte(4 * i + 0);
                o[4 * i + 3] = byte(4 * i + 3);
            }
            return img;
        case BPP_32P:
        case BPP_8:
        case BPP_8P:
            if (!pal) throw Error("palettized texture without a PALLETE32 section");
            for (std::size_t i = 0; i < n; ++i) {
                const uint8_t* p = pal + 4 * byte(i);
                o[4 * i + 0] = p[2];
                o[4 * i + 1] = p[1];
                o[4 * i + 2] = p[0];
                o[4 * i + 3] = p[3];
            }
            return img;
        case BPP_16:
            for (std::size_t i = 0; i < n; ++i) {
                uint16_t v = uint16_t(byte(2 * i) | byte(2 * i + 1) << 8);
                int r, g, b, a = 255;
                if (iflags & DTX_PREFER4444) {
                    r = ((v >> 8) & 15) * 17; g = ((v >> 4) & 15) * 17; b = (v & 15) * 17; a = (v >> 12) * 17;
                } else if (iflags & DTX_PREFER5551) {
                    r = s5((v >> 10) & 31); g = s5((v >> 5) & 31); b = s5(v & 31); a = (v & 0x8000) ? 255 : 0;
                } else {
                    rgb565(v, r, g, b);
                }
                o[4 * i + 0] = uint8_t(r); o[4 * i + 1] = uint8_t(g); o[4 * i + 2] = uint8_t(b); o[4 * i + 3] = uint8_t(a);
            }
            return img;
        case BPP_S3TC_DXT1:
        case BPP_S3TC_DXT3:
        case BPP_S3TC_DXT5: {
            int bw = (width + 3) / 4, bh = (height + 3) / 4;
            std::size_t bb = bpp == BPP_S3TC_DXT1 ? 8 : 16;
            std::size_t pos = 0;
            for (int by = 0; by < bh; ++by)
                for (int bx = 0; bx < bw; ++bx, pos += bb) {
                    int alpha[16];
                    bool has_alpha = bpp != BPP_S3TC_DXT1;
                    std::size_t cpos = pos + (has_alpha ? 8 : 0);
                    if (bpp == BPP_S3TC_DXT3) {
                        for (int i = 0; i < 16; ++i) alpha[i] = ((byte(pos + i / 2) >> (4 * (i & 1))) & 15) * 17;
                    } else if (bpp == BPP_S3TC_DXT5) {
                        int a0 = byte(pos), a1 = byte(pos + 1), tab[8] = {a0, a1};
                        if (a0 > a1) {
                            for (int i = 1; i < 7; ++i) tab[i + 1] = ((7 - i) * a0 + i * a1) / 7;
                        } else {
                            for (int i = 1; i < 5; ++i) tab[i + 1] = ((5 - i) * a0 + i * a1) / 5;
                            tab[6] = 0;
                            tab[7] = 255;
                        }
                        uint64_t bits = 0;
                        for (int k = 0; k < 6; ++k) bits |= uint64_t(byte(pos + 2 + k)) << (8 * k);
                        for (int i = 0; i < 16; ++i) alpha[i] = tab[(bits >> (3 * i)) & 7];
                    }
                    uint16_t c0 = uint16_t(byte(cpos) | byte(cpos + 1) << 8);
                    uint16_t c1 = uint16_t(byte(cpos + 2) | byte(cpos + 3) << 8);
                    uint32_t bits = uint32_t(byte(cpos + 4)) | uint32_t(byte(cpos + 5)) << 8 |
                                    uint32_t(byte(cpos + 6)) << 16 | uint32_t(byte(cpos + 7)) << 24;
                    int r0, g0, b0, r1, g1, b1;
                    rgb565(c0, r0, g0, b0);
                    rgb565(c1, r1, g1, b1);
                    int col[4][4] = {{r0, g0, b0, 255}, {r1, g1, b1, 255}};
                    bool four = bpp != BPP_S3TC_DXT1 || c0 > c1;
                    if (four) {
                        int c2[4] = {(2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255};
                        int c3[4] = {(r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255};
                        std::memcpy(col[2], c2, sizeof c2);
                        std::memcpy(col[3], c3, sizeof c3);
                    } else {
                        int c2[4] = {(r0 + r1) >> 1, (g0 + g1) >> 1, (b0 + b1) >> 1, 255};
                        int c3[4] = {0, 0, 0, 0};
                        std::memcpy(col[2], c2, sizeof c2);
                        std::memcpy(col[3], c3, sizeof c3);
                    }
                    for (int py = 0; py < 4; ++py) {
                        int y = by * 4 + py;
                        if (y >= height) break;
                        for (int px = 0; px < 4; ++px) {
                            int x = bx * 4 + px;
                            if (x >= width) break;
                            int i = py * 4 + px;
                            const int* c = col[(bits >> (2 * i)) & 3];
                            uint8_t* q = o + (std::size_t(y) * width + x) * 4;
                            q[0] = uint8_t(c[0]); q[1] = uint8_t(c[1]); q[2] = uint8_t(c[2]);
                            q[3] = uint8_t(has_alpha ? alpha[i] : c[3]);
                        }
                    }
                }
            return img;
        }
    }
    throw Error("unknown pixel format");
}

Image decode_old_pixels(int width, int height, const uint8_t* data, std::size_t size, const uint8_t* pal,
                        const uint8_t* mask, std::size_t mask_size) {
    // The engine's D3D upload (Blood 2 d3d.ren 0x1001db60, KISS d3d.ren
    // 0x100242b0) maps each index through a 16-bit table made from palette
    // bytes 1-3 (0x1001d718: R, G, B; byte 0 is unused), ORs the format's
    // alpha bits into entries 246-255, and with masks ORs each row's w/2
    // bytes in as 4444 alpha: low nibble for the even pixel, high for the
    // odd one. A 1-pixel-wide level gets no mask bits.
    Image img;
    img.width = std::max(width, 0);
    img.height = std::max(height, 0);
    std::size_t n = std::size_t(img.width) * img.height;
    img.rgba.assign(n * 4, 0);
    if (!n) return img;
    if (!pal) throw Error("palettized texture without a palette");
    const std::size_t stride = std::size_t(img.width) >> 1;
    uint8_t* o = img.rgba.data();
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x, o += 4) {
            std::size_t i = std::size_t(y) * img.width + x;
            uint8_t idx = i < size ? data[i] : 0;
            const uint8_t* p = pal + 4 * idx;
            o[0] = p[1];
            o[1] = p[2];
            o[2] = p[3];
            if (!mask || idx >= kOldOpaqueFirst) {
                o[3] = 255;
            } else if (std::size_t(x) < stride * 2) {
                std::size_t m = std::size_t(y) * stride + std::size_t(x >> 1);
                o[3] = uint8_t((m < mask_size ? (mask[m] >> (4 * (x & 1))) & 15 : 0) * 17);
            } else {
                o[3] = 0;
            }
        }
    return img;
}

Image Texture::decode(int mip, int face, PaletteAlpha alpha) const {
    if (mip < 0 || mip >= int(mips.size())) throw Error("no such mip level");
    int w = std::max(width >> mip, 0), h = std::max(height >> mip, 0);
    if (is_old()) {
        if (face > 0) throw Error("not a cube map");
        std::vector<uint8_t> pal = palette;
        pal.resize(1024, 0);
        // A mask missing or cut short by the end of the file reads as zero
        // (transparent), but the texture stays masked: mask is non-null.
        static const uint8_t kNoMask = 0;
        const uint8_t* m = nullptr;
        std::size_t mn = 0;
        if (has_alpha_masks()) {
            m = &kNoMask;
            if (std::size_t(mip) < masks.size() && !masks[std::size_t(mip)].empty()) {
                m = masks[std::size_t(mip)].data();
                mn = masks[std::size_t(mip)].size();
            }
        }
        const auto& px = mips[std::size_t(mip)];
        return decode_old_pixels(w, h, px.data(), px.size(), pal.data(), m, mn);
    }
    const std::vector<uint8_t>* level = &mips[std::size_t(mip)];
    std::vector<uint8_t> face_data;
    if (face > 0) {
        if (!is_cubemap()) throw Error("not a cube map");
        const Section* s = find_section("CUBEMAPDATA");
        if (!s) throw Error("cube map without CUBEMAPDATA");
        std::size_t per_face = 0, before = 0;
        for (int i = 0; i < mip_count; ++i) {
            std::size_t n = image_size(bpp(), width >> i, height >> i);
            if (i < mip) before += n;
            per_face += n;
        }
        std::size_t start = std::size_t(face - 1) * per_face + before;
        std::size_t n = image_size(bpp(), w, h);
        if (start + n > s->data.size()) throw Error("cube face past the end of its section");
        face_data.assign(s->data.begin() + std::ptrdiff_t(start), s->data.begin() + std::ptrdiff_t(start + n));
        level = &face_data;
    }
    const Section* p = find_section("PALLETE32");
    const uint8_t* pal = nullptr;
    std::vector<uint8_t> palbuf;
    if (p) {
        palbuf = p->data;
        palbuf.resize(1024, 0);
        pal = palbuf.data();
    }
    return decode_pixels(bpp(), w, h, level->data(), level->size(), pal, iflags, alpha);
}

}  // namespace lithtech::dtx
