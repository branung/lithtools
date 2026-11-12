// lithrez: LithTech's resource compiler, cross-platform.
//
// The classic commands behave and print as LITHREZ 1.10 does (a superset of
// 1.00): "lithrez cv foo.rez dir *.dtx;*.ltb". The freshen, sort and info
// commands that the original kept for its ZMgr build are enabled, and there
// are extra commands for debugging and build scripts (see `lithrez help`).

#include <cstdlib>
#include <cctype>
#include <lithtech/butes.hpp>
#include <lithtech/ps2rez.hpp>
#include <lithtech/rez.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace fs = std::filesystem;
using namespace lithtech;
using rez::Archive;
using rez::Directory;
using rez::Resource;

namespace {

#ifndef LITHREZ_VERSION
#define LITHREZ_VERSION "dev"
#endif
const char* kVersion = LITHREZ_VERSION;

void classic_help() {
    std::printf("\nLITHREZ %s (lithtools, cross-platform; compatible with LITHREZ 1.00 and 1.10)\n", kVersion);
    std::printf("\nUsage: LITHREZ <commands> <rez file name> [parameters]\n");
    std::printf("\nCommands: c <rez file name> <root directory to read> [extension[;]] - Create");
    std::printf("\n          v <rez file name>                          - View");
    std::printf("\n          x <rez file name> <directory to output to> - Extract");
    std::printf("\n          f <rez file name> <directory to read>      - Freshen (add or replace)");
    std::printf("\n          s <rez file name>                          - Sort (compact)");
    std::printf("\n          i <rez file name>                          - Information");
    std::printf("\nOption:   v                                          - Verbose");
    std::printf("\nOption:   z                                          - Warn zero len");
    std::printf("\nOption:   l                                          - Lower case ok\n");
    std::printf("\nExample: lithrez cv foo.rez c:\\foo *.ltb;*.dat;*.dtx");
    std::printf("\n         (would create rez file foo.rez from the contents of the");
    std::printf("\n          directory \"c:\\foo\" where files with extensions ltb dat and");
    std::printf("\n          dtx are added, the verbose option would be turned on)\n");
    std::printf("\nRun \"lithrez help\" for the extra commands and options.\n\n");
}

void extra_help() {
    std::printf(R"(lithrez %s: LithTech resource (.rez) tool

Classic commands (as LITHREZ 1.00/1.10; options are letters in the command):
  lithrez c[v][z][l] <rez> <dir> [*.ext;*.ext]  create from a directory tree
  lithrez v <rez>                               view
  lithrez x[v] <rez> <dir>                      extract everything
  lithrez f[v] <rez> <dir>                      freshen: add or replace files
  lithrez s <rez>                               sort/compact (keeps a .BAK)
  lithrez i <rez>                               is-sorted information

Extra commands:
  lithrez list <rez> [--long] [--json] [filter]   every resource, one per line
  lithrez header <rez> [--json]                   the header fields
  lithrez dump <rez>                              directory blocks with offsets
  lithrez verify <rez>                            check the structure
  lithrez cat <rez> <path> [--text]               a resource's bytes to stdout (--text: a
                                                  PS2 .but attribute file as text)
  lithrez get <rez> <path>... -o <dir>            extract chosen files/directories
  lithrez add <rez> <dir in rez> <file|dir>...    add or replace (creates <rez>)
  lithrez rm <rez> <path>...                      remove files or directories
  lithrez mkdir <rez> <path>                      add an empty directory
  lithrez diff <a.rez> <b.rez> [--data]           compare two archives
  lithrez roundtrip <rez> <out.rez>               read and write back unchanged
  lithrez tables <ps2 rez>                        PS2: each table's layout

PS2 archives (LITH_PS2.REZ, or a PS2 disc image holding one) work with v, x,
list, header, dump, verify, cat, get, diff and roundtrip (which rebuilds the
archive from its tables). Their files are named from a list of recovered
names; the rest are _UNNAMED/<hash>.<ext>. add, rm and f edit them in place
(experimental: checked against the IOP module's code, but not on a PS2).
  --names <file>        more PS2 names (one per line, or "HASH<tab>name")

Options for create/freshen/add:
  --time <seconds>      stamp this time on everything (default: now, or
                        SOURCE_DATE_EPOCH if set) for reproducible builds
  --title <text>        the header's second line (default "LithTech
                        Resource File"; "" for Shogo/Blood 2 style)
  --compat 1.00|1.02|1.10
                        which LithRez to match: 1.00/1.01 (NOLF, AvP2;
                        the default) number resources, 1.02 and later
                        (NOLF2, Contract J.A.C.K., Tron) write 0 IDs
  --lower               keep the case of names (as option l)
  --ext "*.dtx;*.ltb"   only these extensions (as 1.10's last parameter)
  --order rezmgr|stored entry order in directory blocks for writes other than
                        create (default stored; create always uses rezmgr)
)",
                kVersion);
}

void out(const std::string& s) { std::fputs(s.c_str(), stdout); }

void notify(const rez::Report& r) {
    if (r.errors > 0) {
        if (r.errors == 1) std::printf("\n%i ERROR HAS OCCURED!!!!!!!\n", r.errors);
        else std::printf("\n%i ERRORS HAVE OCCURED!!!!!!!\n", r.errors);
    }
    if (r.warnings > 0) {
        if (r.warnings == 1) std::printf("\n%i WARNING HAS OCCURED!!!!!!!\n", r.warnings);
        else std::printf("\n%i WARNINGS HAVE OCCURED!!!!!!!\n", r.warnings);
    }
}

bool has_flag(const std::string& cmd, char f) {
    for (char c : cmd)
        if (std::toupper(static_cast<unsigned char>(c)) == f) return true;
    return false;
}

std::string pad(const std::string& s, std::size_t w) { return s.size() >= w ? s : s + std::string(w - s.size(), ' '); }

void view_dir(const Directory& d, rez::Report& rep) {
    std::printf("\nDirectory %s :\n", d.name.c_str());
    rep.dirs++;
    for (Resource* r : d.resources_rezmgr_order()) {
        std::printf("  Type = %s Name = %s Size = %-8i ID = %-8i\n", pad(r->type_string(), 4).c_str(),
                    pad(r->name, 12).c_str(), int(r->size), int(r->id));
        rep.resources++;
    }
    for (Directory* s : d.dirs_rezmgr_order()) view_dir(*s, rep);
}

std::unique_ptr<Archive> open_checked(const std::string& path, rez::Report& rep) {
    try {
        auto a = Archive::open(path);
        if (!rez::looks_like_lithtech(*a)) {
            rep.error("Not a LithTech resource file!");
            return nullptr;
        }
        return a;
    } catch (const std::exception& e) {
        rep.error(e.what());
        return nullptr;
    }
}

unsigned long file_size(const std::string& p) {
    std::error_code ec;
    auto s = fs::file_size(fs::u8path(p), ec);
    return ec ? 0 : (unsigned long)s;
}

// ------------------------------------------------------------------------
// Classic LithRez

int classic(int argc, char** argv) {
    std::string cmd = argv[1];
    std::string rezfile = argv[2];
    const char* target = argc > 3 ? argv[3] : nullptr;
    std::string exts = argc > 4 ? argv[4] : "*.*";

    char command = 'I';
    if (has_flag(cmd, 'V')) command = 'V';
    if (has_flag(cmd, 'X')) command = 'X';
    if (has_flag(cmd, 'C')) command = 'C';
    if (has_flag(cmd, 'F')) command = 'F';
    if (has_flag(cmd, 'S')) command = 'S';
    // 1.00 read the command from the first letter; 1.10 lets any letter
    // name it. Honour the first letter when it is a command.
    char first = char(std::toupper(static_cast<unsigned char>(cmd[0])));
    if (std::strchr("VXCFSI", first)) command = first;

    rez::Report rep;
    rep.out = out;
    bool verbose = cmd.size() > 1 && has_flag(cmd.substr(1), 'V');
    rez::CreateOptions co;
    co.verbose = verbose;
    co.warn_zero_length = has_flag(cmd.substr(1), 'Z');
    co.lower_case_ok = has_flag(cmd.substr(1), 'L');
    co.extensions = exts;
    if (const char* t = std::getenv("LITHREZ_TITLE")) co.user_title = std::string(t) == " " ? "" : t;
    if (const char* c = std::getenv("LITHREZ_COMPAT")) co.zero_ids = std::string(c) != "1.00" && std::string(c) != "1.01";

    if ((command == 'X' || command == 'C' || command == 'F') && !target) {
        classic_help();
        return 1;
    }

    switch (command) {
        case 'X': {
            auto a = open_checked(rezfile, rep);
            if (!a) break;
            std::printf("\nExtracting rez file %s to directory %s\n", rezfile.c_str(), target);
            rez::extract(a->root(), target, rep, verbose);
            if (verbose) std::printf("\n");
            std::printf("Finished extracting %i directories %i resources\n", rep.dirs, rep.resources);
            notify(rep);
            break;
        }
        case 'V': {
            auto a = open_checked(rezfile, rep);
            if (!a) break;
            std::printf("\nView resource file %s\n", rezfile.c_str());
            view_dir(a->root(), rep);
            std::printf("\n");
            std::printf("File contains %i directories %i resources\n", rep.dirs, rep.resources);
            notify(rep);
            break;
        }
        case 'C': {
            std::printf("\nCreating rez file %s from directory %s\n", rezfile.c_str(), target);
            auto a = rez::create_from_directory(target, co, rep);
            if (verbose) std::printf("\n");
            std::printf("Finished creating %i directories %i resources\n", rep.dirs, rep.resources);
            notify(rep);
            try {
                rez::SaveOptions so;
                so.order = rez::EntryOrder::RezMgr;
                a->save(rezfile, so);
            } catch (const std::exception& e) {
                std::printf("ERROR! %s\n", e.what());
                return 1;
            }
            break;
        }
        case 'F': {
            auto a = open_checked(rezfile, rep);
            if (!a) break;
            if (a->is_ps2()) std::printf("\nnote: editing a PS2 archive is experimental (not tested on a PS2)\n");
            std::printf("\nFreshening rez file %s from directory %s\n", rezfile.c_str(), target);
            rez::add_from_directory(*a, a->root(), target, co, rep, false);
            if (verbose) std::printf("\n");
            std::printf("Finished freshening %i directories %i resources\n", rep.dirs, rep.resources);
            notify(rep);
            a->save(rezfile);
            break;
        }
        case 'S': {
            auto a = open_checked(rezfile, rep);
            if (!a) break;
            if (a->is_ps2()) {
                std::printf("\nERROR! %s is a PS2 archive: its layout (five tables per level) can't be sorted\n",
                            rezfile.c_str());
                return 1;
            }
            std::printf("\nSorting %s\n", rezfile.c_str());
            // Compact keeps the original as <name>.BAK.
            fs::path bak = fs::u8path(rezfile).replace_extension(".BAK");
            std::error_code ec;
            fs::remove(bak, ec);
            fs::copy_file(fs::u8path(rezfile), bak, ec);
            a->compact_order();
            a->header.time = rez::current_time(std::nullopt);
            a->save(rezfile);
            break;
        }
        case 'I': {
            std::unique_ptr<Archive> a;
            try {
                a = Archive::open(rezfile);
            } catch (const std::exception&) {
                std::printf("\nFailed to open file %s\n", rezfile.c_str());
                break;
            }
            std::printf("\n Rez File = %s\n", rezfile.c_str());
            std::printf("Is Sorted = %s\n", a->header.sorted ? "TRUE" : "FALSE");
            std::printf("\n");
            notify(rep);
            break;
        }
    }

    std::printf("Rez File Size = %lu\n", file_size(rezfile));
    return rep.errors ? 1 : 0;
}

// ------------------------------------------------------------------------
// Extra commands

struct Args {
    std::vector<std::string> pos;
    std::map<std::string, std::string> opt;
    std::set<std::string> flags;
};

Args parse(int argc, char** argv, int from, const std::set<std::string>& valued) {
    Args a;
    for (int i = from; i < argc; ++i) {
        std::string s = argv[i];
        if (s.size() > 2 && s.rfind("--", 0) == 0) {
            auto eq = s.find('=');
            std::string k = s.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
            if (eq != std::string::npos) a.opt[k] = s.substr(eq + 1);
            else if (valued.count(k) && i + 1 < argc) a.opt[k] = argv[++i];
            else a.flags.insert(k);
        } else if (s == "-o" && i + 1 < argc) {
            a.opt["o"] = argv[++i];
        } else {
            a.pos.push_back(s);
        }
    }
    return a;
}

std::string json_str(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += char(c); }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += char(c);
    }
    return o + "\"";
}

