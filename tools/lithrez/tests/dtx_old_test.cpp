// lithtech::dtx versions -2 (LithTech 1.0, Blood 2) and -3 (LithTech 1.5,
// KISS Psycho Circus) on files made here, one per layout, decoded level by
// level and checked pixel by pixel: -2 plain, -2 with alpha masks (an odd
// 1x1 last level, palette entries 246-255 opaque under a clear mask), -3
// with a command string and a detail scale/angle, -3 masked with a garbage
// command and a section after the masks; then a truncated file (lenient
// and strict) and files that must be refused.
//   dtx_old_test
#include <lithtech/dtx.hpp>

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

static void put16(std::vector<uint8_t>& f, std::size_t o, unsigned v) { f[o] = uint8_t(v), f[o + 1] = uint8_t(v >> 8); }
static void put32(std::vector<uint8_t>& f, std::size_t o, uint32_t v) {
    for (int i = 0; i < 4; ++i) f[o + i] = uint8_t(v >> (8 * i));
}

// Palette entry i: {unused, R, G, B} = {0xff, i, 255 - i, i ^ 0x5a}.
static uint8_t R(int i) { return uint8_t(i); }
static uint8_t G(int i) { return uint8_t(255 - i); }
static uint8_t B(int i) { return uint8_t(i ^ 0x5a); }

struct Section { std::string type, name; std::vector<uint8_t> data; };

// A -2/-3 file: header (36 bytes, or 164 with the command string for -3),
// palette id, palette, every level's pixels, then every level's masks,
// then sections.
static std::vector<uint8_t> make(int version, int w, int h, int mips, uint32_t flags,
                                 const std::vector<std::vector<uint8_t>>& pixels,
                                 const std::vector<std::vector<uint8_t>>& masks = {},
                                 const std::vector<uint8_t>& extra = {}, const std::vector<uint8_t>& command = {},
                                 const std::vector<Section>& sections = {}) {
    std::size_t header = version == -3 ? 164 : 36;
    std::vector<uint8_t> f(header + 8 + 1024, 0);
    put32(f, 0, 0);
    put32(f, 4, uint32_t(version));
    put16(f, 8, unsigned(w)), put16(f, 10, unsigned(h)), put16(f, 12, unsigned(mips));
    put16(f, 14, unsigned(sections.size()));
    put32(f, 16, flags);
    put32(f, 20, 0x1e);
    for (std::size_t i = 0; i < extra.size() && i < 12; ++i) f[24 + i] = extra[i];
    for (std::size_t i = 0; i < command.size() && i < 128; ++i) f[36 + i] = command[i];
    put32(f, header, 0x00123456);
    put32(f, header + 4, 0x89abcdef);
    for (int i = 0; i < 256; ++i) {
        uint8_t* p = &f[header + 8 + 4 * std::size_t(i)];
        p[0] = 0xff, p[1] = R(i), p[2] = G(i), p[3] = B(i);
    }
    for (auto& p : pixels) f.insert(f.end(), p.begin(), p.end());
    for (auto& m : masks) f.insert(f.end(), m.begin(), m.end());
    for (auto& s : sections) {
        std::vector<uint8_t> sh(32, 0);
        std::memcpy(sh.data(), s.type.c_str(), s.type.size());
        std::memcpy(sh.data() + 15, s.name.c_str(), s.name.size());
        put32(sh, 28, uint32_t(s.data.size()));
        f.insert(f.end(), sh.begin(), sh.end());
        f.insert(f.end(), s.data.begin(), s.data.end());
    }
    return f;
}

// Level `mip` of t against the indices and the expected alpha per pixel.
static void expect(const dtx::Texture& t, int mip, const std::vector<uint8_t>& idx, const std::vector<int>& alpha) {
    auto img = t.decode(mip);
    CHECK(img.width == std::max(t.width >> mip, 0));
    CHECK(img.height == std::max(t.height >> mip, 0));
    CHECK(img.rgba.size() == idx.size() * 4);
    for (std::size_t i = 0; i < idx.size() && 4 * i + 3 < img.rgba.size(); ++i) {
        const uint8_t* p = &img.rgba[4 * i];
        if (p[0] != R(idx[i]) || p[1] != G(idx[i]) || p[2] != B(idx[i]) || p[3] != alpha[i]) {
            std::printf("FAIL level %d pixel %zu: got %d,%d,%d,%d want %d,%d,%d,%d\n", mip, i, p[0], p[1], p[2], p[3],
                        R(idx[i]), G(idx[i]), B(idx[i]), alpha[i]);
            ++failures;
        }
    }
}

