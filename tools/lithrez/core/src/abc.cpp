#include "lithtech/abc.hpp"

#include <cmath>
#include <cstring>
#include <map>

namespace lithtech::abc {

namespace {

struct Reader {
    const std::vector<uint8_t>* dp;
    std::size_t pos;
    std::size_t end;
    const std::vector<uint8_t>& d() const { return *dp; }
    void need(std::size_t n) {
        if (pos + n > end) throw Error("truncated .abc chunk");
    }
    uint8_t u8() { need(1); return d()[pos++]; }
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
    // A count that must fit in what's left of the chunk at `each` bytes apiece.
    uint32_t count(std::size_t each) {
        uint32_t n = u32();
        if (each && std::size_t(n) * each > end - pos) throw Error("bad count in .abc");
        return n;
    }
};

LOD read_lod(Reader& r) {
    LOD lod;
    uint32_t nt = r.count(30);
    lod.triangles.resize(nt);
    for (auto& t : lod.triangles)
        for (int c = 0; c < 3; ++c) {
            t.uv[c][0] = r.f32();
            t.uv[c][1] = r.f32();
            t.indices[c] = r.u16();
        }
    uint32_t nv = r.count(28);
    lod.vertices.resize(nv);
    for (auto& v : lod.vertices) {
        uint16_t nw = r.u16();
        v.replacement = r.u16();
        v.weights.resize(nw);
        for (auto& w : v.weights) {
            w.node = r.u32();
            w.location = r.vec3();
            w.bias = r.f32();
        }
        v.location = r.vec3();
        v.normal = r.vec3();
    }
    return lod;
}

void read_node(Reader& r, std::vector<Node>& out, int parent, int depth) {
    if (depth > 512) throw Error(".abc node tree too deep");
    Node n;
    n.name = r.str();
    n.index = r.u16();
    n.flags = r.u8();
    for (auto& f : n.bind) f = r.f32();
    n.parent = parent;
    int me = int(out.size());
    out.push_back(std::move(n));
    uint32_t nc = r.count(23);
    for (uint32_t i = 0; i < nc; ++i) read_node(r, out, me, depth + 1);
}

}  // namespace

Model read(const std::vector<uint8_t>& d) {
    // The chunk list: name, offset of the next chunk (0xFFFFFFFF last), body.
    std::map<std::string, std::pair<std::size_t, std::size_t>> chunks;
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
    auto section = [&](const char* name) {
        auto it = chunks.find(name);
        if (it == chunks.end()) throw Error(std::string("no ") + name + " chunk; not a LithTech .abc model?");
        return Reader{&d, it->second.first, it->second.second};
    };

    Model m;
    Reader r = section("Header");
    m.version = r.u32();
    if (m.version < 9 || m.version > 12)
        throw Error(".abc version " + std::to_string(m.version) + " (9 to 12 are supported)");
    for (int i = 0; i < 13; ++i) r.u32();
    m.command_string = r.str();
    m.internal_radius = r.f32();
    uint16_t nlods = r.u16();
    r.need(62);
    r.pos += 62;
    for (int i = 0; i < nlods; ++i) m.lod_distances.push_back(r.f32());

    r = section("Pieces");
    r.u32();
    uint32_t np = r.count(12);
    for (uint32_t i = 0; i < np; ++i) {
        Piece p;
        p.texture_index = r.u16();
        p.specular_power = r.f32();
        p.specular_scale = r.f32();
        if (m.version >= 10) p.lod_weight = r.f32();
        uint16_t nres = r.u16();
        r.need(std::size_t(nres) * 4);
        r.pos += std::size_t(nres) * 4;
        p.name = r.str();
        for (int l = 0; l < 1 + nlods; ++l) p.lods.push_back(read_lod(r));
        m.pieces.push_back(std::move(p));
    }

    r = section("Nodes");
    read_node(r, m.nodes, -1, 0);
    std::size_t nn = m.nodes.size();

    r = section("ChildModels");
    uint32_t nc = r.count(4);
    for (uint32_t i = 0; i < nc; ++i) {
        if (i > 0) m.child_models.push_back(r.str());
        r.u32();
        r.need(nn * 28);
        r.pos += nn * 28;
    }

    r = section("Animation");
    uint32_t na = r.count(16);
    for (uint32_t i = 0; i < na; ++i) {
        Animation a;
        a.dims = r.vec3();
        a.name = r.str();
        if (m.version >= 11) a.weight_set = int32_t(r.u32());
        if (m.version >= 12) a.interpolation_ms = r.u32();
        uint32_t nk = r.count(6);
        for (uint32_t k = 0; k < nk; ++k) {
            uint32_t t = r.u32();
            a.keyframes.push_back({t, r.str()});
        }
        r.need(nn * nk * 28);
        a.transforms.resize(nn);
        for (auto& per : a.transforms) {
            per.resize(nk);
            for (auto& tr : per) {
                tr.first = r.vec3();
                tr.second = r.quat();
            }
        }
        m.animations.push_back(std::move(a));
    }

    r = section("Sockets");
    uint32_t ns = r.count(8);
    for (uint32_t i = 0; i < ns; ++i) {
        Socket s;
        s.node = r.u32();
        s.name = r.str();
        s.rotation = r.quat();
        s.position = r.vec3();
        m.sockets.push_back(std::move(s));
    }
    return m;
}

// Scaled by 2/|q|^2, as every LithTech engine's quat_ConvertToMatrix is
// (LithTech 1's quaternion.h, NOLF's sharedcommon.cpp, Jupiter's
// ltquatbase.cpp), so a quaternion needn't be unit: .abc v6 stores
// thousands that aren't (abc6.hpp). For a unit one it is the plain formula.
Mat4 quat_to_matrix(const Vec3& p, const Quat& q) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float n = x * x + y * y + z * z + w * w;
    float s = n > 0 ? 2 / n : 0;
    return {1 - s * (y * y + z * z), s * (x * y - z * w), s * (x * z + y * w), p[0],
            s * (x * y + z * w), 1 - s * (x * x + z * z), s * (y * z - x * w), p[1],
            s * (x * z - y * w), s * (y * z + x * w), 1 - s * (x * x + y * y), p[2],
            0, 0, 0, 1};
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 o{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[4 * i + k] * b[4 * k + j];
            o[4 * i + j] = s;
        }
    return o;
}

