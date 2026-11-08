// Reads every .abc v6 under a directory (Shogo, Blood 2, KISS) with
// lithtech::abc6 and checks what abc6::to_abc makes of it: skin(bind) gives
// back every vertex's stored position, the first key of the first
// animation is the bind pose, and abc::animate at that key leaves every
// vertex where the bind put it.
//   abc6_verify <dir> [--dump <out.txt>]
// --dump writes every LOD 0 vertex of every model posed at every keyframe
// and halfway between each pair, for a comparison with another reader
// ("model <path>", "pose <anim> <key> <t ms>", then "<x> <y> <z>" lines).
#include <lithtech/abc6.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using namespace lithtech;

static std::vector<abc::Vec3> posed(const abc::Model& m, const abc::Animation& a, float t) {
    auto M = abc::pose(m, a, t);
    abc::LOD lod = m.pieces[0].lods[0];
    abc::animate(m, a, t, lod);
    std::vector<abc::Vec3> out;
    for (auto& v : lod.vertices) out.push_back(abc::skin(v, M));
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: abc6_verify <dir> [--dump <out.txt>]\n"); return 2; }
    FILE* dump = nullptr;
    for (int i = 2; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--dump")) dump = std::fopen(argv[i + 1], "w");
    std::vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(argv[1])) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        for (auto& c : ext) c = char(std::tolower(c));
        if (ext == ".abc") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    int v6 = 0, other = 0, failed = 0, pose_match = 0, rest_match = 0, animated_models = 0;
    long verts = 0, skin_ok = 0, poses = 0;
    double worst_skin = 0, worst_pose = 0, worst_rest = 0;
    for (auto& path : files) {
        std::ifstream f(path, std::ios::binary);
        std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!abc6::is_model(d)) { ++other; continue; }
        ++v6;
        abc::Model m;
        try {
            m = abc6::to_abc(abc6::read(d));
        } catch (const std::exception& ex) {
            std::printf("FAIL %s: %s\n", path.u8string().c_str(), ex.what());
            ++failed;
            continue;
        }
        auto bind = abc::bind_pose(m);
        for (auto& v : m.pieces[0].lods[0].vertices) {
            ++verts;
            auto s = abc::skin(v, bind);
            float dd = 0;
            for (int i = 0; i < 3; ++i) dd = std::max(dd, std::fabs(s[i] - v.location[i]));
            if (dd < 1e-3f) ++skin_ok;
            worst_skin = std::max(worst_skin, double(dd));
        }
        if (!m.animations.empty() && !m.animations[0].keyframes.empty()) {
            const abc::Animation& a = m.animations[0];
            float t0 = float(a.keyframes[0].time);
            auto P = abc::pose(m, a, t0);
            double mw = 0;
            for (std::size_t n = 0; n < P.size(); ++n)
                for (int i = 0; i < 12; ++i) mw = std::max(mw, double(std::fabs(P[n][i] - m.nodes[n].bind[i])));
            worst_pose = std::max(worst_pose, mw);
            if (mw < 1e-4) ++pose_match;
            auto at0 = posed(m, a, t0);
            double rw = 0;
            for (std::size_t vi = 0; vi < at0.size(); ++vi)
                for (int i = 0; i < 3; ++i)
                    rw = std::max(rw, double(std::fabs(at0[vi][i] - m.pieces[0].lods[0].vertices[vi].location[i])));
            worst_rest = std::max(worst_rest, rw);
            if (rw < 1e-3) ++rest_match;
        }
        if (abc::has_vertex_animation(m)) ++animated_models;
        if (dump) {
            std::fprintf(dump, "model %s\n", fs::relative(path, argv[1]).generic_u8string().c_str());
            for (std::size_t ai = 0; ai < m.animations.size(); ++ai) {
                const auto& a = m.animations[ai];
                for (std::size_t k = 0; k < a.keyframes.size(); ++k) {
                    std::vector<std::pair<int, float>> times = {{0, float(a.keyframes[k].time)}};
                    if (k + 1 < a.keyframes.size() && a.keyframes[k + 1].time > a.keyframes[k].time + 1)
                        times.push_back({1, (a.keyframes[k].time + a.keyframes[k + 1].time) / 2.0f});
                    for (auto [mid, t] : times) {
                        std::fprintf(dump, "pose %zu %zu%s %.9g\n", ai, k, mid ? "+" : "", t);
                        for (auto& p : posed(m, a, t)) std::fprintf(dump, "%.9g %.9g %.9g\n", p[0], p[1], p[2]);
                        ++poses;
                    }
                }
            }
        }
    }
    if (dump) std::fclose(dump);
    std::printf("%d v6 files (%d other .abc skipped), %d failed; %ld vertices: skin(bind) gives back %ld (worst %.3g); "
                "first key of the first animation is the bind pose in %d of %d (worst %.3g), and leaves every "
                "vertex at its bind position in %d (worst %.3g); %d models with vertex animation; %ld poses dumped\n",
                v6, other, failed, verts, skin_ok, worst_skin, pose_match, v6 - failed, worst_pose, rest_match,
                worst_rest, animated_models, poses);
    return failed || !v6 || skin_ok != verts ? 1 : 0;
}
