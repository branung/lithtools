// LithTech 1's .abc models, version 6 (Shogo, Blood 2, KISS Psycho
// Circus): reading, for previews, and conversion to abc::Model, the shared
// structure the preview draws. The C++ twin of plugins/blender's
// lithtech/abc6.py, whose module notes have the format and the evidence.
//
// In brief: one mesh, each vertex on one node, its position in that node's
// space at frame 0 of animation 0; keyframe rotations stored conjugated;
// every model drawn mirrored in z. to_abc gives each vertex one weight of
// bias 1, binds each node at frame 0, and negates z of positions and
// quaternions (the conjugate and the mirror together). Quaternions keep
// their stored length: thousands in the shipped models are not unit, and
// LithTech 1 interpolates the raw values and normalizes only building the
// matrix, as abc::quat_to_matrix and abc::pose do. A "deformation" node
// also keys its listed vertices per keyframe, as bytes decompressed by the
// track's scale and translation; to_abc carries them in
// Node::animated_vertices and Animation::vertex_frames, which abc::animate
// applies.
#pragma once

#include "lithtech/abc.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace lithtech::abc6 {

using Error = abc::Error;
using Vec3 = abc::Vec3;
using Quat = abc::Quat;

enum NodeFlags : uint8_t { NodeNull = 1, NodeTris = 2, NodeDeformation = 4 };

struct Triangle {
    float uv[3][2];                   // per corner, v down as in DTX
    uint16_t indices[3];              // into Model::vertices
    std::array<int8_t, 3> normal;     // face normal, over 127
};

struct Vertex {
    Vec3 position;                    // in its node's space (see above)
    std::array<int8_t, 3> normal;     // over 127
    uint8_t node = 0;
    uint16_t replacements[2] = {0, 0};
};

struct Node {
    Vec3 bounds_min, bounds_max;
    std::string name;
    uint16_t index = 0;
    uint8_t flags = 0;
    std::vector<uint16_t> animated_vertices;   // the "MD" list
    uint32_t child_count = 0;
    int parent = -1;                  // index into Model::nodes
};

struct Track {
    std::vector<std::pair<Vec3, Quat>> keys;   // as stored
    std::vector<uint8_t> vertex_bytes;          // 3 * animated vertices * keyframes
    Vec3 scale{1, 1, 1}, translation{0, 0, 0};
};

struct Animation {
    std::string name;
    uint32_t length = 0;
    Vec3 bounds_min, bounds_max;
    std::vector<abc::Keyframe> keyframes;
    std::vector<Track> tracks;        // one per node
};

struct Model {
    std::string command_string;
    Vec3 bounds_min, bounds_max;
    std::vector<uint16_t> vertex_start;
    std::vector<Triangle> triangles;
    uint32_t normal_vertex_count = 0;  // the full-detail vertices, first in `vertices`
    std::vector<Vertex> vertices;
    std::vector<Node> nodes;           // depth first, parent first
    std::vector<Animation> animations;
    std::vector<Vec3> anim_dims;       // empty with no AnimDims chunk
};

// A v6 file: its first chunk is Header and starts with
// "MonolithExport Model File v6".
bool is_model(const std::vector<uint8_t>& data);
Model read(const std::vector<uint8_t>& data);
// The shared model, as abc::read gives a v9-12 file: one piece, one LOD,
// in the engine's drawing space.
abc::Model to_abc(const Model& m);

}  // namespace lithtech::abc6
