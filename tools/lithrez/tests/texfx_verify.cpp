// Reads every texture script (.TFS) and texture effect group (.TFG) in
// the given .rez files (one game's archives together): each script's
// problems by lithtech::texscript::check, each group's scripts looked up
// among the archives' scripts (the engine finds them from the resource
// root) and its values counted against the script's parameters. --dump
// prints every script decompiled and disassembled.
//   texfx_verify [--dump] <file.rez>...
#include <lithtech/rez.hpp>
#include <lithtech/texscript.hpp>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace lithtech;

static std::string key(std::string p) {
    for (auto& c : p) c = c == '/' ? '\\' : char(std::toupper(uint8_t(c)));
    while (!p.empty() && p[0] == '\\') p.erase(0, 1);
    return p;
}

int main(int argc, char** argv) {
    bool dump = false;
    std::vector<std::unique_ptr<rez::Archive>> archives;
    std::vector<std::string> names;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dump")) { dump = true; continue; }
        try {
            archives.push_back(rez::Archive::open(argv[i]));
            names.push_back(argv[i]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s: %s\n", argv[i], e.what());
            return 2;
        }
    }
    std::vector<std::pair<std::size_t, const rez::Resource*>> tfs, tfg;
    for (std::size_t a = 0; a < archives.size(); ++a) {
        std::function<void(const rez::Directory&)> walk = [&](const rez::Directory& d) {
            for (auto& r : d.resources()) {
                if (r->type_string() == "TFS") tfs.emplace_back(a, r.get());
                else if (r->type_string() == "TFG") tfg.emplace_back(a, r.get());
            }
            for (auto& s : d.dirs()) walk(*s);
        };
        walk(archives[a]->root());
    }
    int bad = 0, sok = 0, sbad = 0, problems = 0, gok = 0, gbad = 0, missing = 0, mismatched = 0;
    std::map<std::string, texscript::Script> scripts;
    std::map<std::string, int> inputs, stage_kinds, channels;
    for (auto& [a, r] : tfs) {
        try {
            auto s = texscript::read(r->read());
            ++sok;
            scripts[key(r->path('\\'))] = s;
            inputs[std::string(texscript::input_name(s.input) ? texscript::input_name(s.input) : "?") + " -> " +
                   (texscript::output_name(s.output) ? texscript::output_name(s.output) : "?")]++;
            for (auto& p : texscript::check(s)) {
                ++problems;
                std::printf("PROBLEM %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), p.c_str());
            }
            if (s.trailing) std::printf("TRAILING %s %s: %zu bytes\n", names[a].c_str(), r->path('/').c_str(), s.trailing);
            if (dump) {
                std::printf("== %s %s\n%s", names[a].c_str(), r->path('/').c_str(), texscript::decompile(s).c_str());
                for (auto& l : texscript::disassemble(s)) std::printf("    %s\n", l.c_str());
            }
        } catch (const std::exception& e) {
            ++sbad;
            std::printf("FAIL %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), e.what());
        }
    }
    for (auto& [a, r] : tfg) {
        try {
            auto g = texscript::read_group(r->read());
            ++gok;
            if (g.trailing) std::printf("TRAILING %s %s: %zu bytes\n", names[a].c_str(), r->path('/').c_str(), g.trailing);
            if (g.stage_count != texscript::kMaxStages)
                std::printf("NOTE %s %s: %u stages stored\n", names[a].c_str(), r->path('/').c_str(), g.stage_count);
            for (std::size_t i = 0; i < g.stages.size(); ++i) {
                const auto& st = g.stages[i];
                const char* t = texscript::stage_type_name(st.type);
                stage_kinds[std::string(t ? t : "unknown") + (st.type ? std::string(", ") + (texscript::channel_name(st.channel) ? texscript::channel_name(st.channel) : "?") : "")]++;
                if (st.type != texscript::STAGE_EVALUATED) continue;
                auto it = scripts.find(key(st.script));
                if (it == scripts.end()) {
                    ++missing;
                    std::printf("MISSING %s %s stage %zu: %s\n", names[a].c_str(), r->path('/').c_str(), i, st.script.c_str());
                    continue;
                }
                if (st.defaults.size() < it->second.params.size()) {
                    ++mismatched;
                    std::printf("FEWER VALUES %s %s stage %zu: %zu values, %zu parameters\n", names[a].c_str(),
                                r->path('/').c_str(), i, st.defaults.size(), it->second.params.size());
                }
            }
        } catch (const std::exception& e) {
            ++gbad;
            std::printf("FAIL %s %s: %s\n", names[a].c_str(), r->path('/').c_str(), e.what());
        }
    }
    bad = sbad + gbad;
    std::printf("%zu scripts: %d read, %d failed, %d problems; %zu groups: %d read, %d failed; %d stage scripts not "
                "in the archives, %d stages with fewer values than parameters\n",
                tfs.size(), sok, sbad, problems, tfg.size(), gok, gbad, missing, mismatched);
    for (auto& [k, n] : inputs) std::printf("  script %s: %d\n", k.c_str(), n);
    for (auto& [k, n] : stage_kinds) std::printf("  stage %s: %d\n", k.c_str(), n);
    return bad ? 1 : 0;
}