namespace {
Quat slerp(Quat a, Quat b, float t) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) {
        d = -d;
        for (auto& c : b) c = -c;
    }
    float s0 = 1 - t, s1 = t;
    if (d < 0.9995f) {
        float th = std::acos(d), sn = std::sin(th);
        s0 = std::sin((1 - t) * th) / sn;
        s1 = std::sin(t * th) / sn;
    }
    Quat o{};
    float len = 0;
    for (int i = 0; i < 4; ++i) {
        o[i] = s0 * a[i] + s1 * b[i];
        len += o[i] * o[i];
    }
    len = std::sqrt(len);
    if (len > 0)
        for (auto& c : o) c /= len;
    return o;
}
}  // namespace

namespace {
// The two keys around time t and how far between them.
void key_span(const Animation& a, float t, std::size_t& k0, std::size_t& k1, float& f) {
    std::size_t nk = a.keyframes.size();
    k0 = k1 = 0;
    f = 0;
    if (nk > 1) {
        if (t <= a.keyframes.front().time) {
            k0 = k1 = 0;
        } else if (t >= a.keyframes.back().time) {
            k0 = k1 = nk - 1;
        } else {
            while (k1 < nk && a.keyframes[k1].time <= t) ++k1;
            k0 = k1 - 1;
            float span = float(a.keyframes[k1].time - a.keyframes[k0].time);
            f = span > 0 ? (t - a.keyframes[k0].time) / span : 0;
        }
    }
}
}  // namespace

std::vector<Mat4> pose(const Model& m, const Animation& a, float t) {
    std::size_t nk = a.keyframes.size();
    std::size_t k0, k1;
    float f;
    key_span(a, t, k0, k1, f);
    std::vector<Mat4> out(m.nodes.size());
    for (std::size_t i = 0; i < m.nodes.size(); ++i) {
        Mat4 local;
        if (nk == 0 || i >= a.transforms.size()) {
            out[i] = m.nodes[i].bind;   // no keys: the bind pose
            continue;
        } else {
            auto& p0 = a.transforms[i][k0];
            auto& p1 = a.transforms[i][k1];
            Vec3 pos{p0.first[0] + (p1.first[0] - p0.first[0]) * f, p0.first[1] + (p1.first[1] - p0.first[1]) * f,
                     p0.first[2] + (p1.first[2] - p0.first[2]) * f};
            local = quat_to_matrix(pos, slerp(p0.second, p1.second, f));
        }
        int par = m.nodes[i].parent;
        out[i] = par < 0 ? local : multiply(out[std::size_t(par)], local);
    }
    return out;
}

bool has_vertex_animation(const Model& m) {
    for (auto& n : m.nodes)
        if (!n.animated_vertices.empty()) return true;
    return false;
}

void animate(const Model& m, const Animation& a, float t, LOD& lod) {
    if (a.vertex_frames.empty() || a.keyframes.empty()) return;
    std::size_t k0, k1;
    float f;
    key_span(a, t, k0, k1, f);
    for (std::size_t n = 0; n < m.nodes.size() && n < a.vertex_frames.size(); ++n) {
        const auto& frames = a.vertex_frames[n];
        const auto& list = m.nodes[n].animated_vertices;
        if (frames.size() <= k1 || list.empty()) continue;
        for (std::size_t i = 0; i < list.size() && i < frames[k0].size() && i < frames[k1].size(); ++i) {
            if (list[i] >= lod.vertices.size()) continue;
            Vertex& v = lod.vertices[list[i]];
            if (v.weights.size() != 1) continue;
            const Vec3& p0 = frames[k0][i];
            const Vec3& p1 = frames[k1][i];
            for (int c = 0; c < 3; ++c) v.weights[0].location[std::size_t(c)] = p0[std::size_t(c)] + (p1[std::size_t(c)] - p0[std::size_t(c)]) * f;
        }
    }
}

std::vector<Mat4> bind_pose(const Model& m) {
    std::vector<Mat4> out;
    for (auto& n : m.nodes) out.push_back(n.bind);
    return out;
}

// The weights' biases needn't add up to 1, and the offsets are scaled to
// match: in the shipped .abc models 1,707 vertices (47 models: AvP2's
// Dunya and Ancient Predator, NOLF's MaleBystander_Style5, NOLF2's
// DeadPilot...) total 1.5 to 11, and 10,419 more a little over or under 1.
// Divided by the total, every vertex of all 754 models lands within 3
// units of its stored bind position; undivided, those fly hundreds of
// units off.
Vec3 skin(const Vertex& v, const std::vector<Mat4>& M) {
    if (v.weights.empty()) return v.location;
    Vec3 s{0, 0, 0};
    float total = 0;
    for (auto& w : v.weights) {
        if (w.node >= M.size()) continue;
        const Mat4& m = M[w.node];
        for (int i = 0; i < 3; ++i)
            s[i] += m[4 * i] * w.location[0] + m[4 * i + 1] * w.location[1] + m[4 * i + 2] * w.location[2] +
                    m[4 * i + 3] * w.bias;
        total += w.bias;
    }
    if (total > 0)
        for (float& c : s) c /= total;
    return s;
}

}  // namespace lithtech::abc
