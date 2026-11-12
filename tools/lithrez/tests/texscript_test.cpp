// lithtech::texscript on scripts and groups made here as TSCompiler and
// DEdit write them: a script's header, parameters, constants and code;
// its disassembly and the source decompiled back; evaluation as the
// engine does it (Sin and Cos swapped, user variables written back);
// check() finding bad code; effect groups with each stage type; and files
// that must be refused.
//   texscript_test
#include <lithtech/texscript.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lithtech;
using namespace lithtech::texscript;

static int failures = 0;
#define CHECK(c)                                                             \
    do {                                                                     \
        if (!(c)) {                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void put16(std::vector<uint8_t>& f, uint32_t v) { f.push_back(uint8_t(v)), f.push_back(uint8_t(v >> 8)); }
static void put32(std::vector<uint8_t>& f, uint32_t v) {
    for (int i = 0; i < 4; ++i) f.push_back(uint8_t(v >> (8 * i)));
}
static void putf(std::vector<uint8_t>& f, float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    put32(f, u);
}

static std::vector<uint8_t> script(const std::vector<std::string>& params, uint32_t input, uint32_t output, uint32_t dirty,
                                   const std::vector<float>& constants, const std::vector<uint8_t>& code) {
    std::vector<uint8_t> f;
    put16(f, 1);
    uint32_t size = 1;
    for (auto& p : params) size += uint32_t(p.size()) + 1;
    put32(f, size);
    f.push_back(uint8_t(params.size()));
    for (auto& p : params) f.insert(f.end(), p.begin(), p.end() + 0), f.push_back(0);
    put32(f, input);
    put32(f, output);
    put32(f, dirty);
    put32(f, uint32_t(constants.size()));
    for (float c : constants) putf(f, c);
    put32(f, uint32_t(code.size()));
    f.insert(f.end(), code.begin(), code.end());
    return f;
}

// Tron 2.0's UVPan.tfs, as TSCompiler builds it from
//   UserParams XSpeed, YSpeed; Input UV; Output TexCoord2;
//   BeginScript Mat02 = (Time * XSpeed) % 1.0; Mat12 = (Time * YSpeed) % 1.0; EndScript
static std::vector<uint8_t> uvpan() {
    return script({"XSpeed", "YSpeed"}, IN_UV, OUT_2, DIRTY_EVERYUPDATE | DIRTY_USERVARCHANGED, {1.0f},
                  {OP_MULTIPLY, VAR_TIME, VAR_USER, 1, OP_BIND, 1, VAR_CONSTANT, 1, OP_ASSIGN, 1, 0, VAR_MAT + 2,
                   OP_MULTIPLY, VAR_TIME, VAR_USER + 1, 1, OP_BIND, 1, VAR_CONSTANT, 1, OP_ASSIGN, 1, 0, VAR_MAT + 6});
}

template <class F>
static bool refused(F f) {
    try {
        f();
    } catch (const Error&) {
        return true;
    }
    return false;
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
    {
        auto s = read(uvpan());
        CHECK(s.version == 1 && s.param_block_size == 15);
        CHECK(s.params.size() == 2 && s.params[0] == "XSpeed" && s.params[1] == "YSpeed");
        CHECK(s.input == IN_UV && s.output == OUT_2 && s.dirty == 3);
        CHECK(s.constants.size() == 1 && s.constants[0] == 1.0f);
        CHECK(s.instructions().size() == 6 && s.trailing == 0);
        CHECK(check(s).empty());
        CHECK(var_name(s, VAR_USER + 1) == "YSpeed" && var_name(s, VAR_USER + 2) == "User2");
        CHECK(var_name(s, VAR_MAT + 6) == "Mat12" && var_name(s, VAR_CONSTANT) == "1.0" && var_name(s, 3) == "Stack3");
        CHECK(var_name(s, VAR_LEVELOFFSETY) == "LevelOffsetY" && var_name(s, VAR_ELAPSED) == "Elapsed");
        auto dis = disassemble(s);
        CHECK(dis.size() == 6 && dis[0] == "0000  02 26 20 01  MULTIPLY  Stack1 = Time * XSpeed");
        CHECK(dis[1] == "0004  04 01 28 01  BIND      Stack1 = fmod(Stack1, 1.0)");
        CHECK(dis[2] == "0008  07 01 00 12  ASSIGN    Mat02 = Stack1");
        CHECK(decompile(s) ==
              "UserParams XSpeed, YSpeed;\nInput UV;\nOutput TexCoord2;\nBeginScript\n"
              "\tMat02 = (Time * XSpeed) % 1.0;\n\tMat12 = (Time * YSpeed) % 1.0;\nEndScript\n");
        float user[NUM_USER] = {0.25f, 0.5f};
        auto m = evaluate(s, 5.0f, 0.1f, user);
        CHECK(near(m[2], std::fmod(1.25f, 1.0f)) && near(m[6], std::fmod(2.5f, 1.0f)));
        CHECK(m[0] == 1 && m[5] == 1 && m[10] == 1 && m[15] == 1 && m[1] == 0);
    }
    {
        // Sin is the cosine and Cos the sine, as the engine computes them;
        // the user variables are written back
        auto f = script({"A"}, IN_UV, OUT_2, 2, {0.5f},
                        {OP_FUNCTION, FN_SIN, VAR_CONSTANT, VAR_MAT + 0, OP_FUNCTION, FN_COS, VAR_CONSTANT, VAR_MAT + 1,
                         OP_FUNCTION, FN_SQRSIN, VAR_CONSTANT, VAR_MAT + 2, OP_FUNCTION, FN_DEGTORAD, VAR_USER, VAR_MAT + 3,
                         OP_NEGATE, VAR_CONSTANT, 0, 0, OP_MIN, 0, VAR_CONSTANT, VAR_MAT + 4,
                         OP_MAX, 0, VAR_CONSTANT, VAR_MAT + 5, OP_ADD, VAR_USER, VAR_CONSTANT, VAR_USER});
        auto s = read(f);
        CHECK(check(s).empty());
        float user[NUM_USER] = {90.0f};
        auto m = evaluate(s, 0, 0, user);
        CHECK(near(m[0], std::cos(0.5f)) && near(m[1], std::sin(0.5f)) && m[2] == 1.0f);
        CHECK(near(m[3], 1.5707963f) && m[4] == -0.5f && m[5] == 0.5f);
        CHECK(user[0] == 90.5f);
        CHECK(decompile(s).find("\tA = A + 0.5;\n") != std::string::npos);
        CHECK(decompile(s).find("\tMat10 = -0.5 < 0.5;\n") != std::string::npos);
        CHECK(disassemble(s)[0].find("the engine's Sin is cos") != std::string::npos);
    }
    {
        // problems: an unknown op, function and input, an operand past the
        // variables, a constant not in the table, a stray byte
        auto s = read(script({}, 9, OUT_2, 0, {}, {12, 0, 0, 0, OP_FUNCTION, 7, 0, 0, OP_ADD, 80, 0, 0, OP_ADD, VAR_CONSTANT, 0, 0, 1}));
        auto p = check(s);
        CHECK(p.size() == 6);
        CHECK(disassemble(s).back() == "(1 bytes left over)");
    }
    {
        auto f = uvpan();
        CHECK(refused([] { read({}); }));
        CHECK(refused([&] { read(std::vector<uint8_t>(f.begin(), f.end() - 1)); }));
        auto bad = f;
        bad[0] = 2;
        CHECK(refused([&] { read(bad); }));
        bad = f;
        bad[2] = 14;   // the block one short: its last name runs out
        CHECK(refused([&] { read(bad); }));
        bad = f;
        bad[2] = 16;   // a byte after the names
        CHECK(refused([&] { read(bad); }));
        f.push_back(0);
        CHECK(read(f).trailing == 1);
    }
    {
        // a group: an evaluated stage (DEdit writes six values) and an
        // overridden one
        std::vector<uint8_t> g;
        put32(g, 1);
        put32(g, 2);
        put32(g, STAGE_EVALUATED);
        put32(g, CH_BASE);
        std::string name = "\\Tex\\TexFX\\UVPan.tfs";
        put16(g, uint32_t(name.size()));
        g.insert(g.end(), name.begin(), name.end());
        put32(g, 6);
        for (float v : {0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}) putf(g, v);
        put32(g, STAGE_OVERRIDDEN);
        put32(g, CH_DETAIL);
        put32(g, 0);
        auto gr = read_group(g);
        CHECK(gr.stage_count == 2 && gr.stages.size() == 2 && gr.trailing == 0);
        CHECK(gr.stages[0].script == name && gr.stages[0].defaults.size() == 6 && gr.stages[0].defaults[0] == 0.3f);
        CHECK(gr.stages[1].type == STAGE_OVERRIDDEN && gr.stages[1].channel == CH_DETAIL && gr.stages[1].override_stage == 0);
        CHECK(std::string(channel_name(CH_ENVMAP)) == "Environment map");

        // disabled stages; a third stage stored is past what the engine reads
        std::vector<uint8_t> d;
        put32(d, 1);
        put32(d, 3);
        for (int i = 0; i < 3; ++i) put32(d, STAGE_DISABLED);
        auto dg = read_group(d);
        CHECK(dg.stages.size() == 2 && dg.trailing == 4);
        CHECK(refused([&] { read_group(std::vector<uint8_t>(g.begin(), g.end() - 1)); }));
        auto bad = g;
        bad[0] = 2;
        CHECK(refused([&] { read_group(bad); }));
        CHECK(refused([] { read_group({}); }));
    }
    if (failures) std::printf("%d failures\n", failures);
    else std::printf("texscript_test: all passed\n");
    return failures ? 1 : 0;
}
