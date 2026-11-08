#include "lithtech/abc6.hpp"

#include <cmath>
#include <cstring>
#include <map>

namespace lithtech::abc6 {

namespace {

const char kToken[] = "MonolithExport Model File v6";

struct Reader {
    const std::vector<uint8_t>* dp;
    std::size_t pos;
    std::size_t end;
    const std::vector<uint8_t>& d() const { return *dp; }
    void need(std::size_t n) {
        if (pos + n > end) throw Error("truncated v6 .abc chunk");
    }
    uint8_t u8() { need(1); return d()[pos++]; }
    int8_t i8() { return int8_t(u8()); }
    uint16_t u16() { need(2); uint16_t v = uint16_t(d()[pos] | d()[pos + 1] << 8); pos += 2; return v; }
    uint32_t u32() {
        need(4);
        uint32_t v = uint32_t(d()[pos]) | uint32_t(d()[pos + 1]) << 8 | uint32_t(d()[pos + 2]) << 16 | uint32_t(d()[pos + 3]) << 24;
        pos += 4;
        return v;
    }
    float f32() { uint32_t u = u32(); float f; std::memcpy(&f, &u, 4); return f; }
    Vec3 vec3() { return {f32(), f32(), f32()}; }
    Quat quat() { return {f32(), f32(), f32(), f32()}; }
    std::string str() {
        uint16_t n = u16();
        need(n);
        std::string s(reinterpret_cast<const char*>(&d()[pos]), n);
        pos += n;
        return s;
    }
    uint32_t count(std::size_t each) {
        uint32_t n = u32();
        if (each && std::size_t(n) * each > end - pos) throw Error("bad count in v6 .abc");
        return n;
    }
};

using Chunks = std::map<std::string, std::pair<std::size_t, std::size_t>>;

Chunks chunk_list(const std::vector<uint8_t>& d) {
    Chunks chunks;
    std::size_t pos = 0;
    for (int i = 0; i < 0x200; ++i) {
        Reader r{&d, pos, d.size()};
        std::string name = r.str();
        uint32_t next = r.u32();
        std::size_t end = next == 0xFFFFFFFF ? d.size() : next;
        if (end > d.size() || end < r.pos) throw Error("bad .abc chunk offset");
        chunks[name] = {r.pos, end};
        if (next == 0xFFFFFFFF) break;
        if (next <= pos) throw Error("bad .abc chunk list");
        pos = next;
    }
    return chunks;
}

Vec3 mirror(const Vec3& p) { return {p[0], p[1], -p[2]}; }

// A stored quaternion in the drawing space: z negated (the conjugate and
// the mirror together), its length kept. LithTech 1 interpolates the raw
// values (quat_Slerp, which takes a plain lerp when the dot passes 1, as it
// does for a long one) and only normalizes building the matrix, as
// abc::quat_to_matrix and abc::pose do.
Quat drawn(const Quat& q) { return {q[0], q[1], -q[2], q[3]}; }

std::vector<Vec3> vertex_frame(const Track& t, std::size_t count, std::size_t key) {
    std::vector<Vec3> out(count);
    std::size_t base = key * count * 3;
    for (std::size_t j = 0; j < count; ++j) {
        if (base + 3 * j + 2 >= t.vertex_bytes.size()) break;
        Vec3 p;
        for (int c = 0; c < 3; ++c)
            p[std::size_t(c)] = float(t.vertex_bytes[base + 3 * j + std::size_t(c)]) * t.scale[std::size_t(c)] +
                                t.translation[std::size_t(c)];
        out[j] = mirror(p);
    }
    return out;
}

Vec3 transform_point(const abc::Mat4& M, const Vec3& p) {
    Vec3 o;
    for (int i = 0; i < 3; ++i)
        o[std::size_t(i)] = M[std::size_t(4 * i)] * p[0] + M[std::size_t(4 * i + 1)] * p[1] +
                            M[std::size_t(4 * i + 2)] * p[2] + M[std::size_t(4 * i + 3)];
    return o;
}

Vec3 transform_vector(const abc::Mat4& M, const Vec3& n) {
    Vec3 o;
    for (int i = 0; i < 3; ++i)
        o[std::size_t(i)] = M[std::size_t(4 * i)] * n[0] + M[std::size_t(4 * i + 1)] * n[1] + M[std::size_t(4 * i + 2)] * n[2];
    return o;
}

}  // namespace

bool is_model(const std::vector<uint8_t>& d) {
    try {
        Reader r{&d, 0, d.size()};
        if (r.str() != "Header") return false;
        r.u32();
        return r.str() == kToken;
    } catch (const Error&) {
        return false;
    }
}

Model read(const std::vector<uint8_t>& d) {
    Chunks chunks = chunk_list(d);
    auto section = [&](const char* name) {
        auto it = chunks.find(name);
        if (it == chunks.end()) throw Error(std::string("no ") + name + " chunk; not a v6 .abc?");
        return Reader{&d, it->second.first, it->second.second};
    };

    Model m;
    Reader r = section("Header");
    if (r.str() != kToken) throw Error("not a v6 .abc");
    m.command_string = r.str();

    r = section("Geometry");
    m.bounds_min = r.vec3();
    m.bounds_max = r.vec3();
    uint32_t nlod = r.count(2);
    m.vertex_start.resize(std::size_t(nlod) + 1);
    for (auto& v : m.vertex_start) v = r.u16();
    uint32_t nt = r.count(33);
    m.triangles.resize(nt);
    for (auto& t : m.triangles) {
        for (int c = 0; c < 3; ++c) {
            t.uv[c][0] = r.f32();
            t.uv[c][1] = r.f32();
        }
        for (auto& i : t.indices) i = r.u16();
        for (auto& n : t.normal) n = r.i8();
    }
    uint32_t nv = r.count(20);
    m.normal_vertex_count = r.u32();
    m.vertices.resize(nv);
    for (auto& v : m.vertices) {
        v.position = r.vec3();
        for (auto& n : v.normal) n = r.i8();
        v.node = r.u8();
        v.replacements[0] = r.u16();
        v.replacements[1] = r.u16();
    }

    // Nodes, depth first, each with its number of children.
    r = section("Nodes");
    std::vector<std::pair<int, uint32_t>> stack;   // (node, children still to come)
    uint32_t remaining = 1;
    while (remaining) {
        if (m.nodes.size() > 4096) throw Error("v6 .abc node tree too large");
        Node n;
        n.bounds_min = r.vec3();
        n.bounds_max = r.vec3();
        n.name = r.str();
        n.index = r.u16();
        n.flags = r.u8();
        uint32_t nmd = r.count(2);
        n.animated_vertices.resize(nmd);
        for (auto& v : n.animated_vertices) v = r.u16();
        n.child_count = r.u32();
        n.parent = stack.empty() ? -1 : stack.back().first;
        if (!stack.empty() && --stack.back().second == 0) stack.pop_back();
        while (!stack.empty() && stack.back().second == 0) stack.pop_back();
        int me = int(m.nodes.size());
        m.nodes.push_back(std::move(n));
        if (m.nodes.back().child_count) stack.push_back({me, m.nodes.back().child_count});
        remaining += m.nodes.back().child_count - 1;
    }

    r = section("Animation");
    uint32_t na = r.count(4);
    for (uint32_t i = 0; i < na; ++i) {
        Animation a;
        a.name = r.str();
        a.length = r.u32();
        a.bounds_min = r.vec3();
        a.bounds_max = r.vec3();
        uint32_t nk = r.count(30);
        for (uint32_t k = 0; k < nk; ++k) {
            uint32_t t = r.u32();
            r.vec3();
            r.vec3();   // the keyframe's box
            a.keyframes.push_back({t, r.str()});
        }
        a.tracks.resize(m.nodes.size());
        for (std::size_t n = 0; n < m.nodes.size(); ++n) {
            Track& tr = a.tracks[n];
            r.need(std::size_t(nk) * 28);
            tr.keys.resize(nk);
            for (auto& key : tr.keys) {
                key.first = r.vec3();
                key.second = r.quat();
            }
            std::size_t nb = std::size_t(3) * m.nodes[n].animated_vertices.size() * nk;
            r.need(nb);
            tr.vertex_bytes.assign(d.begin() + std::ptrdiff_t(r.pos), d.begin() + std::ptrdiff_t(r.pos + nb));
            r.pos += nb;
            tr.scale = r.vec3();
            tr.translation = r.vec3();
        }
        m.animations.push_back(std::move(a));
    }

    if (chunks.count("AnimDims")) {
        r = section("AnimDims");
        for (std::size_t i = 0; i < m.animations.size(); ++i) m.anim_dims.push_back(r.vec3());
    }

    for (auto& t : m.triangles)
        for (auto i : t.indices)
            if (i >= m.vertices.size()) throw Error("a v6 triangle indexes past the vertices");
    for (auto& n : m.nodes)
        for (auto v : n.animated_vertices)
            if (v >= m.vertices.size()) throw Error("a v6 node animates a vertex past the vertices");
    return m;
}

abc::Model to_abc(const Model& m) {
    const std::size_t nn = m.nodes.size();
    const Animation* first = m.animations.empty() ? nullptr : &m.animations[0];
    const bool posed = first && !first->keyframes.empty();
    const abc::Mat4 identity = abc::quat_to_matrix({0, 0, 0}, {0, 0, 0, 1});

    // Each node bound at frame 0 of animation 0.
    std::vector<abc::Mat4> binds(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        abc::Mat4 local = identity;
        if (posed) {
            auto& key = first->tracks[i].keys[0];
            local = abc::quat_to_matrix(mirror(key.first), drawn(key.second));
        }
        int p = m.nodes[i].parent;
        binds[i] = p < 0 ? local : abc::multiply(binds[std::size_t(p)], local);
    }

    // Node-space positions: as stored, mirrored; an animated vertex's from
    // frame 0 of animation 0 (the later entry for a vertex wins).
    std::vector<Vec3> local(m.vertices.size());
    for (std::size_t vi = 0; vi < m.vertices.size(); ++vi) local[vi] = mirror(m.vertices[vi].position);
    if (posed)
        for (std::size_t i = 0; i < nn; ++i) {
            auto& list = m.nodes[i].animated_vertices;
            if (list.empty()) continue;
            auto frame = vertex_frame(first->tracks[i], list.size(), 0);
            for (std::size_t j = 0; j < list.size(); ++j) local[list[j]] = frame[j];
        }

    // LOD 0: the full-detail vertices, which are all the triangles use.
    std::size_t count = m.normal_vertex_count;
    bool fits = count > 0 && count <= m.vertices.size();
    for (auto& t : m.triangles)
        for (auto i : t.indices)
            if (i >= count) fits = false;
    if (!fits) count = m.vertices.size();

    abc::LOD lod;
    lod.vertices.resize(count);
    for (std::size_t vi = 0; vi < count; ++vi) {
        const Vertex& v = m.vertices[vi];
        uint32_t node = v.node < nn ? v.node : 0;
        const abc::Mat4& M = nn ? binds[node] : identity;
        abc::Vertex& o = lod.vertices[vi];
        o.weights.push_back({node, local[vi], 1.0f});
        o.replacement = 0;
        o.location = transform_point(M, local[vi]);
        o.normal = transform_vector(M, mirror({v.normal[0] / 127.f, v.normal[1] / 127.f, v.normal[2] / 127.f}));
    }
    for (auto& t : m.triangles) {
        abc::Triangle o{};
        for (int c = 0; c < 3; ++c) {
            o.indices[c] = t.indices[c];
            o.uv[c][0] = t.uv[c][0];
            o.uv[c][1] = t.uv[c][1];
        }
        lod.triangles.push_back(o);
    }

    abc::Model o;
    o.version = 6;
    o.command_string = m.command_string;
    abc::Piece piece;
    piece.name = "Piece";
    piece.lods.push_back(std::move(lod));
    o.pieces.push_back(std::move(piece));

    bool any_va = false;
    std::vector<std::vector<std::size_t>> keep(nn);   // entries of each list that land in LOD 0
    for (std::size_t i = 0; i < nn; ++i) {
        const Node& n = m.nodes[i];
        abc::Node an;
        an.name = n.name;
        an.index = n.index;
        an.flags = n.flags;
        an.bind = binds[i];
        an.parent = n.parent;
        for (std::size_t j = 0; j < n.animated_vertices.size(); ++j)
            if (n.animated_vertices[j] < count) {
                an.animated_vertices.push_back(n.animated_vertices[j]);
                keep[i].push_back(j);
            }
        any_va = any_va || !an.animated_vertices.empty();
        o.nodes.push_back(std::move(an));
    }

    for (std::size_t ai = 0; ai < m.animations.size(); ++ai) {
        const Animation& a = m.animations[ai];
        abc::Animation oa;
        oa.name = a.name;
        if (ai < m.anim_dims.size()) oa.dims = m.anim_dims[ai];
        else
            for (int c = 0; c < 3; ++c)
                oa.dims[std::size_t(c)] = (a.bounds_max[std::size_t(c)] - a.bounds_min[std::size_t(c)]) / 2;
        oa.keyframes = a.keyframes;
        oa.transforms.resize(nn);
        for (std::size_t n = 0; n < nn; ++n)
            for (auto& key : a.tracks[n].keys) oa.transforms[n].push_back({mirror(key.first), drawn(key.second)});
        if (any_va) {
            oa.vertex_frames.resize(nn);
            for (std::size_t n = 0; n < nn; ++n) {
                auto& list = m.nodes[n].animated_vertices;
                if (keep[n].empty()) continue;
                for (std::size_t k = 0; k < a.keyframes.size(); ++k) {
                    auto frame = vertex_frame(a.tracks[n], list.size(), k);
                    std::vector<Vec3> kept;
                    for (auto j : keep[n]) kept.push_back(frame[j]);
                    oa.vertex_frames[n].push_back(std::move(kept));
                }
            }
        }
        o.animations.push_back(std::move(oa));
    }
    return o;
}

}  // namespace lithtech::abc6