void walk(const Directory& d, const std::function<void(const Resource&)>& f) {
    for (auto& r : d.resources()) f(*r);
    for (auto& s : d.dirs()) walk(*s, f);
}

int cmd_list(const Args& a) {
    if (a.pos.empty()) return 2;
    auto ar = Archive::open(a.pos[0]);
    std::string filter = a.pos.size() > 1 ? a.pos[1] : "";
    std::transform(filter.begin(), filter.end(), filter.begin(), ::toupper);
    bool json = a.flags.count("json"), lng = a.flags.count("long");
    bool first = true;
    if (json) std::printf("[");
    walk(ar->root(), [&](const Resource& r) {
        std::string p = r.path('/');
        std::string up = p;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        if (!filter.empty() && up.find(filter) == std::string::npos) return;
        if (json) {
            std::printf("%s\n {\"path\":%s,\"size\":%u,\"type\":%s,\"id\":%u,\"time\":%u,\"offset\":%llu}",
                        first ? "" : ",", json_str(p).c_str(), r.size, json_str(r.type_string()).c_str(), r.id,
                        r.time, (unsigned long long)(r.source.kind == rez::DataSource::Kind::Archive ? r.source.offset : 0));
        } else if (lng) {
            std::printf("%10u  %-4s  %10u  %10u  %s\n", r.size, r.type_string().c_str(), r.id, r.time, p.c_str());
        } else {
            std::printf("%s\n", p.c_str());
        }
        first = false;
    });
    if (json) std::printf("\n]\n");
    return 0;
}