static bool throws(const std::vector<uint8_t>& f, bool lenient = true) {
    try {
        dtx::read(f, lenient);
    } catch (const dtx::Error&) {
        return true;
    }
    return false;
}

int main() {
    using V = std::vector<uint8_t>;
    using A = std::vector<int>;

    CHECK(dtx::old_mask_size(4, 4) == 8);
    CHECK(dtx::old_mask_size(2, 1) == 1);
    CHECK(dtx::old_mask_size(1, 1) == 1);
    CHECK(dtx::old_mask_size(0, 1) == 0);

    // -2, no masks: 4x2 and 2x1, every pixel opaque whatever its index
    // (the 246-255 rule only matters with masks). Flags 0x408: bit 10 is
    // CUBEMAP in -5, nothing here.
    {
        V l0 = {0, 1, 2, 3, 250, 128, 255, 7}, l1 = {9, 246};
        auto f = make(-2, 4, 2, 2, 0x408, {l0, l1}, {}, {5, 4, 0x88});
        auto t = dtx::read(f, false);
        CHECK(t.version == -2 && t.is_old());
        CHECK(t.width == 4 && t.height == 2 && t.mip_count == 2);
        CHECK(t.bpp() == dtx::BPP_8P);
        CHECK(!t.has_alpha_masks() && t.masks.empty());
        CHECK(!t.is_cubemap());
        CHECK(t.user_flags == 0x1e);
        CHECK(t.extra[0] == 5 && t.extra[1] == 4 && t.extra[2] == 0x88);
        CHECK(t.command.empty());
        CHECK(t.palette_id[0] == 0x00123456 && t.palette_id[1] == 0x89abcdef);
        CHECK(t.palette.size() == 1024 && t.palette[4 * 7 + 1] == 7);
        CHECK(t.sections.empty() && !t.truncated);
        expect(t, 0, l0, A(8, 255));
        expect(t, 1, l1, A(2, 255));
        CHECK(throws(f) == false);
        try {
            t.decode(0, 1);
            CHECK(!"a face of an old texture decoded");
        } catch (const dtx::Error&) {
        }
    }

    // -2 with masks: 4x4, 2x2, 1x1. Masks follow all the pixels: 8, 2 and
    // 1 bytes, rows of w/2 bytes, low nibble for the even pixel. Index 250
    // under nibble 0 stays opaque; the 1x1 level has no mask bits (w/2 = 0)
    // so it is clear unless its index is 246 or more.
    {
        V l0 = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 250, 21, 22, 23, 24, 255};
        V m0 = {0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0x0e};   // nibbles 0..14, then 0 under 255
        V l1 = {30, 31, 32, 33}, m1 = {0xf0, 0x5a};
        V l2 = {40}, m2 = {0xff};
        auto f = make(-2, 4, 4, 3, 0x0a, {l0, l1, l2}, {m0, m1, m2});
        CHECK(f.size() == 36 + 8 + 1024 + 21 + 11);
        auto t = dtx::read(f, false);
        CHECK(t.has_alpha_masks() && t.masks.size() == 3);
        CHECK(t.masks[0] == m0 && t.masks[1] == m1 && t.masks[2] == m2);
        A a0;
        for (int i = 0; i < 15; ++i) a0.push_back(i * 17);
        a0.push_back(255);   // index 255 under nibble 0
        a0[10] = 255;        // index 250 under nibble 10
        expect(t, 0, l0, a0);
        expect(t, 1, l1, {0, 255, 10 * 17, 5 * 17});
        expect(t, 2, l2, {0});
        auto g = make(-2, 1, 1, 1, 0x0a, {{247}}, {{0x00}});
        expect(dtx::read(g, false), 0, {247}, {255});
    }

    // -3: the 164-byte header with a command string, detail scale 6.0 at
    // byte 28 and angle 90 at byte 32; FULLBRITE | SECTIONSFIXED | bit 5.
    {
        V l0 = {1, 2, 3, 4, 5, 6, 7, 8}, l1 = {200, 201};
        float six = 6.0f;
        V extra(12, 0);
        std::memcpy(&extra[4], &six, 4);
        extra[8] = 90;
        std::string cmd = "DetailTex Tex\\Dtl.dtx";
        auto f = make(-3, 2, 4, 2, 0x29, {l0, l1}, {}, extra, V(cmd.begin(), cmd.end()));
        CHECK(f.size() == 164 + 8 + 1024 + 10);
        auto t = dtx::read(f, false);
        CHECK(t.version == -3 && t.width == 2 && t.height == 4);
        CHECK(t.command == cmd);
        CHECK(t.old_detail_scale() == 6.0f);
        CHECK(t.old_detail_angle() == 90);
        CHECK(t.iflags & dtx::DTX_OLD_NOPALETTED);
        CHECK(!t.has_alpha_masks());
        expect(t, 0, l0, A(8, 255));
        expect(t, 1, l1, A(2, 255));
    }

    // -3 with masks, leftover memory for a command string (not text, so no
    // command), and one section after the masks.
    {
        V l0 = {100, 101, 102, 103, 104, 105, 106, 107}, m0 = {0xf0, 0x0f, 0x88, 0x21};
        V l1 = {108, 109}, m1 = {0x7f};
        V junk = {'s', 'i', 'b', 'l', 'e', 0x05, 0x92, 0};
        auto f = make(-3, 4, 2, 2, 0x0a, {l0, l1}, {m0, m1}, {}, junk, {{"LIGHTDEFS", "x", {'a', 'b', 0}}});
        auto t = dtx::read(f, false);
        CHECK(t.command.empty());
        CHECK(t.sections.size() == 1);
        if (t.sections.size() == 1) CHECK(t.sections[0].type == "LIGHTDEFS" && t.sections[0].data.size() == 3);
        expect(t, 0, l0, {0, 255, 255, 0, 8 * 17, 8 * 17, 17, 2 * 17});
        expect(t, 1, l1, {15 * 17, 7 * 17});
    }

    // Truncated inside the masks: lenient keeps it, and the missing mask
    // bytes read as clear; strict refuses it.
    {
        V l0 = {1, 2, 3, 4}, m0 = {0xff, 0xff};
        auto f = make(-2, 2, 2, 1, 0x0a, {l0}, {m0});
        f.pop_back();
        CHECK(throws(f, false));
        auto t = dtx::read(f, true);
        CHECK(t.truncated);
        expect(t, 0, l0, {255, 255, 0, 0});
        f.resize(f.size() - 1);   // no mask at all
        t = dtx::read(f, true);
        CHECK(t.truncated && t.has_alpha_masks());
        expect(t, 0, l0, {0, 0, 0, 0});
    }

    // Refused: version -4, a short palette, mip count 0 or 16.
    {
        auto f = make(-2, 2, 2, 1, 0, {{1, 2, 3, 4}});
        put32(f, 4, uint32_t(-4));
        CHECK(throws(f));
        f = make(-2, 2, 2, 1, 0, {{1, 2, 3, 4}});
        CHECK(!throws(f));
        CHECK(throws(V(f.begin(), f.begin() + 36 + 8 + 1000)));
        f = make(-3, 2, 2, 0, 0, {});
        CHECK(throws(f));
        f = make(-3, 2, 2, 16, 0, {});
        CHECK(throws(f));
    }

    // An empty file (Tron 2.0 ships three) says so.
    try {
        dtx::read(V{});
        CHECK(false);
    } catch (const dtx::Error& e) {
        CHECK(std::string(e.what()).find("empty file") == 0);
    }

    std::printf(failures ? "%d failures\n" : "dtx_old_test: all passed\n", failures);
    return failures ? 1 : 0;
}
