// lithtech::spr on sprites made here: the header and frame names, the
// engine's animation length and frame choice, trailing bytes, and files
// that must be refused (short header, a frame count past the end, a name
// over spr_Create's 1000-byte limit, a name cut short).
//   spr_test
#include <lithtech/spr.hpp>

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

static std::vector<uint8_t> make(uint32_t rate, const std::vector<std::string>& frames, uint32_t keyed = 0,
                                 uint32_t translucent = 1, uint32_t key = 0x00ff00ff) {
    std::vector<uint8_t> f;
    put32(f, uint32_t(frames.size()));
    put32(f, rate);
    put32(f, keyed);
    put32(f, translucent);
    put32(f, key);
    for (auto& s : frames) {
        f.push_back(uint8_t(s.size()));
        f.push_back(uint8_t(s.size() >> 8));
        f.insert(f.end(), s.begin(), s.end());
    }
    return f;
}

static bool refused(const std::vector<uint8_t>& f) {
    try {
        spr::read(f);
    } catch (const spr::Error&) {
        return true;
    }
    return false;
}

int main() {
    {
        auto f = make(10, {"SFX\\A.DTX", "SFX\\B.DTX", "SFX\\C.DTX"}, 1, 0, 0x123456);
        auto s = spr::read(f);
        CHECK(s.frames.size() == 3);
        CHECK(s.frames[1] == "SFX\\B.DTX");
        CHECK(s.frame_rate == 10 && s.keyed == 1 && s.translucent == 0 && s.colour_key == 0x123456);
        CHECK(s.trailing == 0);
        CHECK(s.length_ms() == 300);
        CHECK(s.frame_at(0) == 0 && s.frame_at(99) == 0 && s.frame_at(100) == 1 && s.frame_at(299) == 2);
        CHECK(s.frame_at(300) == 0 && s.frame_at(1250) == 0 && s.frame_at(1350) == 1);   // loops
    }
    {
        // 1000 / 30 = 33 ms a frame in integers: 4 frames last 132 ms
        auto s = spr::read(make(30, {"a", "b", "c", "d"}));
        CHECK(s.length_ms() == 132);
        CHECK(s.frame_at(33) == 1 && s.frame_at(131) == 3);
    }
    {
        // no frames, a zero rate, a rate over 1000: no animation, no frame
        CHECK(spr::read(make(10, {})).frame_at(0) == -1);
        CHECK(spr::read(make(0, {"a"})).frame_at(0) == -1);
        CHECK(spr::read(make(2000, {"a"})).length_ms() == 0);
    }
    {
        auto f = make(10, {"a"});
        f.push_back(0);
        f.push_back(0);
        CHECK(spr::read(f).trailing == 2);
        // an empty name is kept
        auto e = spr::read(make(10, {"", "x"}));
        CHECK(e.frames.size() == 2 && e.frames[0].empty());
    }
    {
        auto f = make(10, {"a"});
        CHECK(refused({}));
        CHECK(refused(std::vector<uint8_t>(f.begin(), f.begin() + 19)));
        CHECK(refused(std::vector<uint8_t>(f.begin(), f.end() - 1)));
        auto many = f;
        many[0] = 0xff, many[1] = 0xff;
        CHECK(refused(many));
        CHECK(refused(make(10, {std::string(1001, 'x')})));
        CHECK(!refused(make(10, {std::string(1000, 'x')})));
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::printf("spr_test: all passed\n");
    return failures ? 1 : 0;
}