// Each table's entries that differ from table 0's: where that level's
// copies are.
int cmd_tables(const Args& a) {
    if (a.pos.empty()) return 2;
    auto p = ps2rez::Archive::open(a.pos[0]);
    std::map<uint32_t, const ps2rez::Entry*> t0;
    for (auto& e : p->tables[0]) t0[e.hash] = &e;
    std::printf("table  entries  moved  first sector  last sector\n");
    for (std::size_t t = 0; t < p->tables.size(); ++t) {
        std::size_t moved = 0;
        uint32_t lo = 0xFFFFFFFFu, hi = 0;
        for (auto& e : p->tables[t]) {
            auto it = t0.find(e.hash);
            if (t && it != t0.end() && it->second->sector == e.sector) continue;
            ++moved;
            if (e.size) lo = std::min(lo, e.sector), hi = std::max(hi, e.sector + e.sectors);
        }
        std::printf("%5zu  %7zu  %5zu  %12u  %11u\n", t, p->tables[t].size(), t ? moved : 0, lo == 0xFFFFFFFFu ? 0 : lo, hi);
    }
    return 0;
}

int ps2_header(const ps2rez::Archive& p, bool json) {
    const auto& h = p.header;
    std::size_t named = 0, files = 0;
    for (auto& e : p.tables[0]) {
        ++files;
        if (ps2rez::Names::global().find(e.hash, h.weight)) ++named;
    }
    if (json) {
        std::printf("{\"format\":\"ps2\",\"version\":%u,\"entries\":%u,\"hash_weight\":%u,\"data_sector\":%u,"
                    "\"tables\":%u,\"capacity\":%u,\"named\":%zu,\"in_iso\":%s,\"offset\":%llu,\"lsn\":%u}\n",
                    h.version, h.entries, h.weight, h.data_sector, h.tables, p.capacity(), named,
                    p.where.in_iso ? "true" : "false", (unsigned long long)p.where.offset, p.where.lsn);
        return 0;
    }
    std::printf("Format             PS2 LITH_PS2.REZ%s\n", p.where.in_iso ? " (in a disc image)" : "");
    if (p.where.in_iso)
        std::printf("Location           sector %u of the image (byte %llu), %llu bytes; volume %u sectors\n", p.where.lsn,
                    (unsigned long long)p.where.offset, (unsigned long long)p.where.size, p.where.volume_sectors);
    std::printf("Version            %u\n", h.version);
    std::printf("Entries per table  %u (room for %u)\n", h.entries, p.capacity());
    std::printf("Hash weight        %u\n", h.weight);
    std::printf("Data from sector   %u\n", h.data_sector);
    std::printf("Tables             %u\n", h.tables);
    std::printf("Names              %zu of %zu files named\n", named, files);
    return 0;
}

