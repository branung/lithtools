// Reads every render style (an .LTB of file type 5) in the given .rez
// files and counts what they hold: styles per file, passes per style,
// passes with Direct3D options and vertex shaders, trailing bytes; and
// the other .LTB files by type. --dump prints each style.
//   renderstyle_verify [--dump] <file.rez>...
#include <lithtech/renderstyle.hpp>
#include <lithtech/rez.hpp>

#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>

using namespace lithtech;

int main(int argc, char** argv) {
    bool dump = false;
    int total = 0, ok = 0, bad = 0, trailing = 0;
    std::map<std::string, int> counts, errors, ltb_types;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dump")) { dump = true; continue; }
        std::unique_ptr<rez::Archive> a;
        try {
            a = rez::Archive::open(argv[i]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s: %s\n", argv[i], e.what());
            return 2;
        }
        std::function<void(const rez::Directory&)> walk = [&](const rez::Directory& d) {
            for (auto& r : d.resources()) {
                if (r->type_string() != "LTB") continue;
                auto data = r->read();
                ltb_types[data.empty() ? "empty" : "type " + std::to_string(data[0])]++;
                if (data.empty() || data[0] != renderstyle::kFileType) continue;
                ++total;
                try {
                    auto f = renderstyle::read(data);
                    ++ok;
                    counts[std::to_string(f.styles.size()) + " styles in a file"]++;
                    for (auto& s : f.styles) {
                        counts[std::to_string(s.passes.size()) + " passes in a style"]++;
                        for (auto& p : s.passes) {
                            if (p.has_d3d) counts["passes with RSD3DRenderPass"]++;
                            if (p.has_d3d && p.d3d.use_vertex_shader) counts["passes with a vertex shader"]++;
                            if (p.bump_env_map) counts["passes with bump env mapping"]++;
                            counts["blend " + renderstyle::blend_name(p.blend)]++;
                            for (auto& st : p.stages)
                                if (st.colour_op) counts["stage " + renderstyle::colour_op_name(st.colour_op)]++;
                        }
                    }
                    if (f.trailing) {
                        ++trailing;
                        std::printf("TRAILING %s %s: %zu bytes\n", argv[i], r->path('/').c_str(), f.trailing);
                    }
                    if (dump) {
                        std::printf("== %s %s: %zu styles, reserved %u %u %u %u\n", argv[i], r->path('/').c_str(), f.styles.size(),
                                    f.reserved1, f.reserved[0], f.reserved[1], f.reserved[2]);
                        for (auto& s : f.styles) {
                            std::printf("  style: %zu passes, diffuse %g %g %g %g\n", s.passes.size(), s.material.diffuse.r,
                                        s.material.diffuse.g, s.material.diffuse.b, s.material.diffuse.a);
                            for (auto& p : s.passes) {
                                std::printf("    pass: %s %s %s", renderstyle::blend_name(p.blend).c_str(),
                                            renderstyle::z_buffer_name(p.z_buffer).c_str(), renderstyle::cull_name(p.cull).c_str());
                                if (p.has_d3d) std::printf(" vs %d (%zu blocks)", p.d3d.use_vertex_shader, p.d3d.blocks.size());
                                std::printf("\n");
                                for (auto& st : p.stages)
                                    if (st.colour_op)
                                        std::printf("      %s %s(%s, %s) %s\n", renderstyle::texture_param_name(st.texture).c_str(),
                                                    renderstyle::colour_op_name(st.colour_op).c_str(),
                                                    renderstyle::colour_arg_name(st.colour_arg1).c_str(),
                                                    renderstyle::colour_arg_name(st.colour_arg2).c_str(),
                                                    renderstyle::uv_source_name(st.uv_source).c_str());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    ++bad;
                    errors[e.what()]++;
                    std::printf("FAIL %s %s: %s\n", argv[i], r->path('/').c_str(), e.what());
                }
            }
            for (auto& s : d.dirs()) walk(*s);
        };
        walk(a->root());
    }
    std::printf("%d render styles: %d read, %d failed; %d files with trailing bytes\n", total, ok, bad, trailing);
    for (auto& [k, n] : ltb_types) std::printf("  .ltb %s: %d\n", k.c_str(), n);
    for (auto& [k, n] : counts) std::printf("  %s: %d\n", k.c_str(), n);
    for (auto& [k, n] : errors) std::printf("  error %d x %s\n", n, k.c_str());
    return bad ? 1 : 0;
}
