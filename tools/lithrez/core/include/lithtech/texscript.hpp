// Texture effects (NOLF2, Contract J.A.C.K., Tron 2.0): compiled texture
// scripts (.tfs), which compute a texture transform each frame on a small
// register machine, and texture effect groups (.tfg), which give a world
// texture's stages their scripts and parameter values. Written by
// TSCompiler and DEdit's effect group editor, read by the Jupiter
// renderer's CTextureScriptInterpreter and CTextureScriptMgr (Build 69's
// Engine/runtime/render_a/src/sys/d3d); see docs/TEXFX.md.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::texscript {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// texturescriptdefs.h: the variable file the bytecode works on.
enum Var : uint8_t {
    VAR_STACK = 0, NUM_STACK = 16,
    VAR_MAT = 16, NUM_MAT = 16,       // Mat<row><col>, row-major
    VAR_USER = 32, NUM_USER = 6,
    VAR_TIME = 38, VAR_ELAPSED = 39,
    VAR_CONSTANT = 40, NUM_CONSTANTS = 32,
    VAR_LEVELOFFSETX = 72, VAR_LEVELOFFSETY = 73, VAR_LEVELOFFSETZ = 74,
    NUM_VARS = 75,
};

enum Op : uint8_t { OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE, OP_BIND, OP_FUNCTION, OP_NEGATE, OP_ASSIGN, OP_MIN, OP_MAX, NUM_OPS };
enum Function : uint8_t { FN_SIN, FN_COS, FN_TAN, FN_SQRSIN, FN_SQRCOS, FN_DEGTORAD, NUM_FUNCTIONS };
enum Input : uint32_t { IN_CSNORMAL, IN_CSPOS, IN_CSREFLECTION, IN_WSNORMAL, IN_WSPOS, IN_WSREFLECTION, IN_UV, NUM_INPUTS };
enum Output : uint32_t { OUT_2, OUT_3, OUT_3PROJ, OUT_4PROJ, NUM_OUTPUTS };
enum Dirty : uint32_t { DIRTY_EVERYUPDATE = 1, DIRTY_USERVARCHANGED = 2 };

struct Instruction {
    uint8_t op = 0, p1 = 0, p2 = 0, out = 0;
};

struct Script {
    uint16_t version = 1;
    uint32_t param_block_size = 0;     // as stored: 1 + the names with their NULs
    std::vector<std::string> params;   // UserParams: the user variables' names
    uint32_t input = IN_UV, output = OUT_2, dirty = 0;
    std::vector<float> constants;
    std::vector<uint8_t> code;         // 4 bytes an instruction
    std::size_t trailing = 0;

    std::vector<Instruction> instructions() const;
};

Script read(const uint8_t* data, std::size_t size);
inline Script read(const std::vector<uint8_t>& data) { return read(data.data(), data.size()); }

// The script's language (TSCompiler's tokens).
const char* input_name(uint32_t input);     // "UV", "WSPos", ...; nullptr if unknown
const char* output_name(uint32_t output);   // "TexCoord2", ...
const char* op_name(uint8_t op);            // "ADD", ...
const char* function_name(uint8_t fn);      // "Sin", ...
// A variable as the script names it: Stack3, Mat02, a UserParams name (or
// User4 past them), Time, Elapsed, LevelOffsetX, or a constant's value.
std::string var_name(const Script& s, uint8_t var);
// How TSCompiler would print a constant (digits and a point: its numbers
// have no sign or exponent).
std::string number(float f);

// Problems the engine wouldn't catch (it checks only the version): an
// unknown op, function, input or output, an operand past the variables,
// a constant the table doesn't have, a code length not a multiple of 4.
std::vector<std::string> check(const Script& s);

// One line per instruction: offset, op, operands, as the interpreter
// reads them.
std::vector<std::string> disassemble(const Script& s);
// The script as TSCompiler's source: UserParams, Input, Output, then the
// statements between BeginScript and EndScript, the stack temporaries
// folded back into expressions.
std::string decompile(const Script& s);

// CTextureScriptInterpreter::Evaluate: the matrix (row-major, m[row][col],
// identity to start) after running the code with these times, user
// variables (updated in place, as the engine writes them back) and level
// offset. As the engine computes it, so Sin is the cosine and Cos the sine
// (texturescriptinterpreter.cpp's TSFuncSin and TSFuncCos are swapped).
std::array<float, 16> evaluate(const Script& s, float time, float elapsed, float user[NUM_USER],
                               const float level_offset[3] = nullptr);

// A texture effect group (.tfg), CTextureScriptMgr::GetInstance's reading.
enum StageType : uint32_t { STAGE_DISABLED = 0, STAGE_OVERRIDDEN = 1, STAGE_EVALUATED = 2 };
enum Channel : uint32_t { CH_NULL, CH_BASE, CH_DETAIL, CH_ENVMAP, CH_LIGHTMAP, CH_DUALTEXTURE, NUM_CHANNELS };
constexpr uint32_t kGroupVersion = 1;
constexpr uint32_t kMaxStages = 2;   // CTextureScriptInstance::NUM_STAGES

struct Stage {
    uint32_t type = STAGE_DISABLED;
    uint32_t channel = CH_NULL;
    uint32_t override_stage = 0;       // STAGE_OVERRIDDEN: the stage whose transform it uses
    std::string script;                // STAGE_EVALUATED: the .tfs, from the resource root
    std::vector<float> defaults;       // its user variables' values
};

struct Group {
    uint32_t version = kGroupVersion;
    uint32_t stage_count = 0;          // as stored; the engine reads at most kMaxStages
    std::vector<Stage> stages;
    std::size_t trailing = 0;
};

Group read_group(const uint8_t* data, std::size_t size);
inline Group read_group(const std::vector<uint8_t>& data) { return read_group(data.data(), data.size()); }
const char* channel_name(uint32_t channel);   // DEdit's names: "Base Texture", ...
const char* stage_type_name(uint32_t type);

}  // namespace lithtech::texscript
