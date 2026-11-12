// Texture scripts and effect groups. Layouts (little-endian):
//
// .tfs, as TSCompiler's ParseScript writes it and CTextureScriptInterpreter::
// LoadScript reads it:
//   u16 version (1)
//   u32 parameter block size, then the block: u8 count, the names each
//     with a NUL (the engine skips the block)
//   u32 input, u32 output, u32 dirty flags
//   u32 constant count, the constants (floats)
//   u32 code size, the code: 4 bytes an instruction (op, p1, p2, out)
//
// .tfg, as DEdit's CEditEffectGroupDlg::SaveEffectGroup writes it and
// CTextureScriptMgr::GetInstance reads it:
//   u32 version (1), u32 stage count (the engine reads at most 2)
//   per stage: u32 type; then, unless disabled, u32 channel; overridden:
//     u32 stage; evaluated: the script name (u16 length, the characters;
//     ILTStream::ReadString), u32 count, the default values (floats)

#include "lithtech/texscript.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace lithtech::texscript {

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

struct Reader {
    const uint8_t* d;
    std::size_t n, p = 0;
    const char* what;
    void need(std::size_t k) const {
        if (n - p < k) throw Error(std::string("ends in ") + what);
    }
    uint32_t u32(const char* w) {
        what = w;
        need(4);
        p += 4;
        return rd32(d + p - 4);
    }
    float f32(const char* w) {
        what = w;
        need(4);
        p += 4;
        return rdf(d + p - 4);
    }
};

}  // namespace

std::vector<Instruction> Script::instructions() const {
    std::vector<Instruction> out;
    for (std::size_t i = 0; i + 4 <= code.size(); i += 4) out.push_back({code[i], code[i + 1], code[i + 2], code[i + 3]});
    return out;
}

Script read(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    if (n < 2) throw Error("too short for a texture script");
    Script s;
    s.version = uint16_t(d[0] | d[1] << 8);
    if (s.version != 1) throw Error("version " + std::to_string(s.version) + " (the engine takes only 1)");
    Reader r{d, n, 2, ""};
    s.param_block_size = r.u32("the parameter block's size");
    r.what = "the parameter block";
    r.need(s.param_block_size);
    std::size_t end = r.p + s.param_block_size;
    if (s.param_block_size) {
        unsigned count = d[r.p++];
        for (unsigned i = 0; i < count; ++i) {
            const uint8_t* z = static_cast<const uint8_t*>(std::memchr(d + r.p, 0, end - r.p));
            if (!z) throw Error("parameter " + std::to_string(i) + "'s name runs past the parameter block");
            s.params.emplace_back(reinterpret_cast<const char*>(d + r.p), std::size_t(z - (d + r.p)));
            r.p = std::size_t(z - d) + 1;
        }
        if (r.p != end) throw Error("the parameter block has " + std::to_string(end - r.p) + " bytes after its names");
    }
    r.p = end;
    s.input = r.u32("the input type");
    s.output = r.u32("the output type");
    s.dirty = r.u32("the dirty flags");
    uint32_t nc = r.u32("the constant count");
    if (nc > (n - r.p) / 4) throw Error("constant count " + std::to_string(nc) + " too big for the file");
    for (uint32_t i = 0; i < nc; ++i) s.constants.push_back(r.f32("the constants"));
    uint32_t len = r.u32("the code size");
    r.what = "the code";
    r.need(len);
    s.code.assign(d + r.p, d + r.p + len);
    r.p += len;
    s.trailing = n - r.p;
    return s;
}

const char* input_name(uint32_t i) {
    static const char* const n[] = {"CSNormal", "CSPos", "CSReflection", "WSNormal", "WSPos", "WSReflection", "UV"};
    return i < NUM_INPUTS ? n[i] : nullptr;
}

const char* output_name(uint32_t o) {
    static const char* const n[] = {"TexCoord2", "TexCoord3", "TexCoord3Proj", "TexCoord4Proj"};
    return o < NUM_OUTPUTS ? n[o] : nullptr;
}