// header and dump read what they can of a damaged file
static rez::OpenOptions lenient() {
    rez::OpenOptions o;
    o.lenient = true;
    return o;
}

int cmd_header(const Args& a) {
    if (a.pos.empty()) return 2;
    auto ar = Archive::open(a.pos[0], lenient());
    if (ar->is_ps2()) return ps2_header(*ar->ps2, a.flags.count("json") > 0);
    auto& h = ar->header;
    if (a.flags.count("json")) {
        std::printf("{\"file_type\":%s,\"user_title\":%s,\"version\":%u,\"root_dir_pos\":%u,"
                    "\"root_dir_size\":%u,\"root_dir_time\":%u,\"next_write_pos\":%u,\"time\":%u,"
                    "\"largest_key_array\":%u,\"largest_dir_name\":%u,\"largest_rez_name\":%u,"
                    "\"largest_comment\":%u,\"sorted\":%s,\"directories\":%zu,\"resources\":%zu}\n",
                    json_str(h.file_type).c_str(), json_str(h.user_title).c_str(), h.version, h.root_dir_pos,
                    h.root_dir_size, h.root_dir_time, h.next_write_pos, h.time, h.largest_key_array,
                    h.largest_dir_name, h.largest_rez_name, h.largest_comment, h.sorted ? "true" : "false",
                    ar->root().count_dirs(), ar->root().count_resources());
        return 0;
    }
    std::printf("File type          %s\n", h.file_type.c_str());
    std::printf("User title         %s\n", h.user_title.c_str());
    std::printf("Version            %u\n", h.version);
    std::printf("Root dir           pos %u size %u time %u\n", h.root_dir_pos, h.root_dir_size, h.root_dir_time);
    std::printf("Next write pos     %u\n", h.next_write_pos);
    std::printf("Time               %u\n", h.time);
    std::printf("Largest key array  %u\n", h.largest_key_array);
    std::printf("Largest dir name   %u\n", h.largest_dir_name);
    std::printf("Largest rez name   %u\n", h.largest_rez_name);
    std::printf("Largest comment    %u\n", h.largest_comment);
    std::printf("Sorted             %s\n", h.sorted ? "yes" : "no");
    std::printf("Contents           %zu directories, %zu resources\n", ar->root().count_dirs(),
                ar->root().count_resources());
    for (auto& w : ar->warnings) std::printf("Warning            %s\n", w.c_str());
    return 0;
}

