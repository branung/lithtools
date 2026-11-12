// lithtech::shader on files made here: an LTPS pixel shader, assembled
// vertex shaders rigid and expanded for skinning, describe() on their
// tokens, and files that must be refused.
//   shader_test
#include <lithtech/shader.hpp>

#include <cstdio>
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

// version token, a two-token comment "D3DX8\0\0\0", a nop, the end token
static std::vector<uint8_t> tokens(uint32_t version) {
    std::vector<uint8_t> c;
    put32(c, version);
    put32(c, 0x0002fffe);
    for (char ch : std::string("D3DX8\0\0\0", 8)) c.push_back(uint8_t(ch));
    put32(c, 0);
    put32(c, 0x0000ffff);
    return c;
}

template <class F>
static bool refused(F f) {
    try {
        f();
    } catch (const shader::Error&) {
        return true;
    }
    return false;
}

int main() {
    {
        auto code = tokens(0xffff0101);
        std::vector<uint8_t> f = {'L', 'T', 'P', 'S'};
        put32(f, 1);
        put32(f, uint32_t(code.size()));
        f.insert(f.end(), code.begin(), code.end());
        auto s = shader::read_pixel_shader(f.data(), f.size());
        CHECK(s.version == 1 && s.code == code && s.trailing == 0);
        CHECK(shader::describe(s.code) == "ps.1.1, 6 tokens, \"D3DX8\"");
        auto bad = f;
        bad[4] = 2;
        CHECK(refused([&] { shader::read_pixel_shader(bad.data(), bad.size()); }));
        bad = f;
        bad[0] = 'X';
        CHECK(refused([&] { shader::read_pixel_shader(bad.data(), bad.size()); }));
        CHECK(refused([&] { shader::read_pixel_shader(f.data(), f.size() - 1); }));
    }
    {
        auto code = tokens(0xfffe0101);
        std::vector<uint8_t> f;
        put32(f, 1);
        f.push_back(0);
        put32(f, uint32_t(code.size()));
        f.insert(f.end(), code.begin(), code.end());
        auto s = shader::read_assembled_vertex_shader(f.data(), f.size());
        CHECK(!s.expand_for_skinning && s.code.size() == 1 && s.trailing == 0);
        CHECK(shader::describe(s.code[0]).rfind("vs.1.1", 0) == 0);

        std::vector<uint8_t> g;
        put32(g, 1);
        g.push_back(1);
        for (int i = 0; i < 4; ++i) {
            put32(g, uint32_t(code.size()));
            g.insert(g.end(), code.begin(), code.end());
        }
        auto k = shader::read_assembled_vertex_shader(g.data(), g.size());
        CHECK(k.expand_for_skinning && k.code.size() == 4);
        CHECK(refused([&] { shader::read_assembled_vertex_shader(g.data(), g.size() - 1); }));
        CHECK(shader::describe({1, 2, 3, 4}) == "not shader code");
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::printf("shader_test: all passed\n");
    return failures ? 1 : 0;
}
