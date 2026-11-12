// .ltb reading, ported from plugins/blender's lithtech/ltb.py.

#include "lithtech/ltb.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace lithtech::ltb {

namespace {

constexpr std::size_t kHeaderSize = 20;
constexpr uint8_t kD3DModelFile = 1;
constexpr uint16_t kD3DVersion = 9;

constexpr uint32_t VDT_POSITION = 0x0001, VDT_NORMAL = 0x0002, VDT_UVSETS_1 = 0x0010, VDT_UVSETS_2 = 0x0020,
                   VDT_UVSETS_3 = 0x0040, VDT_UVSETS_4 = 0x0080, VDT_BASISVECTORS = 0x0100;

constexpr float kPos16Scale = 16.0f;
constexpr float kQuat16Scale = 32767.0f;

// What a model file version stores (ltb.py's Layout).
struct Layout {
    int alloc_count;
    bool render_priority, used_nodes, child_relations, rel_key_times, rel_count_extra, none_interleaved;
    int obb_floats;   // floats before the node index
    bool obb_radius;
    explicit Layout(uint32_t v)
        : alloc_count(v >= 23 ? 15 : 14), render_priority(v >= 20), used_nodes(v >= 22), child_relations(v < 20),
          rel_key_times(v <= 20), rel_count_extra(v == 21 || v == 22), none_interleaved(v < 20),
          obb_floats(v < 20 ? 10 : 15), obb_radius(v >= 24) {}
};

struct Reader {
    const std::vector<uint8_t>* d;
    std::size_t pos = 0;
    std::size_t size() const { return d->size(); }
    void need(std::size_t n) const {
        if (n > size() || pos > size() - n) throw Error("truncated .ltb");
    }
    uint8_t u8() { need(1); return (*d)[pos++]; }
    uint16_t u16() {
        need(2);
        uint16_t v = uint16_t((*d)[pos] | (*d)[pos + 1] << 8);
        pos += 2;
        return v;
    }
    uint32_t u32() {
        need(4);
        uint32_t v = uint32_t((*d)[pos]) | uint32_t((*d)[pos + 1]) << 8 | uint32_t((*d)[pos + 2]) << 16 |
                     uint32_t((*d)[pos + 3]) << 24;
        pos += 4;
        return v;
    }
    int32_t i32() { return int32_t(u32()); }
    int16_t i16() { return int16_t(u16()); }
    float f32() {
        uint32_t u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    Vec3 vec3() { return {f32(), f32(), f32()}; }
    Quat quat() { return {f32(), f32(), f32(), f32()}; }
    std::string str() {
        uint16_t n = u16();
        need(n);
        std::string s(reinterpret_cast<const char*>(d->data() + pos), n);
        pos += n;
        return s;
    }
    void skip(std::size_t n) {
        need(n);
        pos += n;
    }
    // A count of items of at least `each` bytes that must fit in what's left.
    uint32_t count(std::size_t each) {
        uint32_t n = u32();
        if (each && std::size_t(n) > (size() - pos) / each) throw Error("bad count in .ltb");
        return n;
    }
};

int uv_sets(uint32_t f) {
    if (f & VDT_UVSETS_1) return 1;
    if (f & VDT_UVSETS_2) return 2;
    if (f & VDT_UVSETS_3) return 3;
    if (f & VDT_UVSETS_4) return 4;
    return 0;
}

// (weights per vertex, has bone index bytes) of the position stream.
std::pair<int, bool> blend_layout(const Mesh& m) {
    if (m.kind == SKEL_MESH) {
        if (m.use_matrix_palettes) {
            if (m.max_bones_per_vert < 2 || m.max_bones_per_vert > 4)
                throw Error("matrix palette mesh with " + std::to_string(m.max_bones_per_vert) + " bones per vertex");
            return {int(m.max_bones_per_vert) - 1, true};
        }
        if (m.max_bones_per_tri == 0) return {0, false};
        if (m.max_bones_per_tri > 4)
            throw Error("skeletal mesh with " + std::to_string(m.max_bones_per_tri) + " bones per triangle");
        return {int(m.max_bones_per_tri) - 1, false};
    }
    return {0, false};
}

void read_streams(Reader& r, Mesh& m) {
    auto [nw, has_index] = blend_layout(m);
    // every vertex holds at least a few bytes in some stream; guard the size
    if (m.vertex_count > r.size()) throw Error("bad vertex count in .ltb");
    m.vertices.assign(m.vertex_count, Vertex{});
    for (uint32_t flags : m.stream_flags) {
        if (!flags) continue;
        bool pn = (flags & VDT_POSITION) && (flags & VDT_NORMAL);
        int nuv = uv_sets(flags);
        bool basis = flags & VDT_BASISVECTORS;
        for (Vertex& v : m.vertices) {
            if (pn) {
                v.position = r.vec3();
                v.weights.resize(std::size_t(nw));
                for (int i = 0; i < nw; ++i) v.weights[std::size_t(i)] = r.f32();
                if (has_index)
                    for (auto& b : v.bone_indices) b = r.u8();
                v.normal = r.vec3();
            }
            if (nuv) {
                v.uvs.resize(std::size_t(nuv));
                for (auto& uv : v.uvs) uv = {r.f32(), r.f32()};
            }
            if (basis) r.skip(std::size_t(m.basis_floats) * 4);
        }
    }
}

Mesh read_mesh_as(Reader& r, int kind, int basis_floats) {
    uint32_t size = r.u32();
    std::size_t start = r.pos;
    r.need(size);
    Mesh m;
    m.kind = kind;
    m.basis_floats = basis_floats;
    if (kind == NULL_MESH) {
        r.skip(size);
        return m;
    }
    if (kind != RIGID_MESH && kind != SKEL_MESH && kind != VA_MESH)
        throw Error("unknown render object type " + std::to_string(kind));
    m.vertex_count = r.u32();
    if (kind == VA_MESH) m.undup_vertex_count = r.u32();
    m.poly_count = r.u32();
    m.max_bones_per_tri = r.u32();
    m.max_bones_per_vert = r.u32();
    if (kind == SKEL_MESH) m.reindexed_bones_flag = r.u8() != 0;
    for (auto& f : m.stream_flags) f = r.u32();
    if (kind == SKEL_MESH) {
        m.use_matrix_palettes = r.u8() != 0;
        if (m.use_matrix_palettes) {
            m.min_bone = r.u32();
            m.max_bone = r.u32();
            if (m.reindexed_bones_flag) {
                uint32_t n = r.count(4);
                for (uint32_t i = 0; i < n; ++i) m.reindexed_bones.push_back(r.u32());
            }
        }
    } else if (kind == VA_MESH) {
        m.anim_node_index = r.u32();
        m.bone_effector = r.u32();
    } else {
        m.bone_effector = r.u32();
    }
    if (kind == SKEL_MESH && !m.use_matrix_palettes && m.max_bones_per_tri == 0) {
        // Find the vertex size that leaves the indices and whole bone sets.
        std::size_t rest = size - (r.pos - start);
        bool found = false;
        for (std::size_t vsize = 4; vsize <= 128; vsize += 4) {
            std::size_t vbytes = vsize * m.vertex_count;
            std::size_t ib = 6 * std::size_t(m.poly_count);
            if (vbytes + ib + 4 > rest) break;
            std::size_t tail = rest - vbytes - ib;
            std::size_t at = r.pos + vbytes + ib;
            uint32_t nsets = uint32_t((*r.d)[at]) | uint32_t((*r.d)[at + 1]) << 8 | uint32_t((*r.d)[at + 2]) << 16 |
                             uint32_t((*r.d)[at + 3]) << 24;
            if (tail == 4 + 12 * std::size_t(nsets)) {
                m.vertices.assign(m.vertex_count, Vertex{});
                for (uint32_t i = 0; i < m.vertex_count; ++i) {
                    std::size_t p = r.pos + i * vsize;
                    if (vsize >= 12) {
                        Reader q{r.d, p};
                        m.vertices[i].position = q.vec3();
                    }
                }
                r.skip(vbytes);
                m.raw_vertices = true;
                found = true;
                break;
            }
        }
        if (!found) throw Error("can't find the vertex size of a 0-bone skeletal mesh");
    } else {
        read_streams(r, m);
    }
    std::size_t n = std::size_t(m.poly_count) * 3;
    r.need(n * 2);
    m.indices.resize(n);
    for (auto& i : m.indices) i = r.u16();
    if (kind == SKEL_MESH && !m.use_matrix_palettes) {
        uint32_t ns = r.count(12);
        for (uint32_t i = 0; i < ns; ++i) {
            BoneSet b;
            b.first_vertex = r.u16();
            b.vertex_count = r.u16();
            for (auto& x : b.bones) x = r.u8();
            b.index_into_index_buffer = r.u32();
            m.bone_sets.push_back(b);
        }
    } else if (kind == VA_MESH) {
        uint32_t nd = r.count(4);
        for (uint32_t i = 0; i < nd; ++i) {
            uint16_t a = r.u16();
            m.dup_map.emplace_back(a, r.u16());
        }
    }
    if (r.pos - start != size)
        throw Error("render object: read " + std::to_string(r.pos - start) + " bytes of " + std::to_string(size));
    return m;
}

Mesh read_mesh(Reader& r, int kind) {
    std::size_t start = r.pos;
    try {
        return read_mesh_as(r, kind, 6);
    } catch (const Error&) {
        // a basis-vector mesh whose vertices hold 3 basis floats
        r.pos = start;
        return read_mesh_as(r, kind, 3);
    }
}

void read_node(Reader& r, std::vector<Node>& out, int parent, int depth) {
    if (depth > 512) throw Error(".ltb node tree too deep");
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

Track read_track(Reader& r, const Animation& a, std::size_t nkeys, const Layout& L) {
    Track t;
    uint32_t c = a.compression;
    if (c == ANCMPRS_NONE) {
        t.vertex_anim = r.u8() != 0;
        if (t.vertex_anim) {
            for (std::size_t k = 0; k < nkeys; ++k) {
                uint32_t n = r.count(12);
                std::vector<float> fr(std::size_t(n) * 3);
                for (auto& f : fr) f = r.f32();
                t.vertex_frames.push_back(std::move(fr));
            }
        } else if (L.none_interleaved) {
            r.need(nkeys * 28);
            for (std::size_t k = 0; k < nkeys; ++k) {
                t.positions.push_back(r.vec3());
                t.rotations.push_back(r.quat());
            }
        } else {
            r.need(nkeys * 28);
            for (std::size_t k = 0; k < nkeys; ++k) t.positions.push_back(r.vec3());
            for (std::size_t k = 0; k < nkeys; ++k) t.rotations.push_back(r.quat());
        }
        return t;
    }
    if (c != ANCMPRS_REL && c != ANCMPRS_REL_16 && c != ANCMPRS_REL_16_ROT_ONLY)
        throw Error("unknown animation compression " + std::to_string(c));
    bool q16 = c == ANCMPRS_REL_16 || c == ANCMPRS_REL_16_ROT_ONLY;
    uint32_t n = r.count(6);
    if (n && L.rel_count_extra) r.u32();
    for (uint32_t i = 0; i < n; ++i) {
        if (L.rel_key_times) t.pos_times.push_back(r.u32());
        if (c == ANCMPRS_REL_16) {
            float x = r.i16(), y = r.i16(), z = r.i16();
            t.positions.push_back({x / kPos16Scale, y / kPos16Scale, z / kPos16Scale});
        } else {
            t.positions.push_back(r.vec3());
        }
    }
    n = r.count(8);
    if (n && L.rel_count_extra) r.u32();
    for (uint32_t i = 0; i < n; ++i) {
        if (L.rel_key_times) t.rot_times.push_back(r.u32());
        if (q16) {
            float x = r.i16(), y = r.i16(), z = r.i16(), w = r.i16();
            t.rotations.push_back({x / kQuat16Scale, y / kQuat16Scale, z / kQuat16Scale, w / kQuat16Scale});
        } else {
            t.rotations.push_back(r.quat());
        }
    }
    return t;
}

template <class V>
V channel(const std::vector<V>& values, const std::vector<uint32_t>& times, std::size_t key, uint32_t time,
          const V& identity, std::size_t rank) {
    if (values.empty()) return identity;
    if (values.size() == 1) return values[0];
    if (times.empty()) return values[std::min(key, values.size() - 1)];
    std::vector<std::size_t> exact;
    for (std::size_t i = 0; i < times.size() && i < values.size(); ++i)
        if (times[i] == time) exact.push_back(i);
    if (!exact.empty()) return values[exact[std::min(rank, exact.size() - 1)]];
    if (time <= times[0]) return values[0];
    std::size_t n = std::min(times.size(), values.size());
    for (std::size_t i = 1; i < n; ++i) {
        if (time <= times[i]) {
            uint32_t t0 = times[i - 1], t1 = times[i];
            double f = t1 > t0 ? double(time - t0) / double(t1 - t0) : 0.0;
            V a = values[i - 1], b = values[i];
            if (a.size() == 4) {
                double dot = 0;
                for (std::size_t k = 0; k < 4; ++k) dot += double(a[k]) * b[k];
                if (dot < 0)
                    for (auto& x : b) x = -x;
            }
            V out{};
            for (std::size_t k = 0; k < a.size(); ++k) out[k] = float(a[k] + (b[k] - a[k]) * f);
            return out;
        }
    }
    return values[n - 1];
}

Mat4 inverse(const Mat4& m) {
    // general 4x4 inverse (bind matrices may hold scale)
    double a[4][8];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 8; ++j) a[i][j] = j < 4 ? m[std::size_t(4 * i + j)] : (j - 4 == i ? 1.0 : 0.0);
    for (int c = 0; c < 4; ++c) {
        int piv = c;
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
        if (std::fabs(a[piv][c]) < 1e-12) {
            Mat4 id{};
            id[0] = id[5] = id[10] = id[15] = 1;
            return id;
        }
        if (piv != c)
            for (int j = 0; j < 8; ++j) std::swap(a[c][j], a[piv][j]);
        double p = a[c][c];
        for (int j = 0; j < 8; ++j) a[c][j] /= p;
        for (int r = 0; r < 4; ++r)
            if (r != c) {
                double f = a[r][c];
                for (int j = 0; j < 8; ++j) a[r][j] -= f * a[c][j];
            }
    }
    Mat4 out{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) out[std::size_t(4 * i + j)] = float(a[i][j + 4]);
    return out;
}

}  // namespace

std::pair<Vec3, Quat> Animation::key_transform(std::size_t node, std::size_t key) const {
    const Vec3 zero{0, 0, 0};
    const Quat ident{0, 0, 0, 1};
    if (node >= tracks.size()) return {zero, ident};
    const Track& t = tracks[node];
    if (compression == ANCMPRS_NONE) {
        if (t.vertex_anim || t.positions.empty() || t.rotations.empty()) return {zero, ident};
        return {t.positions[std::min(key, t.positions.size() - 1)], t.rotations[std::min(key, t.rotations.size() - 1)]};
    }
    uint32_t time = key < keyframes.size() ? keyframes[key].time : 0;
    std::size_t rank = 0;
    for (std::size_t k = 0; k < key && k < keyframes.size(); ++k)
        if (keyframes[k].time == time) ++rank;
    Vec3 pos = channel(t.positions, t.pos_times, key, time, zero, rank);
    Quat rot = channel(t.rotations, t.rot_times, key, time, ident, rank);
    if ((compression == ANCMPRS_REL_16 || compression == ANCMPRS_REL_16_ROT_ONLY) && !t.rotations.empty()) {
        double n = std::sqrt(double(rot[0]) * rot[0] + double(rot[1]) * rot[1] + double(rot[2]) * rot[2] +
                             double(rot[3]) * rot[3]);
        if (n == 0) n = 1;
        for (auto& x : rot) x = float(x / n);
    }
    return {pos, rot};
}

bool is_chunked(const std::vector<uint8_t>& d) {
    static const uint8_t sig[11] = {0x09, 0x00, 'L', 'T', 'B', 'H', 'e', 'a', 'd', 'e', 'r'};
    return d.size() >= 11 && std::memcmp(d.data(), sig, 11) == 0;
}

bool is_ltb(const std::vector<uint8_t>& d) {
    if (is_chunked(d)) return true;
    return d.size() >= kHeaderSize + 4 && d[0] == kD3DModelFile && (d[2] | d[3] << 8) == kD3DVersion;
}

Model read(const std::vector<uint8_t>& data) {
    if (data.size() < kHeaderSize) throw Error("too short for an .ltb");
    if (is_chunked(data))
        throw Error("a chunked .ltb (an early format the games don't load either)");
    Reader r{&data, 0};
    uint8_t type = data[0];
    uint16_t lv = uint16_t(data[2] | data[3] << 8);
    if (type == 5) throw Error("LTB file type 5 is a render style, not a model (lithtech::renderstyle reads it)");
    if (type != kD3DModelFile) throw Error("LTB file type " + std::to_string(type) + " is not a D3D model");
    if (lv != kD3DVersion) throw Error("LTB version " + std::to_string(lv) + " (only 9 is known)");
    r.pos = kHeaderSize;
    Model m;
    m.version = r.u32();
    if (m.version != 18 && (m.version < 20 || m.version > 25))
        throw Error("model file version " + std::to_string(m.version) + " (18 and 20 to 25 are known)");
    Layout L(m.version);
    r.skip(std::size_t(L.alloc_count) * 4);
    m.command_string = r.str();
    m.radius = r.f32();
    std::size_t obb = std::size_t(L.obb_floats) * 4 + 4 + (L.obb_radius ? 4 : 0);
    uint32_t nobb = r.count(obb);
    r.skip(nobb * obb);

    uint32_t np = r.count(10);
    for (uint32_t i = 0; i < np; ++i) {
        Piece p;
        p.name = r.str();
        uint32_t nlods = r.count(4);
        for (uint32_t k = 0; k < nlods; ++k) p.lod_distances.push_back(r.f32());
        r.skip(8);   // min/max lod offsets, no longer used
        for (uint32_t k = 0; k < nlods; ++k) {
            LOD lod;
            lod.texture_count = r.u32();
            for (auto& t : lod.textures) t = r.i32();
            lod.render_style = r.i32();
            if (L.render_priority) lod.render_priority = r.u8();
            int kind = int(r.u32());
            lod.mesh = read_mesh(r, kind);
            if (L.used_nodes) {
                uint8_t n = r.u8();
                r.need(n);
                lod.used_nodes.assign(data.begin() + std::ptrdiff_t(r.pos), data.begin() + std::ptrdiff_t(r.pos + n));
                r.pos += n;
            }
            p.lods.push_back(std::move(lod));
        }
        m.pieces.push_back(std::move(p));
    }

    read_node(r, m.nodes, -1, 0);
    std::size_t nn = m.nodes.size();
    uint32_t nws = r.count(6);
    for (uint32_t i = 0; i < nws; ++i) {
        r.str();
        uint32_t n = r.count(4);
        r.skip(std::size_t(n) * 4);
    }

    uint32_t nchild = r.count(2);
    if (nchild == 0) throw Error("no child models (the model itself counts as one)");
    if (L.child_relations) {
        for (uint32_t i = 0; i < nchild; ++i) {
            if (i) m.child_models.push_back(r.str());
            r.u32();
            r.skip(nn * 28);
        }
    } else {
        for (uint32_t i = 1; i < nchild; ++i) m.child_models.push_back(r.str());
    }

    uint32_t na = r.count(26);
    for (uint32_t i = 0; i < na; ++i) {
        Animation a;
        a.dims = r.vec3();
        a.name = r.str();
        a.compression = r.u32();
        a.interpolation_ms = r.u32();
        uint32_t nk = r.count(6);
        for (uint32_t k = 0; k < nk; ++k) {
            Keyframe kf;
            kf.time = r.u32();
            kf.command = r.str();
            a.keyframes.push_back(std::move(kf));
        }
        for (std::size_t n = 0; n < nn; ++n) a.tracks.push_back(read_track(r, a, a.keyframes.size(), L));
        m.animations.push_back(std::move(a));
    }

    uint32_t ns = r.count(46);
    for (uint32_t i = 0; i < ns; ++i) {
        Socket s;
        s.node = r.u32();
        s.name = r.str();
        s.rotation = r.quat();
        s.position = r.vec3();
        s.scale = r.vec3();
        m.sockets.push_back(std::move(s));
    }

    for (uint32_t i = 0; i < nchild; ++i) {
        uint32_t nb = r.count(26);
        for (uint32_t k = 0; k < nb; ++k) {
            r.str();
            r.skip(24);
        }
    }
    if (r.pos != data.size())
        throw Error(std::to_string(data.size() - r.pos) + " bytes left after the model");
    return m;
}

std::vector<std::vector<std::pair<uint32_t, float>>> vertex_weights(const Mesh& m) {
    std::vector<std::vector<std::pair<uint32_t, float>>> out(m.vertices.size());
    if (m.kind == RIGID_MESH || m.kind == VA_MESH) {
        for (auto& w : out) w.emplace_back(m.bone_effector, 1.0f);
        return out;
    }
    if (m.kind != SKEL_MESH) return out;
    if (m.use_matrix_palettes) {
        uint32_t nb = m.max_bones_per_vert;
        for (std::size_t vi = 0; vi < m.vertices.size(); ++vi) {
            const Vertex& v = m.vertices[vi];
            float rest = 1.0f;
            for (uint32_t i = 0; i < nb && i < 4; ++i) {
                uint32_t b = v.bone_indices[i];
                uint32_t node = (m.reindexed_bones_flag && b < m.reindexed_bones.size()) ? m.reindexed_bones[b] : b;
                float w;
                if (i + 1 < nb) {
                    w = i < v.weights.size() ? v.weights[i] : 0.0f;
                    rest -= w;
                } else {
                    w = rest;
                }
                out[vi].emplace_back(node, w);
            }
        }
        return out;
    }
    for (const BoneSet& bs : m.bone_sets) {
        std::vector<uint32_t> bones;
        for (uint8_t b : bs.bones) {
            if (b == 0xFF) break;
            bones.push_back(b);
        }
        std::size_t k = bones.size();
        std::size_t end = std::min<std::size_t>(std::size_t(bs.first_vertex) + bs.vertex_count, m.vertices.size());
        for (std::size_t vi = bs.first_vertex; vi < end; ++vi) {
            const Vertex& v = m.vertices[vi];
            float rest = 1.0f;
            for (std::size_t i = 0; i < k; ++i) {
                float w;
                if (i + 1 < k) {
                    w = i < v.weights.size() ? v.weights[i] : 0.0f;
                    rest -= w;
                } else {
                    w = rest;
                }
                out[vi].emplace_back(bones[i], w);
            }
        }
    }
    return out;
}

abc::Model to_abc(const Model& m) {
    abc::Model o;
    o.version = m.version;
    o.command_string = m.command_string;
    o.internal_radius = m.radius;
    std::vector<Mat4> inv;
    for (const Node& n : m.nodes) {
        abc::Node an;
        an.name = n.name;
        an.index = n.index;
        an.flags = n.flags;
        an.bind = n.bind;
        an.parent = n.parent;
        o.nodes.push_back(an);
        inv.push_back(inverse(n.bind));
    }
    std::size_t most = 0;
    for (const Piece& p : m.pieces) {
        if (p.lods.size() > most) {
            most = p.lods.size();
            o.lod_distances.assign(p.lod_distances.begin() + (p.lod_distances.empty() ? 0 : 1), p.lod_distances.end());
        }
        abc::Piece ap;
        ap.name = p.name;
        int tex = p.lods.empty() ? 0 : p.lods[0].textures[0];
        ap.texture_index = uint16_t(tex >= 0 ? tex : 0);
        for (const LOD& lod : p.lods) {
            const Mesh& me = lod.mesh;
            abc::LOD al;
            auto weights = vertex_weights(me);
            al.vertices.resize(me.vertices.size());
            for (std::size_t vi = 0; vi < me.vertices.size(); ++vi) {
                const Vertex& v = me.vertices[vi];
                abc::Vertex& av = al.vertices[vi];
                av.location = v.position;
                av.normal = v.normal;
                av.replacement = 0;
                for (auto [node, w] : weights[vi]) {
                    if (node >= inv.size()) continue;
                    const Mat4& M = inv[node];
                    abc::Weight aw;
                    aw.node = node;
                    aw.bias = w;
                    for (int r = 0; r < 3; ++r)
                        aw.location[std::size_t(r)] =
                            w * (M[std::size_t(4 * r)] * v.position[0] + M[std::size_t(4 * r + 1)] * v.position[1] +
                                 M[std::size_t(4 * r + 2)] * v.position[2] + M[std::size_t(4 * r + 3)]);
                    av.weights.push_back(aw);
                }
            }
            for (std::size_t i = 0; i + 2 < me.indices.size(); i += 3) {
                abc::Triangle t{};
                bool ok = true;
                for (int c = 0; c < 3; ++c) {
                    uint16_t vi = me.indices[i + std::size_t(c)];
                    if (vi >= me.vertices.size()) {
                        ok = false;
                        break;
                    }
                    t.indices[c] = vi;
                    const auto& uvs = me.vertices[vi].uvs;
                    t.uv[c][0] = uvs.empty() ? 0.0f : uvs[0][0];
                    t.uv[c][1] = uvs.empty() ? 0.0f : uvs[0][1];
                }
                if (ok) al.triangles.push_back(t);
            }
            ap.lods.push_back(std::move(al));
        }
        o.pieces.push_back(std::move(ap));
    }
    o.child_models = m.child_models;
    for (const Animation& a : m.animations) {
        abc::Animation aa;
        aa.name = a.name;
        aa.dims = a.dims;
        aa.interpolation_ms = a.interpolation_ms;
        for (const Keyframe& k : a.keyframes) aa.keyframes.push_back({k.time, k.command});
        aa.transforms.resize(m.nodes.size());
        for (std::size_t n = 0; n < m.nodes.size(); ++n) {
            aa.transforms[n].resize(a.keyframes.size());
            for (std::size_t k = 0; k < a.keyframes.size(); ++k) {
                auto [p, q] = a.key_transform(n, k);
                aa.transforms[n][k] = {p, q};
            }
        }
        o.animations.push_back(std::move(aa));
    }
    for (const Socket& s : m.sockets) o.sockets.push_back({s.node, s.name, s.rotation, s.position});
    return o;
}

}  // namespace lithtech::ltb