void dump_dir(const Directory& d, int depth) {
    std::string ind(depth * 2, ' ');
    std::printf("%s[%s] time %u\n", ind.c_str(), d.parent() ? d.name.c_str() : "(root)", d.time);
    for (auto& r : d.resources())
        std::printf("%s  %-4s %-24s id %-10u size %-10u at %-10llu time %u%s%s\n", ind.c_str(),
                    r->type_string().c_str(), r->name.c_str(), r->id, r->size,
                    (unsigned long long)(r->source.kind == rez::DataSource::Kind::Archive ? r->source.offset : 0), r->time,
                    r->description.empty() ? "" : (" desc \"" + r->description + "\"").c_str(),
                    r->keys.empty() ? "" : (" keys " + std::to_string(r->keys.size())).c_str());
    for (auto& s : d.dirs()) dump_dir(*s, depth + 1);
}

int cmd_dump(const Args& a) {
    if (a.pos.empty()) return 2;
    auto ar = Archive::open(a.pos[0], lenient());
    cmd_header(a);
    std::printf("\n");
    dump_dir(ar->root(), 0);
    return 0;
}

// A PS2 archive's tables: in the file, sizes and sector counts agreeing,
// every table naming the same files, no two extents overlapping. With
// --data, counts how many of the other tables' entries are copies of table
// 0's version and how many are variants (PAL layouts, other languages).
int verify_ps2(const ps2rez::Archive& p, const std::string& path, bool data) {
    int problems = 0;
    auto bad = [&](const std::string& s) {
        if (++problems <= 20) std::printf("problem: %s\n", s.c_str());
    };
    uint64_t fsize = file_size(path);
    std::set<uint32_t> names0;
    for (auto& e : p.tables[0]) names0.insert(e.hash);
    if (names0.size() != p.tables[0].size()) bad("table 0 names a file twice");
    std::map<uint32_t, ps2rez::Entry> extents;   // by sector
    std::map<uint32_t, const ps2rez::Entry*> t0;
    for (auto& e : p.tables[0]) t0[e.hash] = &e;
    std::size_t copies = 0, differ = 0;
    for (std::size_t t = 0; t < p.tables.size(); ++t) {
        std::set<uint32_t> names;
        for (auto& e : p.tables[t]) {
            names.insert(e.hash);
            if (e.sectors != (e.size + ps2rez::kSector - 1) / ps2rez::kSector)
                bad("table " + std::to_string(t) + ": sector count doesn't match the size");
            if (e.size && e.sector < p.header.data_sector) bad("table " + std::to_string(t) + ": data inside the tables");
            if (p.file_offset(e) + e.size > fsize) bad("table " + std::to_string(t) + ": data past the end of the file");
            if (!e.size) continue;
            auto [it, fresh] = extents.emplace(e.sector, e);
            if (!fresh && (it->second.hash != e.hash || it->second.size != e.size))
                bad("two files at sector " + std::to_string(e.sector));
            if (fresh && t && data) {
                auto o = t0.find(e.hash);
                if (o != t0.end() && o->second->sector != e.sector) {
                    ++copies;
                    if (e.size != o->second->size || p.read(e) != p.read(*o->second)) ++differ;
                }
            }
        }
        if (names != names0) bad("table " + std::to_string(t) + " names different files from table 0");
    }
    uint64_t prev_end = 0;
    for (auto& [s, e] : extents) {
        if (uint64_t(s) * ps2rez::kSector < prev_end) bad("extents overlap at sector " + std::to_string(s));
        prev_end = std::max(prev_end, (uint64_t(s) + e.sectors) * ps2rez::kSector);
    }
    std::printf("%s: PS2 archive, %zu tables of %u entries, %zu extents%s, %d problem(s)\n", path.c_str(),
                p.tables.size(), p.header.entries, extents.size(),
                data ? (", other tables' data: " + std::to_string(copies - differ) + " copies of table 0's, " +
                        std::to_string(differ) + " variants").c_str()
                     : "",
                problems);
    return problems ? 1 : 0;
}

