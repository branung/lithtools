// Reads every .SPR in the given .rez files (one game's archives together)
// and looks up each frame among the .DTX files of all of them, as the
// engine finds them across the game's resource files. Prints the failures
// and frames found in none of the archives, then counts: frame rates,
// flags, trailing bytes.
//   spr_verify [--list] <file.rez>...
#include <lithtech/rez.hpp>
#include <lithtech/spr.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace lithtech;

static std::string key(std::string p) {
    for (auto& c : p) c = c == '/' ? '\\' : char(std::toupper(uint8_t(c)));
    return p;
}

int main(int argc, char** argv) {
    bool list = false;
    std::vector<std::unique_ptr<rez::Archive>> archives;
    std::vector<std::string> names;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--list")) { list = true; continue; }
        try {
            archives.push_back(rez::Archive::open(argv[i]));
            names.push_back(argv[i]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s: %s\n", argv[i], e.what());
            return 2;
        }
    }
    std::set<std::string> textures;
    std::vector<std::pair<std::size_t, const rez::Resource*>> sprites;
    for (std::size_t a = 0; a < archives.size(); ++a) {
        std::function<void(const rez::Directory&)> walk = [&](const rez::Directory& d) {
            for (auto& r : d.resources()) {
                if (r->type_string() == "DTX") textures.insert(key(r->path('\\')));
                else if (r->type_string() == "SPR") sprites.emplace_back(a, r.get());
            }
            for (auto& s : d.dirs()) walk(*s);
        };
        walk(archives[a]->root());
    }
    int ok = 0, bad = 0, frames = 0, missing = 0, with_missing = 0, trailing = 0;
    std::map<std::string, int> errors, rates, flags;
    for (auto& [a, r] : sprites) {
        try {
            auto s = spr::read(r->read());
            ++ok;
            rates[std::to_string(s.frame_rate)]++;
            flags["keyed " + std::to_string(s.keyed) + ", translucent " + std::to_string(s.translucent)]++;
            if (s.trailing) {
                ++trailing;
                std::printf("TRAILING %s %s: %zu bytes\n", names[a].c_str(), r->path('/').c_str(), s.trailing);
            }
            int m = 0;
            for (auto& f : s.frames) {
                ++frames;
                if (!textures.count(key(f))) {
                    ++m;
                    std::printf("MISSING %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), f.c_str());
                }
            }
            missing += m;
            with_missing += m != 0;
            if (list)
                std::printf("%s\t%s\t%zu frames\t%u fps\n", names[a].c_str(), r->path('/').c_str(), s.frames.size(),
                            s.frame_rate);
        } catch (const std::exception& e) {
            ++bad;
            errors[e.what()]++;
            std::printf("FAIL %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), e.what());
        }
    }
    std::printf("%zu sprites: %d read, %d failed; %d frames, %d not in the archives (in %d sprites); %d with trailing bytes\n",
                sprites.size(), ok, bad, frames, missing, with_missing, trailing);
    for (auto& [k, n] : rates) std::printf("  rate %-6s %d\n", k.c_str(), n);
    for (auto& [k, n] : flags) std::printf("  %s: %d\n", k.c_str(), n);
    for (auto& [k, n] : errors) std::printf("  error %d x %s\n", n, k.c_str());
    return bad ? 1 : 0;
}