const char* op_name(uint8_t op) {
    static const char* const n[] = {"ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "BIND", "FUNCTION", "NEGATE", "ASSIGN", "MIN", "MAX"};
    return op < NUM_OPS ? n[op] : nullptr;
}

const char* function_name(uint8_t fn) {
    static const char* const n[] = {"Sin", "Cos", "Tan", "SqrSin", "SqrCos", "DegToRad"};
    return fn < NUM_FUNCTIONS ? n[fn] : nullptr;
}

std::string number(float f) {
    if (std::isnan(f) || std::isinf(f)) return std::isnan(f) ? "nan" : (f < 0 ? "-inf" : "inf");
    char b[64];
    for (int prec = 1; prec <= 12; ++prec) {
        std::snprintf(b, sizeof b, "%.*f", prec, double(f));
        if (float(std::strtod(b, nullptr)) == f) break;
    }
    std::string s = b;
    while (s.size() > 2 && s.back() == '0' && s[s.size() - 2] != '.') s.pop_back();
    return s;
}

std::string var_name(const Script& s, uint8_t v) {
    if (v < VAR_MAT) return "Stack" + std::to_string(v);
    if (v < VAR_USER) {
        int m = v - VAR_MAT;
        return "Mat" + std::to_string(m / 4) + std::to_string(m % 4);
    }
    if (v < VAR_TIME) {
        std::size_t u = v - VAR_USER;
        return u < s.params.size() ? s.params[u] : "User" + std::to_string(u);
    }
    if (v == VAR_TIME) return "Time";
    if (v == VAR_ELAPSED) return "Elapsed";
    if (v < VAR_LEVELOFFSETX) {
        std::size_t c = v - VAR_CONSTANT;
        return c < s.constants.size() ? number(s.constants[c]) : "Constant" + std::to_string(c);
    }
    if (v == VAR_LEVELOFFSETX) return "LevelOffsetX";
    if (v == VAR_LEVELOFFSETY) return "LevelOffsetY";
    if (v == VAR_LEVELOFFSETZ) return "LevelOffsetZ";
    return "Var" + std::to_string(v);
}

std::vector<std::string> check(const Script& s) {
    std::vector<std::string> out;
    if (!input_name(s.input)) out.push_back("unknown input type " + std::to_string(s.input));
    if (!output_name(s.output)) out.push_back("unknown output type " + std::to_string(s.output));
    if (s.code.size() % 4) out.push_back("code size " + std::to_string(s.code.size()) + " isn't a multiple of 4");
    if (s.params.size() > NUM_USER) out.push_back(std::to_string(s.params.size()) + " parameters (the engine has 6)");
    if (s.constants.size() >= NUM_CONSTANTS) out.push_back(std::to_string(s.constants.size()) + " constants (the engine asserts under 32)");
    auto operand = [&](std::size_t at, uint8_t v, const char* which) {
        std::string where = "instruction at " + std::to_string(at) + ": ";
        if (v >= NUM_VARS) out.push_back(where + which + " " + std::to_string(v) + " is past the variables");
        else if (v >= VAR_CONSTANT && v < VAR_LEVELOFFSETX && std::size_t(v - VAR_CONSTANT) >= s.constants.size())
            out.push_back(where + which + " is constant " + std::to_string(v - VAR_CONSTANT) + ", not in the table");
    };
    auto ins = s.instructions();
    for (std::size_t i = 0; i < ins.size(); ++i) {
        const Instruction& x = ins[i];
        std::size_t at = 4 * i;
        if (x.op >= NUM_OPS) {
            out.push_back("instruction at " + std::to_string(at) + ": unknown op " + std::to_string(x.op));
            continue;
        }
        if (x.op == OP_FUNCTION) {
            if (x.p1 >= NUM_FUNCTIONS)
                out.push_back("instruction at " + std::to_string(at) + ": unknown function " + std::to_string(x.p1));
        } else {
            operand(at, x.p1, "operand 1");
        }
        if (x.op != OP_NEGATE && x.op != OP_ASSIGN) operand(at, x.p2, "operand 2");
        if (x.out >= NUM_VARS) operand(at, x.out, "the result");
    }
    return out;
}

std::vector<std::string> disassemble(const Script& s) {
    std::vector<std::string> out;
    auto ins = s.instructions();
    char b[160];
    for (std::size_t i = 0; i < ins.size(); ++i) {
        const Instruction& x = ins[i];
        const char* op = op_name(x.op);
        std::string a = var_name(s, x.p1), c = var_name(s, x.p2), o = var_name(s, x.out), text;
        switch (x.op) {
        case OP_ADD: text = o + " = " + a + " + " + c; break;
        case OP_SUBTRACT: text = o + " = " + a + " - " + c; break;
        case OP_MULTIPLY: text = o + " = " + a + " * " + c; break;
        case OP_DIVIDE: text = o + " = " + a + " / " + c; break;
        case OP_BIND: text = o + " = fmod(" + a + ", " + c + ")"; break;
        case OP_FUNCTION: {
            const char* f = function_name(x.p1);
            text = o + " = " + (f ? f : "Function" + std::to_string(x.p1)) + "(" + c + ")";
            if (x.p1 == FN_SIN) text += "    ; the engine's Sin is cos";
            else if (x.p1 == FN_COS) text += "    ; the engine's Cos is sin";
            break;
        }
        case OP_NEGATE: text = o + " = -" + a; break;
        case OP_ASSIGN: text = o + " = " + a; break;
        case OP_MIN: text = o + " = min(" + a + ", " + c + ")"; break;
        case OP_MAX: text = o + " = max(" + a + ", " + c + ")"; break;
        default: text = "?"; break;
        }
        std::snprintf(b, sizeof b, "%04zx  %02x %02x %02x %02x  %-8s  ", 4 * i, x.op, x.p1, x.p2, x.out, op ? op : "?");
        out.push_back(b + text);
    }
    if (s.code.size() % 4) out.push_back("(" + std::to_string(s.code.size() % 4) + " bytes left over)");
    return out;
}

std::string decompile(const Script& s) {
    std::string src = "UserParams";
    for (std::size_t i = 0; i < s.params.size(); ++i) src += (i ? ", " : " ") + s.params[i];
    src += ";\n";
    const char* in = input_name(s.input);
    const char* outn = output_name(s.output);
    src += std::string("Input ") + (in ? in : "?" + std::to_string(s.input)) + ";\n";
    src += std::string("Output ") + (outn ? outn : "?" + std::to_string(s.output)) + ";\n";
    src += "BeginScript\n";
    // a stack register holds the expression last computed into it; any
    // other destination ends a statement
    std::string stack[NUM_STACK];
    bool compound[NUM_STACK] = {};
    auto operand = [&](uint8_t v, bool paren) {
        if (v < VAR_MAT) return paren && compound[v] ? "(" + stack[v] + ")" : stack[v];
        return var_name(s, v);
    };
    for (const Instruction& x : s.instructions()) {
        std::string e;
        bool comp = true;
        switch (x.op) {
        case OP_ADD: e = operand(x.p1, false) + " + " + operand(x.p2, true); break;
        case OP_SUBTRACT: e = operand(x.p1, false) + " - " + operand(x.p2, true); break;
        case OP_MULTIPLY: e = operand(x.p1, true) + " * " + operand(x.p2, true); break;
        case OP_DIVIDE: e = operand(x.p1, true) + " / " + operand(x.p2, true); break;
        case OP_BIND: e = operand(x.p1, true) + " % " + operand(x.p2, true); break;
        case OP_MIN: e = operand(x.p1, true) + " < " + operand(x.p2, true); break;
        case OP_MAX: e = operand(x.p1, true) + " > " + operand(x.p2, true); break;
        case OP_FUNCTION: {
            const char* f = function_name(x.p1);
            e = std::string(f ? f : "Function" + std::to_string(x.p1)) + "(" + operand(x.p2, false) + ")";
            comp = false;
            break;
        }
        case OP_NEGATE: e = "-" + operand(x.p1, true); comp = false; break;
        case OP_ASSIGN: e = operand(x.p1, false); comp = x.p1 < VAR_MAT && compound[x.p1]; break;
        default: e = "?"; break;
        }
        if (x.out < VAR_MAT && x.op != OP_ASSIGN) {
            stack[x.out] = e;
            compound[x.out] = comp;
        } else {
            src += "\t" + var_name(s, x.out) + " = " + e + ";\n";
            if (x.out < VAR_MAT) stack[x.out] = e, compound[x.out] = comp;
        }
    }
    src += "EndScript\n";
    return src;
}

std::array<float, 16> evaluate(const Script& s, float time, float elapsed, float user[NUM_USER], const float level_offset[3]) {
    float v[NUM_VARS + 1] = {};
    for (std::size_t i = 0; i < s.constants.size() && i < NUM_CONSTANTS; ++i) v[VAR_CONSTANT + i] = s.constants[i];
    if (user) std::memcpy(v + VAR_USER, user, sizeof(float) * NUM_USER);
    for (int i = 0; i < 4; ++i) v[VAR_MAT + 5 * i] = 1;
    v[VAR_TIME] = time;
    v[VAR_ELAPSED] = elapsed;
    if (level_offset) std::memcpy(v + VAR_LEVELOFFSETX, level_offset, sizeof(float) * 3);
    auto at = [&](uint8_t i) { return i < NUM_VARS ? v[i] : 0.0f; };   // the engine would read past its array
    for (const Instruction& x : s.instructions()) {
        float a = at(x.p1), b = at(x.p2), r = 0;
        switch (x.op) {
        case OP_ADD: r = a + b; break;
        case OP_SUBTRACT: r = a - b; break;
        case OP_MULTIPLY: r = a * b; break;
        case OP_DIVIDE: r = a / b; break;
        case OP_BIND: r = float(std::fmod(a, b)); break;
        case OP_FUNCTION:
            switch (x.p1) {
            case FN_SIN: r = float(std::cos(b)); break;   // as the engine has them
            case FN_COS: r = float(std::sin(b)); break;
            case FN_TAN: r = float(std::tan(b)); break;
            case FN_SQRSIN: r = std::sin(b) >= 0.0 ? 1.0f : -1.0f; break;
            case FN_SQRCOS: r = std::cos(b) >= 0.0 ? 1.0f : -1.0f; break;
            case FN_DEGTORAD: r = b * 0.017453292519f; break;
            default: break;
            }
            break;
        case OP_NEGATE: r = -a; break;
        case OP_ASSIGN: r = a; break;
        case OP_MIN: r = a < b ? a : b; break;   // LTMIN
        case OP_MAX: r = a > b ? a : b; break;   // LTMAX
        default: continue;
        }
        if (x.out < NUM_VARS) v[x.out] = r;
    }
    if (user) std::memcpy(user, v + VAR_USER, sizeof(float) * NUM_USER);
    std::array<float, 16> m;
    std::memcpy(m.data(), v + VAR_MAT, sizeof(float) * 16);
    return m;
}

const char* channel_name(uint32_t c) {
    static const char* const n[] = {"Disabled", "Base Texture", "Detail Texture", "Environment map", "Light Map", "Dual Texture"};
    return c < NUM_CHANNELS ? n[c] : nullptr;
}

const char* stage_type_name(uint32_t t) {
    static const char* const n[] = {"disabled", "overridden", "evaluated"};
    return t < 3 ? n[t] : nullptr;
}

Group read_group(const uint8_t* d, std::size_t n) {
    if (n == 0) throw Error("empty file");
    Reader r{d, n, 0, ""};
    Group g;
    g.version = r.u32("the version");
    if (g.version != kGroupVersion) throw Error("version " + std::to_string(g.version) + " (the engine takes only 1)");
    g.stage_count = r.u32("the stage count");
    uint32_t count = std::min(g.stage_count, kMaxStages);
    for (uint32_t i = 0; i < count; ++i) {
        Stage st;
        st.type = r.u32("a stage's type");
        if (st.type != STAGE_DISABLED) {
            st.channel = r.u32("a stage's channel");
            if (st.type == STAGE_OVERRIDDEN) {
                st.override_stage = r.u32("a stage's override");
            } else if (st.type == STAGE_EVALUATED) {
                r.what = "a stage's script name";
                r.need(2);
                std::size_t len = std::size_t(d[r.p] | d[r.p + 1] << 8);
                r.p += 2;
                r.need(len);
                st.script.assign(reinterpret_cast<const char*>(d + r.p), len);
                r.p += len;
                uint32_t nd = r.u32("a stage's value count");
                if (nd > (n - r.p) / 4) throw Error("value count " + std::to_string(nd) + " too big for the file");
                for (uint32_t k = 0; k < nd; ++k) st.defaults.push_back(r.f32("a stage's values"));
            }
        }
        g.stages.push_back(std::move(st));
    }
    g.trailing = n - r.p;
    return g;
}

}  // namespace lithtech::texscript
