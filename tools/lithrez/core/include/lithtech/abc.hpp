// LithTech .abc models, versions 9-12 (NOLF, AvP2, AvP2 Primal Hunt):
// reading, for previews. The C++ twin of plugins/blender's lithtech/abc.py;
// plugins/blender/docs/ABC.md has the format and the evidence. Version 6
// (Shogo, Blood 2, KISS) is lithtech::abc6, converted to this Model.

#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lithtech::abc {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;      // x, y, z, w
using Mat4 = std::array<float, 16>;     // row-major, translation in column 3

struct Weight { uint32_t node; Vec3 location; float bias; };
struct Vertex { std::vector<Weight> weights; uint16_t replacement; Vec3 location, normal; };
struct Triangle { uint16_t indices[3]; float uv[3][2]; };   // v points down
struct LOD { std::vector<Triangle> triangles; std::vector<Vertex> vertices; };

struct Piece {
    std::string name;
    uint16_t texture_index = 0;
    float specular_power = 5, specular_scale = 1, lod_weight = 1;
    std::vector<LOD> lods;
};

struct Node {
    std::string name;
    uint16_t index = 0;
    uint8_t flags = 0;
    Mat4 bind{};           // node -> model space at the bind pose
    int parent = -1;       // index into Model::nodes
    // Vertex animation, which only .abc v6 has (lithtech::abc6): the vertices
    // of pieces[0].lods[0] whose positions this node keys per keyframe, in
    // Animation::vertex_frames order. Empty in versions 9-12.
    std::vector<uint32_t> animated_vertices;
};

struct Keyframe { uint32_t time; std::string command; };

struct Animation {
    std::string name;
    Vec3 dims{};
    int32_t weight_set = -1;
    uint32_t interpolation_ms = 200;
    std::vector<Keyframe> keyframes;
    // transforms[node][key], relative to the parent node
    std::vector<std::vector<std::pair<Vec3, Quat>>> transforms;
    // vertex_frames[node][key][i]: the node-space position of the node's
    // animated_vertices[i] at that key (.abc v6 only; empty otherwise).
    std::vector<std::vector<std::vector<Vec3>>> vertex_frames;
    uint32_t length_ms() const { return keyframes.empty() ? 0 : keyframes.back().time; }
};

struct Socket { uint32_t node; std::string name; Quat rotation; Vec3 position; };

struct Model {
    uint32_t version = 0;
    std::string command_string;
    float internal_radius = 0;
    std::vector<float> lod_distances;
    std::vector<Piece> pieces;
    std::vector<Node> nodes;           // depth-first, parent first
    std::vector<std::string> child_models;   // file names, the model itself excluded
    std::vector<Animation> animations;
    std::vector<Socket> sockets;
};

Model read(const std::vector<uint8_t>& data);

Mat4 quat_to_matrix(const Vec3& position, const Quat& rotation);
Mat4 multiply(const Mat4& a, const Mat4& b);
// Each node's model-space matrix at time t (ms) of an animation, keys
// interpolated linearly (slerp for rotations).
std::vector<Mat4> pose(const Model& m, const Animation& a, float t_ms);
std::vector<Mat4> bind_pose(const Model& m);
// A vertex placed by its weights: sum of matrix @ (x, y, z, bias).
Vec3 skin(const Vertex& v, const std::vector<Mat4>& matrices);
// For a model whose nodes animate vertices (.abc v6): moves each animated
// vertex of `lod` (a copy of pieces[0].lods[0]) to its keyed node-space
// position at time t, interpolated linearly between keys as pose() is, by
// setting its one weight's location; skin() then places it as usual. A
// later entry for the same vertex wins. Does nothing for other models.
void animate(const Model& m, const Animation& a, float t_ms, LOD& lod);
bool has_vertex_animation(const Model& m);

}  // namespace lithtech::abc