int cmd_verify(const Args& a) {
    if (a.pos.empty()) return 2;
    int problems = 0;
    std::unique_ptr<Archive> ar;
    try {
        ar = Archive::open(a.pos[0]);
    } catch (const std::exception& e) {
        std::printf("FAIL %s\n", e.what());
        return 1;
    }
    if (ar->is_ps2()) return verify_ps2(*ar->ps2, a.pos[0], a.flags.count("data") > 0);
    auto size = file_size(a.pos[0]);
    // Data ranges must lie in the file and not overlap.
    std::vector<std::pair<uint64_t, uint64_t>> ranges;
    std::map<std::string, int> names;
    walk(ar->root(), [&](const Resource& r) {
        if (r.size && r.source.kind == rez::DataSource::Kind::Archive) {
            ranges.push_back({r.source.offset, r.source.offset + r.size});
            if (r.source.offset < rez::kHeaderSize) {
                std::printf("problem: %s starts inside the header\n", r.path().c_str());
                problems++;
            }
        }
        std::string k = r.path();
        std::transform(k.begin(), k.end(), k.begin(), ::toupper);
        if (++names[k] == 2) {
            std::printf("problem: %s appears more than once\n", r.path().c_str());
            problems++;
        }
    });
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t i = 1; i < ranges.size(); ++i)
        if (ranges[i].first < ranges[i - 1].second) {
            std::printf("problem: data overlaps at %llu\n", (unsigned long long)ranges[i].first);
            problems++;
        }
    if (!ranges.empty() && ranges.back().second > size) {
        std::printf("problem: data past the end of the file\n");
        problems++;
    }
    if (!rez::looks_like_lithtech(*ar)) std::printf("note: header title isn't \"LithTech Resource File\"\n");
    std::printf("%s: %zu directories, %zu resources, %d problem(s)\n", a.pos[0].c_str(), ar->root().count_dirs(),
                ar->root().count_resources(), problems);
    return problems ? 1 : 0;
}

int cmd_cat(const Args& a) {
    if (a.pos.size() < 2) return 2;
    auto ar = Archive::open(a.pos[0]);
    Resource* r = ar->root().find_path(a.pos[1]);
    if (!r) {
        std::fprintf(stderr, "lithrez: %s not found in %s\n", a.pos[1].c_str(), a.pos[0].c_str());
        return 1;
    }
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    auto d = r->read();
    if (a.flags.count("text") && butes::looks_like(d.data(), d.size())) {   // a PS2 compiled attribute file
        std::string t = butes::to_text(butes::read(d.data(), d.size()));
        std::fwrite(t.data(), 1, t.size(), stdout);
        return 0;
    }
    std::fwrite(d.data(), 1, d.size(), stdout);
    return 0;
}

int cmd_get(const Args& a) {
    if (a.pos.size() < 2 || !a.opt.count("o")) return 2;
    auto ar = Archive::open(a.pos[0]);
    rez::Report rep;
    rep.out = out;
    std::string outdir = a.opt.at("o");
    std::error_code ec;
    fs::create_directories(fs::u8path(outdir), ec);
    for (std::size_t i = 1; i < a.pos.size(); ++i) {
        if (Resource* r = ar->root().find_path(a.pos[i])) {
            rez::extract_resource(*r, outdir, rep);
        } else if (Directory* d = ar->root().find_dir_path(a.pos[i])) {
            rez::extract(*d, (fs::u8path(outdir) / fs::u8path(d->name.empty() ? "ROOT" : d->name)).u8string(), rep,
                         false);
        } else {
            rep.error(a.pos[i] + " not found");
        }
    }
    return rep.errors ? 1 : 0;
}

