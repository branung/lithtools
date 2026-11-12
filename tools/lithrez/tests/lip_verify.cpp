// Reads every .LIP in the given .rez files and checks it against the voice
// line of the same name beside it (<n>.WAV in the same folder, in any of
// the archives given): whether there is one, and how the lip-sync's length
// (keyframes / 30 s) compares with the sound's. Also counts node sets,
// rotations that aren't unit length and trailing bytes. --no-sound skips
// decoding the sounds.
//   lip_verify [--no-sound] <file.rez>...
#include <lithtech/lip.hpp>
#include <lithtech/rez.hpp>
#include <lithtech/sound.hpp>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace lithtech;

static std::string upper(std::string p) {
    for (auto& c : p) c = c == '/' ? '\\' : char(std::toupper(uint8_t(c)));
    return p;
}

int main(int argc, char** argv) {
    bool sound = true;
    std::vector<std::unique_ptr<rez::Archive>> archives;
    std::vector<std::string> names;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--no-sound")) { sound = false; continue; }
        try {
            archives.push_back(rez::Archive::open(argv[i]));
            names.push_back(argv[i]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s: %s\n", argv[i], e.what());
            return 2;
        }
    }
    std::map<std::string, const rez::Resource*> wavs;
    std::vector<std::pair<std::size_t, const rez::Resource*>> lips;
    for (std::size_t a = 0; a < archives.size(); ++a) {
        std::function<void(const rez::Directory&)> walk = [&](const rez::Directory& d) {
            for (auto& r : d.resources()) {
                if (r->type_string() == "WAV") wavs.emplace(upper(r->parent()->path('\\') + "\\" + r->name), r.get());
                else if (r->type_string() == "LIP") lips.emplace_back(a, r.get());
            }
            for (auto& s : d.dirs()) walk(*s);
        };
        walk(archives[a]->root());
    }
    int ok = 0, bad = 0, trailing = 0, no_wav = 0, not_unit = 0, compared = 0, longer = 0, shorter = 0;
    double worst_over = 0, worst_under = 0;
    std::map<std::string, int> errors, node_sets, versions;
    std::map<int, int> diff_hist;   // lip - wav, in whole keyframes, clamped to +-10
    for (auto& [a, r] : lips) {
        try {
            auto l = lip::read(r->read());
            ++ok;
            std::string set;
            for (auto& n : l.nodes) set += (set.empty() ? "" : " ") + n.name;
            node_sets[std::to_string(l.nodes.size()) + ": " + set]++;
            if (l.trailing) {
                ++trailing;
                std::printf("TRAILING %s %s: %zu bytes\n", names[a].c_str(), r->path('/').c_str(), l.trailing);
            }
            bool unit = true;
            for (auto& n : l.nodes)
                for (auto& k : n.keys) {
                    float m = 0;
                    for (float c : k.rotation) m += c * c;
                    if (std::fabs(m - 1) > 1e-3f) unit = false;
                }
            not_unit += !unit;
            auto w = wavs.find(upper(r->parent()->path('\\') + "\\" + r->name));
            if (w == wavs.end()) {
                ++no_wav;
                std::printf("NO WAV %s %s\n", names[a].c_str(), r->path('/').c_str());
                continue;
            }
            if (!sound) continue;
            auto data = w->second->read();
            double secs = sound::decode(data.data(), data.size()).seconds();
            double d = l.length_seconds() - secs;
            ++compared;
            if (d > 0) ++longer, worst_over = std::max(worst_over, d);
            else ++shorter, worst_under = std::min(worst_under, d);
            int k = int(std::lround(d * lip::kKeyframesPerSecond));
            diff_hist[std::max(-10, std::min(10, k))]++;
        } catch (const std::exception& e) {
            ++bad;
            errors[e.what()]++;
            std::printf("FAIL %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), e.what());
        }
    }
    std::printf("%zu lip-sync files: %d read, %d failed; %d without a voice line; %d with rotations not unit length; "
                "%d with trailing bytes\n",
                lips.size(), ok, bad, no_wav, not_unit, trailing);
    for (auto& [k, n] : node_sets) std::printf("  nodes %s: %d\n", k.c_str(), n);
    if (compared) {
        std::printf("  against the voice line (%d): %d longer (at most %.3f s), %d shorter or equal (at most %.3f s)\n",
                    compared, longer, worst_over, shorter, -worst_under);
        for (auto& [k, n] : diff_hist)
            std::printf("    lip - wav %s%d keyframes: %d\n", k == 10 ? ">=" : k == -10 ? "<=" : "", k, n);
    }
    for (auto& [k, n] : errors) std::printf("  error %d x %s\n", n, k.c_str());
    return bad ? 1 : 0;
}