rez::CreateOptions create_opts(const Args& a) {
    rez::CreateOptions co;
    if (a.opt.count("time")) co.time = uint32_t(std::stoul(a.opt.at("time")));
    if (a.opt.count("title")) co.user_title = a.opt.at("title");
    if (a.opt.count("ext")) co.extensions = a.opt.at("ext");
    co.lower_case_ok = a.flags.count("lower") > 0;
    if (a.opt.count("compat")) co.zero_ids = a.opt.at("compat") != "1.00" && a.opt.at("compat") != "1.01";
    co.verbose = a.flags.count("verbose") > 0;
    return co;
}

rez::SaveOptions save_opts(const Args& a) {
    rez::SaveOptions so;
    if (a.opt.count("order") && a.opt.at("order") == "rezmgr") so.order = rez::EntryOrder::RezMgr;
    return so;
}

std::unique_ptr<Archive> open_or_new(const std::string& path, const rez::CreateOptions& co) {
    std::error_code ec;
    if (fs::exists(fs::u8path(path), ec)) return Archive::open(path);
    auto a = std::make_unique<Archive>();
    a->header.user_title = co.user_title;
    return a;
}

Directory* ensure_dir(Archive& ar, const std::string& path, uint32_t now, bool lower) {
    Directory* d = &ar.root();
    std::string cur;
    for (char c : path + "/") {
        if (c == '/' || c == '\\') {
            if (!cur.empty()) {
                if (!lower) std::transform(cur.begin(), cur.end(), cur.begin(), ::toupper);
                Directory* n = d->find_dir(cur);
                d = n ? n : d->add_dir(cur, now);
            }
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    return d;
}

int cmd_add(const Args& a) {
    if (a.pos.size() < 3) return 2;
    auto co = create_opts(a);
    auto ar = open_or_new(a.pos[0], co);
    uint32_t now = rez::current_time(co.time);
    Directory* into = ensure_dir(*ar, a.pos[1], now, co.lower_case_ok);
    rez::Report rep;
    rep.out = out;
    for (std::size_t i = 2; i < a.pos.size(); ++i) {
        fs::path p = fs::u8path(a.pos[i]);
        if (fs::is_directory(p)) {
            std::string base = p.filename().u8string();
            if (base.empty()) base = p.parent_path().filename().u8string();
            Directory* sub = ensure_dir(*ar, (into->path('/').empty() ? "" : into->path('/') + "/") + base, now,
                                        co.lower_case_ok);
            rez::add_from_directory(*ar, *sub, a.pos[i], co, rep);
        } else {
            rez::add_file(*ar, *into, a.pos[i], co, rep);
        }
    }
    ar->header.time = now;
    if (ar->is_ps2()) std::printf("note: editing a PS2 archive is experimental (not tested on a PS2)\n");
    ar->save(a.pos[0], save_opts(a));
    std::printf("%d resource(s) added or replaced\n", rep.resources);
    return rep.errors ? 1 : 0;
}

int cmd_rm(const Args& a) {
    if (a.pos.size() < 2) return 2;
    auto ar = Archive::open(a.pos[0]);
    int n = 0;
    for (std::size_t i = 1; i < a.pos.size(); ++i) {
        if (Resource* r = ar->root().find_path(a.pos[i])) {
            r->parent()->take(r);
            n++;
        } else if (Directory* d = ar->root().find_dir_path(a.pos[i]); d && d->parent()) {
            d->parent()->take_dir(d);
            n++;
        } else {
            std::fprintf(stderr, "lithrez: %s not found\n", a.pos[i].c_str());
            return 1;
        }
    }
    ar->header.time = rez::current_time(std::nullopt);
    if (ar->is_ps2()) std::printf("note: editing a PS2 archive is experimental (not tested on a PS2)\n");
    ar->save(a.pos[0], save_opts(a));
    std::printf("%d item(s) removed\n", n);
    return 0;
}

int cmd_mkdir(const Args& a) {
    if (a.pos.size() < 2) return 2;
    auto co = create_opts(a);
    auto ar = open_or_new(a.pos[0], co);
    ensure_dir(*ar, a.pos[1], rez::current_time(co.time), co.lower_case_ok);
    ar->save(a.pos[0], save_opts(a));
    return 0;
}

int cmd_diff(const Args& a) {
    if (a.pos.size() < 2) return 2;
    auto x = Archive::open(a.pos[0]), y = Archive::open(a.pos[1]);
    std::map<std::string, const Resource*> mx, my;
    walk(x->root(), [&](const Resource& r) { mx[r.path('/')] = &r; });
    walk(y->root(), [&](const Resource& r) { my[r.path('/')] = &r; });
    int diffs = 0;
    for (auto& [p, r] : mx) {
        auto it = my.find(p);
        if (it == my.end()) { std::printf("- %s\n", p.c_str()); diffs++; continue; }
        const Resource* s = it->second;
        std::string why;
        if (r->size != s->size) why += " size " + std::to_string(r->size) + "->" + std::to_string(s->size);
        if (r->id != s->id) why += " id " + std::to_string(r->id) + "->" + std::to_string(s->id);
        if (r->time != s->time) why += " time";
        if (a.flags.count("data") && r->size == s->size && r->read() != s->read()) why += " data";
        if (!why.empty()) { std::printf("~ %s:%s\n", p.c_str(), why.c_str()); diffs++; }
    }
    for (auto& [p, r] : my)
        if (!mx.count(p)) { std::printf("+ %s\n", p.c_str()); diffs++; }
    std::printf("%d difference(s)\n", diffs);
    return diffs ? 1 : 0;
}

int cmd_roundtrip(const Args& a) {
    if (a.pos.size() < 2) return 2;
    auto ar = Archive::open(a.pos[0]);
    if (ar->is_ps2()) {   // the archive alone, rebuilt from its tables
        ar->ps2->write_rebuilt(a.pos[1]);
        return 0;
    }
    rez::SaveOptions so = save_opts(a);
    so.recompute_largest = false;
    ar->save(a.pos[1], so);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        classic_help();
        return 1;
    }
    // --names <file>, anywhere: more PS2 names
    std::vector<char*> args;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--names" && i + 1 < argc) {
            try {
                ps2rez::Names::global().load_file(argv[++i]);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "lithrez: %s\n", e.what());
                return 1;
            }
        } else {
            args.push_back(argv[i]);
        }
    }
    argc = int(args.size());
    argv = args.data();
    if (argc < 2) {
        classic_help();
        return 1;
    }
    std::string c = argv[1];
    try {
        const std::set<std::string> valued = {"time", "title", "ext", "order", "compat"};
        auto A = [&] { return parse(argc, argv, 2, valued); };
        if (c == "help" || c == "--help" || c == "-h") { extra_help(); return 0; }
        if (c == "--version") { std::printf("lithrez %s\n", kVersion); return 0; }
        int r = -1;
        if (c == "list") r = cmd_list(A());
        else if (c == "header") r = cmd_header(A());
        else if (c == "dump") r = cmd_dump(A());
        else if (c == "verify") r = cmd_verify(A());
        else if (c == "cat") r = cmd_cat(A());
        else if (c == "get") r = cmd_get(A());
        else if (c == "add") r = cmd_add(A());
        else if (c == "rm") r = cmd_rm(A());
        else if (c == "mkdir") r = cmd_mkdir(A());
        else if (c == "diff") r = cmd_diff(A());
        else if (c == "roundtrip") r = cmd_roundtrip(A());
        else if (c == "tables") r = cmd_tables(A());
        if (r == 2) { extra_help(); return 2; }
        if (r >= 0) return r;

        // Classic: options after the positional arguments are allowed too.
        std::vector<char*> pos;
        std::optional<uint32_t> t;
        std::string title;
        bool have_title = false;
        for (int i = 0; i < argc; ++i) {
            std::string s = argv[i];
            if (s == "--time" && i + 1 < argc) t = uint32_t(std::stoul(argv[++i]));
            else if (s == "--title" && i + 1 < argc) { title = argv[++i]; have_title = true; }
            else if (s == "--compat" && i + 1 < argc) {
                static std::string env;
                env = argv[++i];
#if defined(_WIN32)
                _putenv_s("LITHREZ_COMPAT", env.c_str());
#else
                setenv("LITHREZ_COMPAT", env.c_str(), 1);
#endif
            }
            else pos.push_back(argv[i]);
        }
        if (pos.size() < 3) { classic_help(); return 1; }
        if (t) {
            static std::string env = std::to_string(*t);
#if defined(_WIN32)
            _putenv_s("SOURCE_DATE_EPOCH", env.c_str());
#else
            setenv("SOURCE_DATE_EPOCH", env.c_str(), 1);
#endif
        }
        if (have_title) {
            static std::string env = title;
#if defined(_WIN32)
            _putenv_s("LITHREZ_TITLE", env.empty() ? " " : env.c_str());
#else
            setenv("LITHREZ_TITLE", env.c_str(), 1);
#endif
        }
        return classic(int(pos.size()), pos.data());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "lithrez: %s\n", e.what());
        return 1;
    }
}
