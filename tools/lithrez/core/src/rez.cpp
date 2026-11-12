// LithTech .rez archives. The behaviour follows the 1998 RezMgr and
// LithRez (legacy/talon/src/LT2/tools/LithRez, which match the shipped lithrez.exe)
// and LithRez 1.10 (legacy/jupiter/src/Engine/tools/LithRez); see docs/REZ.md.

#include "lithtech/rez.hpp"
#include "lithtech/ps2rez.hpp"

#include <vector>
#include <string>
#include <iterator>
#include <chrono>
#include <tuple>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>

namespace fs = std::filesystem;

namespace lithtech::rez {

namespace {

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

void wr32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 24));
}

char upper(char c) { return (c >= 'a' && c <= 'z') ? char(c - 32) : c; }

std::string to_upper(std::string s) {
    for (char& c : s) c = upper(c);
    return s;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (upper(a[i]) != upper(b[i])) return false;
    return true;
}

std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

// Hash-table iteration order: bins in turn, newest entry first in each.
template <class T, class KeyFn>
std::vector<T*> hash_order(const std::vector<T*>& inserted, unsigned bins, KeyFn key) {
    std::vector<std::vector<T*>> b(bins);
    for (T* x : inserted) b[key(x) % bins].push_back(x);
    std::vector<T*> out;
    out.reserve(inserted.size());
    for (auto& bin : b)
        for (auto it = bin.rbegin(); it != bin.rend(); ++it) out.push_back(*it);
    return out;
}

constexpr unsigned kDirBins = 5;     // kDefaultDirNumHashBins
constexpr unsigned kTypeBins = 9;    // kDefaultTypNumHashBins
constexpr unsigned kNameBins = 19;   // kDefaultByNameNumHashBins

}  // namespace

// ---------------------------------------------------------------------------
// Types

uint32_t type_from_string(std::string_view ext) {
    uint32_t t = 0;
    auto* p = reinterpret_cast<uint8_t*>(&t);
    std::size_t n = std::min<std::size_t>(ext.size(), 4);
    uint8_t b[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < n; ++i) b[n - 1 - i] = uint8_t(ext[i]);
    // little-endian dword regardless of the host
    t = uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
    (void)p;
    return t;
}

std::string type_to_string(uint32_t type) {
    uint8_t b[4] = {uint8_t(type), uint8_t(type >> 8), uint8_t(type >> 16), uint8_t(type >> 24)};
    int n = b[3] ? 4 : b[2] ? 3 : b[1] ? 2 : b[0] ? 1 : 0;
    std::string s;
    for (int i = 0; i < n; ++i) s.push_back(char(b[n - 1 - i]));
    return s;
}

// ---------------------------------------------------------------------------
// Resource

std::string Resource::file_name() const {
    std::string t = type_string();
    return t.empty() ? name : name + "." + t;
}

std::string Resource::path(char sep) const {
    std::string p = parent_ ? parent_->path(sep) : std::string();
    return p.empty() ? file_name() : p + sep + file_name();
}

std::vector<uint8_t> Resource::read() const {
    switch (source.kind) {
        case DataSource::Kind::None:
            return {};
        case DataSource::Kind::Archive:
            if (!archive_) throw Error("resource has no archive");
            return archive_->read_at(source.offset, size);
        case DataSource::Kind::Memory:
            return source.bytes ? *source.bytes : std::vector<uint8_t>{};
        case DataSource::Kind::File: {
            std::ifstream f(fs::u8path(source.path), std::ios::binary);
            if (!f) throw Error("can't open " + source.path);
            if (source.slice) {
                std::vector<uint8_t> v(size);
                f.seekg(std::streamoff(source.offset));
                f.read(reinterpret_cast<char*>(v.data()), std::streamsize(size));
                if (!f) throw Error("can't read " + source.path);
                return v;
            }
            std::vector<uint8_t> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            return v;
        }
    }
    return {};
}

std::unique_ptr<Resource> Resource::copy_of(const Resource& r) {
    auto c = std::make_unique<Resource>();
    c->name = r.name;
    c->type = r.type;
    c->id = r.id;
    c->time = r.time;
    c->size = r.size;
    c->description = r.description;
    c->keys = r.keys;
    c->source = r.source;
    if (r.source.kind == DataSource::Kind::Archive) {
        if (!r.archive_) throw Error("resource has no archive");
        c->source = DataSource{};
        c->source.kind = DataSource::Kind::File;
        c->source.path = r.archive_->file_path();
        c->source.offset = r.source.offset;
        c->source.slice = true;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Directory

std::string Directory::path(char sep) const {
    if (!parent_) return {};
    std::string p = parent_->path(sep);
    return p.empty() ? name : p + sep + name;
}

std::vector<Directory*> Directory::dirs_rezmgr_order() const {
    std::vector<Directory*> v;
    for (auto& d : dirs_) v.push_back(d.get());
    return hash_order(v, kDirBins, [](Directory* d) { return unsigned(d->name.size()); });
}

std::vector<Resource*> Directory::resources_rezmgr_order() const {
    std::vector<uint32_t> types;
    for (uint32_t t : types_) types.push_back(t);
    for (auto& r : res_)
        if (std::find(types.begin(), types.end(), r->type) == types.end()) types.push_back(r->type);
    std::vector<uint32_t*> tp;
    for (auto& t : types) tp.push_back(&t);
    auto order = hash_order(tp, kTypeBins, [](uint32_t* t) { return unsigned(*t % kTypeBins); });
    std::vector<Resource*> out;
    for (uint32_t* t : order) {
        std::vector<Resource*> of;
        for (auto& r : res_)
            if (r->type == *t) of.push_back(r.get());
        for (Resource* r : hash_order(of, kNameBins, [](Resource* r) { return unsigned(r->name.size()); }))
            out.push_back(r);
    }
    return out;
}

Directory* Directory::find_dir(std::string_view n, bool ic) const {
    for (auto& d : dirs_)
        if (ic ? iequals(d->name, n) : d->name == n) return d.get();
    return nullptr;
}

Resource* Directory::find(std::string_view n, uint32_t type, bool ic) const {
    for (auto& r : res_)
        if (r->type == type && (ic ? iequals(r->name, n) : r->name == n)) return r.get();
    return nullptr;
}

namespace {
std::vector<std::string> split_path(std::string_view p) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : p) {
        if (c == '\\' || c == '/') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
}  // namespace

Directory* Directory::find_dir_path(std::string_view p, bool ic) const {
    const Directory* d = this;
    for (auto& part : split_path(p)) {
        d = d->find_dir(part, ic);
        if (!d) return nullptr;
    }
    return const_cast<Directory*>(d);
}

Resource* Directory::find_path(std::string_view p, bool ic) const {
    auto parts = split_path(p);
    if (parts.empty()) return nullptr;
    const Directory* d = this;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        d = d->find_dir(parts[i], ic);
        if (!d) return nullptr;
    }
    const std::string& leaf = parts.back();
    auto dot = leaf.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = to_upper(leaf.substr(dot + 1));
        if (ext.size() <= 4)
            if (Resource* r = d->find(leaf.substr(0, dot), type_from_string(ext), ic)) return r;
    }
    return d->find(leaf, 0, ic);
}

Directory* Directory::add_dir(std::string n, uint32_t t) {
    auto d = std::make_unique<Directory>();
    d->name = std::move(n);
    d->time = t;
    d->parent_ = this;
    d->archive_ = archive_;
    dirs_.push_back(std::move(d));
    return dirs_.back().get();
}

Resource* Directory::add(std::unique_ptr<Resource> r) {
    r->parent_ = this;
    if (archive_ && !r->archive_) r->archive_ = archive_;
    if (std::find(types_.begin(), types_.end(), r->type) == types_.end()) types_.push_back(r->type);
    res_.push_back(std::move(r));
    return res_.back().get();
}

std::unique_ptr<Directory> Directory::take_dir(Directory* d) {
    for (auto it = dirs_.begin(); it != dirs_.end(); ++it)
        if (it->get() == d) {
            auto p = std::move(*it);
            dirs_.erase(it);
            p->parent_ = nullptr;
            return p;
        }
    return nullptr;
}

std::unique_ptr<Resource> Directory::take(Resource* r) {
    for (auto it = res_.begin(); it != res_.end(); ++it)
        if (it->get() == r) {
            auto p = std::move(*it);
            res_.erase(it);
            p->parent_ = nullptr;
            return p;
        }
    return nullptr;
}

void Directory::adopt(std::unique_ptr<Directory> d) {
    d->parent_ = this;
    dirs_.push_back(std::move(d));
}

std::size_t Directory::count_resources() const {
    std::size_t n = res_.size();
    for (auto& d : dirs_) n += d->count_resources();
    return n;
}

std::size_t Directory::count_dirs() const {
    std::size_t n = dirs_.size();
    for (auto& d : dirs_) n += d->count_dirs();
    return n;
}

// ---------------------------------------------------------------------------
// Archive

struct Archive::File {
    std::ifstream in;
    uint64_t size = 0;
    std::mutex lock;
};

Archive::Archive() : root_(std::make_unique<Directory>()) { root_->archive_ = this; }
Archive::~Archive() = default;

void Archive::attach(Directory& d) {
    d.archive_ = this;
    for (auto& r : d.res_) r->archive_ = this;
    for (auto& s : d.dirs_) {
        s->parent_ = &d;
        attach(*s);
    }
}

std::vector<uint8_t> Archive::read_at(uint64_t offset, uint32_t size) const {
    if (!file_) throw Error("archive has no file");
    std::lock_guard<std::mutex> g(file_->lock);
    if (offset + size > file_->size)
        throw Error("resource data runs past the end of the file");
    std::vector<uint8_t> v(size);
    file_->in.clear();
    file_->in.seekg(std::streamoff(offset));
    file_->in.read(reinterpret_cast<char*>(v.data()), size);
    if (!file_->in) throw Error("read error");
    return v;
}

bool Archive::probe(const std::string& path) {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    uint8_t h[kHeaderSize];
    if (f.read(reinterpret_cast<char*>(h), sizeof h) && h[0] == 0x0d && h[1] == 0x0a && h[62] == 0x0d &&
        h[63] == 0x0a && h[126] == 0x1a && std::memcmp(h + 2, "RezMgr", 6) == 0)
        return true;
    return ps2rez::probe(path);
}

std::unique_ptr<Archive> Archive::open(const std::string& path, OpenOptions opts) {
    if (ps2rez::probe(path)) {
        auto a = std::make_unique<Archive>();
        a->open_ps2(path, opts);
        return a;
    }
    auto a = std::make_unique<Archive>();
    a->path_ = path;
    a->file_ = std::make_unique<File>();
    a->file_->in.open(fs::u8path(path), std::ios::binary);
    if (!a->file_->in) throw Error("can't open " + path);
    a->file_->in.seekg(0, std::ios::end);
    a->file_->size = uint64_t(a->file_->in.tellg());
    a->file_->in.seekg(0);
    if (a->file_->size < kHeaderSize) throw Error(path + ": too short to be a rez file");

    uint8_t h[kHeaderSize];
    a->file_->in.read(reinterpret_cast<char*>(h), sizeof h);
    // Open() checks the line breaks and the end-of-file mark only.
    if (h[0] != 0x0d || h[1] != 0x0a || h[62] != 0x0d || h[63] != 0x0a || h[124] != 0x0d ||
        h[125] != 0x0a || h[126] != 0x1a)
        throw Error(path + ": not a RezMgr file");
    Header& H = a->header;
    H.file_type = rtrim(std::string(reinterpret_cast<char*>(h + 2), kTitleSize));
    H.user_title = rtrim(std::string(reinterpret_cast<char*>(h + 64), kTitleSize));
    const uint8_t* v = h + 127;
    H.version = rd32(v);
    H.root_dir_pos = rd32(v + 4);
    H.root_dir_size = rd32(v + 8);
    H.root_dir_time = rd32(v + 12);
    H.next_write_pos = rd32(v + 16);
    H.time = rd32(v + 20);
    H.largest_key_array = rd32(v + 24);
    H.largest_dir_name = rd32(v + 28);
    H.largest_rez_name = rd32(v + 32);
    H.largest_comment = rd32(v + 36);
    H.sorted = h[167] != 0;
    if (H.version != kFormatVersion) {
        if (!opts.lenient)
            throw Error(path + ": RezMgr file format version " + std::to_string(H.version) +
                        " (only 1 is known)");
        a->warnings.push_back("format version " + std::to_string(H.version));
    }

    std::vector<std::pair<uint64_t, Resource*>> by_offset;
    uint64_t seq = 0;
    std::set<uint64_t> visiting;

    // Progress: the directory blocks come after the data, from the next write
    // position to the end of the root block (CRezDir::WriteAllDirs), each but
    // the last (the root) followed by a zero gap its own size, so a block and
    // its gap read out of that span is how far the open has got. A file saved
    // without the gaps overruns that count and is measured in block bytes
    // alone; a header that doesn't fit the layout leaves the total unknown.
    // (Once the gaps prove absent the count changes units; it never goes
    // back, so the bar waits until block bytes catch up.)
    OpenProgress prog;
    uint64_t with_gaps = 0, blocks = 0;
    bool no_gaps = false;
    const uint64_t dirs_end = uint64_t(H.root_dir_pos) + H.root_dir_size;
    if (H.next_write_pos <= H.root_dir_pos && dirs_end <= a->file_->size)
        prog.total = dirs_end - H.next_write_pos;
    auto report = [&]() {
        if (with_gaps > prog.total) no_gaps = true;
        prog.done = std::max(prog.done, no_gaps ? blocks : with_gaps);
        if (prog.done > prog.total) prog.total = 0;
        if (opts.progress && !opts.progress(prog)) throw Cancelled();
    };
    report();

    std::function<void(Directory&, uint32_t, uint32_t, int)> read_dir =
        [&](Directory& d, uint32_t pos, uint32_t size, int depth) {
            if (size == 0) return;
            if (depth > 256) throw Error(path + ": directories nested too deep");
            if (!visiting.insert(pos).second) throw Error(path + ": directory loop");
            if (uint64_t(pos) + size > a->file_->size) {
                if (!opts.lenient) throw Error(path + ": directory block past the end of the file");
                a->warnings.push_back("directory block past the end of the file");
                size = uint32_t(a->file_->size > pos ? a->file_->size - pos : 0);
            }
            std::vector<uint8_t> b = a->read_at(pos, size);
            blocks += size;
            with_gaps += depth ? 2 * uint64_t(size) : size;
            ++prog.dirs;
            std::size_t o = 0;
            auto need = [&](std::size_t n) {
                if (o + n > b.size()) throw Error(path + ": truncated directory entry");
            };
            auto cstr = [&]() {
                std::size_t e = o;
                while (e < b.size() && b[e]) ++e;
                if (e >= b.size()) throw Error(path + ": unterminated name");
                std::string s(reinterpret_cast<char*>(b.data() + o), e - o);
                o = e + 1;
                return s;
            };
            std::vector<std::tuple<Directory*, uint32_t, uint32_t>> subdirs;
            while (o < b.size()) {
                need(4);
                uint32_t kind = rd32(&b[o]);
                o += 4;
                if (kind == 1) {
                    need(12);
                    uint32_t dp = rd32(&b[o]), ds = rd32(&b[o + 4]), dt = rd32(&b[o + 8]);
                    o += 12;
                    Directory* sd = d.add_dir(cstr(), dt);
                    subdirs.emplace_back(sd, dp, ds);
                } else if (kind == 0) {
                    need(24);
                    auto r = std::make_unique<Resource>();
                    uint32_t rp = rd32(&b[o]);
                    r->size = rd32(&b[o + 4]);
                    r->time = rd32(&b[o + 8]);
                    r->id = rd32(&b[o + 12]);
                    r->type = rd32(&b[o + 16]);
                    uint32_t nkeys = rd32(&b[o + 20]);
                    o += 24;
                    r->name = cstr();
                    r->description = cstr();
                    need(std::size_t(nkeys) * 4);
                    for (uint32_t k = 0; k < nkeys; ++k) r->keys.push_back(rd32(&b[o + 4 * k]));
                    o += std::size_t(nkeys) * 4;
                    if (r->size) {
                        r->source.kind = DataSource::Kind::Archive;
                        r->source.offset = rp;
                        if (uint64_t(rp) + r->size > a->file_->size) {
                            if (!opts.lenient)
                                throw Error(path + ": data of " + r->name + " past the end of the file");
                            a->warnings.push_back("data of " + r->name + " past the end of the file");
                        }
                    }
                    Resource* rr = d.add(std::move(r));
                    by_offset.emplace_back((uint64_t(rp) << 24) | (seq++ & 0xffffff), rr);
                    ++prog.resources;
                } else {
                    throw Error(path + ": unknown directory entry type " + std::to_string(kind));
                }
            }
            report();
            for (auto& [sd, dp, ds] : subdirs) read_dir(*sd, dp, ds, depth + 1);
            visiting.erase(pos);
        };
    read_dir(*a->root_, H.root_dir_pos, H.root_dir_size, 0);

    std::stable_sort(by_offset.begin(), by_offset.end(),
                     [](auto& x, auto& y) { return x.first < y.first; });
    for (auto& [k, r] : by_offset) r->data_order = a->next_data_order();
    a->attach(*a->root_);
    return a;
}

void Archive::save(const std::string& path, const SaveOptions& opts) {
    if (ps2) return save_ps2(path, opts);
    // Gather every resource; data goes out in data_order.
    std::vector<Resource*> all;
    std::function<void(Directory&)> gather = [&](Directory& d) {
        for (auto& r : d.res_) all.push_back(r.get());
        for (auto& s : d.dirs_) gather(*s);
    };
    gather(*root_);
    std::stable_sort(all.begin(), all.end(),
                     [](Resource* x, Resource* y) { return x->data_order < y->data_order; });

    fs::path out = fs::u8path(path);
    fs::path tmp = out;
    tmp += ".rezmgr~";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) throw Error("can't write " + tmp.u8string());

    // Progress: resource bytes written, of their sizes as the tree has them (a
    // file added from disk may since have changed size: the total follows).
    uint64_t done = 0, total = 0;
    for (Resource* r : all) total += r->size;
    auto report = [&]() {
        if (opts.progress && !opts.progress(done, std::max(done, total))) {
            f.close();
            std::error_code ec;
            fs::remove(tmp, ec);
            throw Cancelled();
        }
    };

    std::vector<uint8_t> zero(kHeaderSize, 0);
    f.write(reinterpret_cast<char*>(zero.data()), zero.size());
    uint64_t pos = kHeaderSize;
    std::map<Resource*, uint32_t> new_pos;
    for (Resource* r : all) {
        report();
        std::vector<uint8_t> data;
        if (r->source.kind != DataSource::Kind::None) data = r->read();
        if ((r->source.kind == DataSource::Kind::File && !r->source.slice) ||
            r->source.kind == DataSource::Kind::Memory)
            r->size = uint32_t(data.size());
        if (data.size() != r->size) throw Error("size of " + r->path() + " changed");
        if (r->size == 0) {
            new_pos[r] = 0;
            continue;
        }
        if (pos + data.size() > UINT32_MAX) throw Error("archive would be larger than 4 GB");
        f.write(reinterpret_cast<char*>(data.data()), data.size());
        new_pos[r] = uint32_t(pos);
        pos += data.size();
        done += data.size();
    }
    report();
    if (opts.progress) opts.progress(done, 0);   // the directory: no stopping now
    uint32_t next_write = uint32_t(pos);

    // Directory blocks, each after its subdirectories (CRezDir::WriteAllDirs).
    std::map<Directory*, std::pair<uint32_t, uint32_t>> dir_block;
    std::size_t gap = 0;   // owed after the last block written
    std::function<void(Directory&)> write_dir = [&](Directory& d) {
        std::vector<Directory*> subs;
        std::vector<Resource*> items;
        if (opts.order == EntryOrder::RezMgr) {
            subs = d.dirs_rezmgr_order();
            items = d.resources_rezmgr_order();
        } else {
            for (auto& s : d.dirs_) subs.push_back(s.get());
            for (auto& r : d.res_) items.push_back(r.get());
        }
        for (Directory* s : subs) write_dir(*s);
        std::vector<uint8_t> b;
        for (Directory* s : subs) {
            wr32(b, 1);
            wr32(b, dir_block[s].first);
            wr32(b, dir_block[s].second);
            wr32(b, s->time);
            b.insert(b.end(), s->name.begin(), s->name.end());
            b.push_back(0);
        }
        for (Resource* r : items) {
            wr32(b, 0);
            wr32(b, new_pos[r]);
            wr32(b, r->size);
            wr32(b, r->time);
            wr32(b, r->id);
            wr32(b, r->type);
            wr32(b, uint32_t(r->keys.size()));
            b.insert(b.end(), r->name.begin(), r->name.end());
            b.push_back(0);
            b.insert(b.end(), r->description.begin(), r->description.end());
            b.push_back(0);
            for (uint32_t k : r->keys) wr32(b, k);
        }
        if (pos + 2 * b.size() > UINT32_MAX) throw Error("archive would be larger than 4 GB");
        dir_block[&d] = {uint32_t(pos), uint32_t(b.size())};
        if (opts.rezmgr_gaps && gap) {
            // the previous block's gap, now that something follows it
            std::vector<uint8_t> z(gap, 0);
            f.write(reinterpret_cast<char*>(z.data()), z.size());
        }
        f.write(reinterpret_cast<char*>(b.data()), b.size());
        pos += b.size();
        if (opts.rezmgr_gaps) {
            pos += b.size();
            gap = b.size();
        }
    };
    write_dir(*root_);

    Header& H = header;
    if (opts.recompute_largest) {
        uint32_t ld = 0, ln = 0, lk = 0, lc = 0;
        std::function<void(Directory&)> scan = [&](Directory& d) {
            for (auto& s : d.dirs_) {
                ld = std::max<uint32_t>(ld, uint32_t(s->name.size() + 1));
                scan(*s);
            }
            for (auto& r : d.res_) {
                ln = std::max<uint32_t>(ln, uint32_t(r->name.size() + 1));
                lk = std::max<uint32_t>(lk, uint32_t(r->keys.size()));
                if (!r->description.empty())
                    lc = std::max<uint32_t>(lc, uint32_t(r->description.size() + 1));
            }
        };
        scan(*root_);
        H.largest_dir_name = ld;
        H.largest_rez_name = ln;
        H.largest_key_array = lk;
        H.largest_comment = lc;
    }
    H.root_dir_pos = dir_block[root_.get()].first;
    H.root_dir_size = dir_block[root_.get()].second;
    H.next_write_pos = next_write;

    std::vector<uint8_t> hd;
    auto title = [&](const std::string& s) {
        std::string t = s.substr(0, kTitleSize);
        t.resize(kTitleSize, ' ');
        hd.insert(hd.end(), t.begin(), t.end());
    };
    hd.push_back(0x0d);
    hd.push_back(0x0a);
    title(H.file_type);
    hd.push_back(0x0d);
    hd.push_back(0x0a);
    title(H.user_title);
    hd.push_back(0x0d);
    hd.push_back(0x0a);
    hd.push_back(0x1a);
    wr32(hd, H.version);
    wr32(hd, H.root_dir_pos);
    wr32(hd, H.root_dir_size);
    wr32(hd, H.root_dir_time);
    wr32(hd, H.next_write_pos);
    wr32(hd, H.time);
    wr32(hd, H.largest_key_array);
    wr32(hd, H.largest_dir_name);
    wr32(hd, H.largest_rez_name);
    wr32(hd, H.largest_comment);
    hd.push_back(H.sorted ? 1 : 0);
    f.seekp(0);
    f.write(reinterpret_cast<char*>(hd.data()), hd.size());
    f.close();
    if (!f) throw Error("error writing " + tmp.u8string());

    // Swap in the new file and point the resources at it.
    if (file_) {
        file_->in.close();
        file_.reset();
    }
    std::error_code ec;
    fs::rename(tmp, out, ec);
    if (ec) {
        fs::remove(out, ec);
        fs::rename(tmp, out, ec);
        if (ec) throw Error("can't replace " + path + ": " + ec.message());
    }
    path_ = path;
    file_ = std::make_unique<File>();
    file_->in.open(out, std::ios::binary);
    file_->in.seekg(0, std::ios::end);
    file_->size = uint64_t(file_->in.tellg());
    for (Resource* r : all) {
        r->archive_ = this;
        r->source = DataSource{};
        if (r->size) {
            r->source.kind = DataSource::Kind::Archive;
            r->source.offset = new_pos[r];
        }
    }
}

void Archive::compact_order() {
    // CRezMgr::Compact copies into a new manager: subdirectories first, then
    // the resources, both in RezMgr order, so the copy's data is grouped by
    // directory and its tables are filled in that order.
    std::function<void(Directory&)> walk = [&](Directory& d) {
        auto subs = d.dirs_rezmgr_order();
        auto items = d.resources_rezmgr_order();
        std::vector<std::unique_ptr<Directory>> nd;
        for (Directory* s : subs) {
            walk(*s);
            nd.push_back(d.take_dir(s));
        }
        std::vector<std::unique_ptr<Resource>> nr;
        std::vector<uint32_t> types;
        for (Resource* r : items) {
            r->data_order = next_data_order();
            if (std::find(types.begin(), types.end(), r->type) == types.end()) types.push_back(r->type);
            nr.push_back(d.take(r));
        }
        for (auto& s : nd) d.adopt(std::move(s));
        d.types_ = types;
        for (auto& r : nr) {
            r->parent_ = &d;
            d.res_.push_back(std::move(r));
        }
    };
    walk(*root_);
    header.sorted = true;
}

// ---------------------------------------------------------------------------
// Commands

uint32_t current_time(const std::optional<uint32_t>& fixed) {
    if (fixed) return *fixed;
    if (const char* e = std::getenv("SOURCE_DATE_EPOCH")) {
        char* end = nullptr;
        unsigned long long v = std::strtoull(e, &end, 10);
        if (end && *end == '\0' && end != e) return uint32_t(v);
    }
    return uint32_t(std::time(nullptr));
}

namespace {

// _findfirst's order on NTFS: by name, upper-cased (NTFS's $UpCase), code
// unit by code unit. Names here are UTF-8; only ASCII is upper-cased, which
// gives the same order for every name LithTech games use.
bool ntfs_less(const std::string& a, const std::string& b) {
    std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char x = uint8_t(upper(a[i])), y = uint8_t(upper(b[i]));
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

struct DirEntry {
    std::string name;
    fs::path path;
    bool is_dir = false;
    uint64_t size = 0;
    uint32_t mtime = 0;
};

std::vector<DirEntry> list_dir(const fs::path& dir, Report& report) {
    std::vector<DirEntry> out;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        DirEntry e;
        e.path = it->path();
        e.name = it->path().filename().u8string();
        std::error_code ec2;
        auto st = fs::status(e.path, ec2);  // follows links, as Windows does
        if (ec2) {
            report.warn("Can't read " + e.path.u8string() + ": " + ec2.message());
            continue;
        }
        if (fs::is_directory(st)) {
            e.is_dir = true;
        } else if (fs::is_regular_file(st)) {
            e.size = fs::file_size(e.path, ec2);
            auto t = fs::last_write_time(e.path, ec2);
            auto sys = std::chrono::time_point_cast<std::chrono::seconds>(
                t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            e.mtime = uint32_t(sys.time_since_epoch().count());
        } else {
            report.warn("Skipping " + e.path.u8string() + " (not a file or directory)");
            continue;
        }
        out.push_back(std::move(e));
    }
    if (ec) report.error("Unable to read directory " + dir.u8string() + ": " + ec.message());
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return ntfs_less(a.name, b.name); });
    return out;
}

// LithRez 1.10's ExtCheck: "*" + the extension against each ";" item,
// ignoring case; "*.*" passes everything.
bool ext_check(const std::string& exts, const std::string& ext) {
    std::string want = "*" + ext;
    std::size_t s = 0;
    while (s <= exts.size()) {
        std::size_t e = exts.find(';', s);
        if (e == std::string::npos) e = exts.size();
        std::string item = exts.substr(s, e - s);
        if (!item.empty()) {
            if (item == "*.*") return true;
            if (iequals(item, want)) return true;
        }
        s = e + 1;
    }
    return false;
}

// atol on the all-digit names: MSVC's strtol, which clamps.
uint32_t atol_clamped(const std::string& s) {
    unsigned long long v = 0;
    for (char c : s) {
        v = v * 10 + unsigned(c - '0');
        if (v > LONG_MAX) return uint32_t(INT32_MAX);
    }
    return uint32_t(v);
}

struct NameParts {
    std::string name;   // as stored
    uint32_t type = 0;
    bool ext_too_long = false;
    std::string ext;    // with the dot, as on disk
};

NameParts split_name(const std::string& file, bool lower_ok) {
    NameParts p;
    auto dot = file.rfind('.');
    std::string fname = dot == std::string::npos ? file : file.substr(0, dot);
    p.ext = dot == std::string::npos ? std::string() : file.substr(dot);
    p.name = fname;
    // _MAX_EXT counts the dot: more than four characters after it go into
    // the name.
    if (p.ext.size() > 5) {
        p.ext_too_long = true;
        p.name += p.ext;
    }
    if (!lower_ok) p.name = to_upper(p.name);
    if (!p.ext_too_long && p.ext.size() > 1) p.type = type_from_string(to_upper(p.ext.substr(1)));
    return p;
}

std::string pad(const std::string& s, std::size_t w) {
    return s.size() >= w ? s : s + std::string(w - s.size(), ' ');
}

std::string item_line(const char* verb, const std::string& type, const std::string& name, uint32_t size,
                      uint32_t id) {
    char buf[64];
    std::string s = std::string(verb) + "Type = " + pad(type, 4) + " Name = " + pad(name, 12) + " Size = ";
    std::snprintf(buf, sizeof buf, "%-8i", int(size));
    s += buf;
    s += " ID = ";
    std::snprintf(buf, sizeof buf, "%-8i", int(id));
    s += buf;
    return s + "\n";
}

struct Builder {
    Archive& a;
    const CreateOptions& o;
    Report& rep;
    uint32_t now;
    bool replace;   // add/freshen: replace same name+type
    bool newer_only;
    int changed = 0;

    void transfer(Directory& d, const fs::path& dir, bool count_dir) {
        if (o.verbose)
            rep.say("\nCreating resource directory " + d.name + " from " + dir.u8string() + "\n");
        if (count_dir) rep.dirs++;
        uint32_t misc_id = 10000000;
        for (auto& e : list_dir(dir, rep)) {
            if (e.name.empty()) {
                rep.warn("Skipping file encountered with no base name.");
                continue;
            }
            if (e.is_dir) {
                std::string base = o.lower_case_ok ? e.name : to_upper(e.name);
                Directory* sub = d.find_dir(base, !o.lower_case_ok);
                if (sub && !replace) {
                    rep.error("Unable to create directory.  Name = " + base);
                    continue;
                }
                if (!sub) sub = d.add_dir(base, now);
                transfer(*sub, e.path, true);
                continue;
            }
            auto parts = split_name(e.name, o.lower_case_ok);
            if (!ext_check(o.extensions, parts.ext)) continue;
            if (o.warn_zero_length && e.size == 0) rep.warn("Zero length file " + e.path.u8string());
            if (parts.ext_too_long)
                rep.warn("Filename " + e.path.u8string() +
                         " extension too long.  Extension will be contained in name.");
            if (e.size > UINT32_MAX) {
                rep.error("File " + e.path.u8string() + " is larger than 4 GB");
                continue;
            }
            bool digits = std::all_of(parts.name.begin(), parts.name.end(),
                                      [](char c) { return c >= '0' && c <= '9'; });
            uint32_t id = digits ? atol_clamped(parts.name) : misc_id++;
            if (o.zero_ids) id = 0;

            Resource* existing = d.find(parts.name, parts.type, !o.lower_case_ok);
            if (existing && replace) {
                if (newer_only && e.mtime <= existing->time) continue;
                if (o.verbose)
                    rep.say(item_line("Update: ", type_to_string(parts.type), parts.name, uint32_t(e.size),
                                      existing->id));
                set_file(*existing, e);
                changed++;
                rep.resources++;
                continue;
            }
            // CreateRez refuses a name or (with items by ID) an ID already
            // used by that type in this directory.
            bool id_taken = false;
            if (!o.zero_ids)
            for (auto& r : d.resources())
                if (r->type == parts.type && r->id == id) id_taken = true;
            if (o.verbose)
                rep.say(item_line("Adding: ", type_to_string(parts.type), parts.name, uint32_t(e.size), id));
            if (existing || id_taken) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%i", int(id));
                rep.error("Unable to create resource from file " + e.path.u8string() + "  Rez Name = " +
                          parts.name + " ID = " + buf);
                continue;
            }
            auto r = std::make_unique<Resource>();
            r->name = parts.name;
            r->type = parts.type;
            r->id = id;
            Resource* rr = d.add(std::move(r));
            set_file(*rr, e);
            changed++;
            rep.resources++;
        }
    }

    void set_file(Resource& r, const DirEntry& e) {
        // SetTime(file time) is followed by Create() and Save(), which both
        // stamp the current time: the file's own time never survives.
        r.time = now;
        r.size = uint32_t(e.size);
        r.source = DataSource{};
        if (e.size) {
            r.source.kind = DataSource::Kind::File;
            r.source.path = e.path.u8string();
            r.data_order = a.next_data_order();
        }
        for (Directory* p = r.parent(); p; p = p->parent()) {
            if (!p->parent()) break;
            p->time = now;
            break;   // MarkCurTime marks the item's own directory only
        }
        a.header.time = now;
    }
};

}  // namespace

std::unique_ptr<Archive> create_from_directory(const std::string& dir, const CreateOptions& opts,
                                               Report& report) {
    auto a = std::make_unique<Archive>();
    a->header.user_title = opts.user_title;
    a->header.time = 0;
    Builder b{*a, opts, report, current_time(opts.time), false, false};
    fs::path p = fs::u8path(dir);
    if (!fs::is_directory(p)) {
        report.error("Unable to read directory " + dir);
        return a;
    }
    b.transfer(a->root(), p, true);
    a->header.sorted = true;  // ForceIsSortedFlag(TRUE)
    return a;
}

int add_from_directory(Archive& archive, Directory& into, const std::string& dir, const CreateOptions& opts,
                       Report& report, bool newer_only) {
    Builder b{archive, opts, report, current_time(opts.time), true, newer_only};
    b.transfer(into, fs::u8path(dir), true);
    if (b.changed) archive.header.sorted = false;
    return b.changed;
}

Resource* add_file(Archive& archive, Directory& into, const std::string& file, const CreateOptions& opts,
                   Report& report) {
    fs::path p = fs::u8path(file);
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        report.error("Unable to open file: " + file);
        return nullptr;
    }
    DirEntry e;
    e.name = p.filename().u8string();
    e.path = p;
    e.size = fs::file_size(p, ec);
    if (e.size > UINT32_MAX) {
        report.error("File " + file + " is larger than 4 GB");
        return nullptr;
    }
    Builder b{archive, opts, report, current_time(opts.time), true, false};
    auto parts = split_name(e.name, opts.lower_case_ok);
    Resource* r = into.find(parts.name, parts.type, !opts.lower_case_ok);
    if (!r) {
        uint32_t id = 10000000;
        bool digits = !parts.name.empty() && std::all_of(parts.name.begin(), parts.name.end(),
                                                          [](char c) { return c >= '0' && c <= '9'; });
        if (digits) {
            id = atol_clamped(parts.name);
        } else {
            for (auto& x : into.resources())
                if (x->id >= id && x->id < 20000000) id = x->id + 1;
        }
        auto nr = std::make_unique<Resource>();
        nr->name = parts.name;
        nr->type = parts.type;
        nr->id = id;
        r = into.add(std::move(nr));
    }
    b.set_file(*r, e);
    archive.header.sorted = false;
    report.resources++;
    return r;
}

namespace {

// A stored name is safe to use as a path component.
bool safe_component(const std::string& s) {
    if (s.empty() || s == "." || s == "..") return false;
    for (char c : s)
        if (c == '/' || c == '\\' || c == '\0' || c == ':') return false;
    return true;
}

}  // namespace

bool extract_resource(const Resource& r, const std::string& target_dir, Report& report) {
    std::string fname = r.file_name();
    if (!safe_component(fname)) {
        report.error("Unsafe resource name " + fname + "; not extracted");
        return false;
    }
    fs::path out = fs::u8path(target_dir) / fs::u8path(fname);
    std::vector<uint8_t> data;
    try {
        data = r.read();
    } catch (const std::exception& e) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%i", int(r.id));
        report.error("Unable to load resource. Name = " + r.name + " ID = " + buf + " (" + e.what() + ")");
        return false;
    }
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    if (!f) {
        report.error("Unable to create file: " + out.u8string());
        return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!f) {
        report.error("Unable to write file: " + out.u8string());
        return false;
    }
    f.close();
    // Give the file the resource's time, as a convenience.
    if (r.time) {
        std::error_code ec;
        auto tp = fs::file_time_type::clock::now() +
                  (std::chrono::system_clock::from_time_t(std::time_t(r.time)) - std::chrono::system_clock::now());
        fs::last_write_time(out, tp, ec);
    }
    return true;
}

void extract(const Directory& dir, const std::string& target, Report& report, bool verbose) {
    if (verbose) report.say("\nExtracting resource directory " + dir.name + " to " + target + "\n");
    report.dirs++;
    std::error_code ec;
    fs::create_directories(fs::u8path(target), ec);
    for (Resource* r : dir.resources_rezmgr_order()) {
        if (verbose) report.say(item_line("Extracting: ", r->type_string(), r->name, r->size, r->id));
        report.resources++;
        extract_resource(*r, target, report);
    }
    for (Directory* d : dir.dirs_rezmgr_order()) {
        if (!safe_component(d->name)) {
            report.error("Unsafe directory name " + d->name + "; not extracted");
            continue;
        }
        extract(*d, (fs::u8path(target) / fs::u8path(d->name)).u8string(), report, verbose);
    }
}

bool looks_like_lithtech(const Archive& a) {
    if (a.is_ps2() || a.header.user_title == kLithTechUserTitle) return true;
    for (const char* f : {"cshell.dll", "cres.dll", "sres.dll", "object.lto", "patch.txt",
                          "sounds\\dirtypesounds.", "blood2.dep", "riot.dep"}) {
        std::string s = f;
        if (!s.empty() && s.back() == '.') s.pop_back();
        if (a.root().find_path(s)) return true;
    }
    return false;
}

}  // namespace lithtech::rez

// ---------------------------------------------------------------------------
// PS2 archives as a tree

namespace lithtech::rez {

namespace {

// _UNNAMED\<8 hex digits>.<ext> at the top: the hash it stands for.
std::optional<uint32_t> unnamed_hash(const Resource& r) {
    const Directory* d = r.parent();
    if (!d || !d->parent() || d->parent()->parent()) return std::nullopt;
    std::string dn = d->name;
    std::transform(dn.begin(), dn.end(), dn.begin(), ::toupper);
    if (dn != "_UNNAMED" || r.name.size() != 8 ||
        !std::all_of(r.name.begin(), r.name.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); }))
        return std::nullopt;
    return uint32_t(std::stoul(r.name, nullptr, 16));
}

}  // namespace

void Archive::open_ps2(const std::string& path, const OpenOptions& opts) {
    ps2 = ps2rez::Archive::open(path);
    // names lithrez stored in the archive when it added files
    if (!ps2rez::Names::global().find(ps2rez::hash(ps2rez::Names::kArchiveNamesFile, ps2->header.weight), ps2->header.weight))
        ps2rez::Names::global().add(ps2rez::Names::kArchiveNamesFile);
    if (const ps2rez::Entry* e = ps2->find(ps2rez::hash(ps2rez::Names::kArchiveNamesFile, ps2->header.weight))) {
        auto text = ps2->read(*e);
        std::string line;
        auto& names = ps2rez::Names::global();
        for (char c : std::string(text.begin(), text.end()) + "\n") {
            if (c == '\n' || c == '\r') {
                if (!line.empty() && line[0] != '#' && !names.find(ps2rez::hash(line, ps2->header.weight), ps2->header.weight))
                    names.add(line);
                line.clear();
            } else {
                line.push_back(c);
            }
        }
    }
    path_ = path;
    file_ = std::make_unique<File>();
    file_->in.open(fs::u8path(path), std::ios::binary);
    if (!file_->in) throw Error("can't open " + path);
    file_->in.seekg(0, std::ios::end);
    file_->size = uint64_t(file_->in.tellg());
    header = Header{};
    header.file_type = "PS2 LITH_PS2.REZ";
    header.user_title.clear();
    std::vector<const ps2rez::Entry*> order;
    for (auto& e : ps2->tables[0]) order.push_back(&e);
    std::stable_sort(order.begin(), order.end(), [](auto* x, auto* y) { return x->sector < y->sector; });
    // Progress: entries named (name_of reads the small ones to recognise them).
    OpenProgress prog;
    prog.total = order.size();
    for (const ps2rez::Entry* e : order) {
        if (opts.progress && !opts.progress(prog)) throw Cancelled();
        ++prog.done;
        ++prog.resources;
        std::string name = ps2->name_of(*e);
        Directory* d = root_.get();
        std::size_t start = 0, sep;
        while ((sep = name.find('\\', start)) != std::string::npos) {
            std::string part = name.substr(start, sep - start);
            start = sep + 1;
            if (part.empty()) continue;
            std::transform(part.begin(), part.end(), part.begin(), ::toupper);   // folders as on the PC
            Directory* sub = d->find_dir(part);
            d = sub ? sub : d->add_dir(part);
        }
        std::string file = name.substr(start);
        auto r = std::make_unique<Resource>();
        auto dot = file.rfind('.');
        if (dot != std::string::npos && file.size() - dot - 1 <= 4) {
            r->name = file.substr(0, dot);
            std::string ext = file.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
            r->type = type_from_string(ext);
        } else {
            r->name = file;
        }
        r->id = e->hash;
        r->size = e->size;
        r->source.kind = DataSource::Kind::Archive;
        r->source.offset = ps2->file_offset(*e);
        r->data_order = next_data_order();
        if (Resource* old = d->find(r->name, r->type)) {
            // two names alike but for case: keep both reachable
            r->name += "~" + std::to_string(e->hash);
            (void)old;
        }
        d->add(std::move(r));
    }
    if (opts.progress && !opts.progress(prog)) throw Cancelled();
    attach(*root_);
}

void Archive::save_ps2(const std::string& path, const SaveOptions& opts) {
    std::map<uint64_t, const ps2rez::Entry*> by_offset;
    std::map<uint32_t, const ps2rez::Entry*> by_hash;
    for (auto& e : ps2->tables[0]) {
        by_offset.emplace(ps2->file_offset(e), &e);
        by_hash.emplace(e.hash, &e);
    }
    std::vector<Resource*> all;
    std::function<void(Directory&)> gather = [&](Directory& d) {
        for (auto& r : d.res_) all.push_back(r.get());
        for (auto& s : d.dirs_) gather(*s);
    };
    gather(*root_);
    // Names the list doesn't know go into the archive's own names file, so
    // they are there when it is opened again.
    {
        auto& names = ps2rez::Names::global();
        std::vector<std::string> extra;
        for (Resource* r : all) {
            if (unnamed_hash(*r)) continue;
            std::string p = r->path('\\');
            if (!names.find(ps2rez::hash(p, ps2->header.weight), ps2->header.weight)) extra.push_back(p);
        }
        Resource* nf = root_->find_path(ps2rez::Names::kArchiveNamesFile);
        if (!extra.empty() || nf) {
            std::string text = "# File names lithrez added to this archive (its file list has only hashes).\n";
            std::set<std::string> listed;
            if (nf) {
                auto old = nf->read();
                std::string line;
                for (char c : std::string(old.begin(), old.end()) + "\n") {
                    if (c == '\n' || c == '\r') {
                        if (!line.empty() && line[0] != '#') listed.insert(line);
                        line.clear();
                    } else {
                        line.push_back(c);
                    }
                }
            }
            for (auto& x : extra) listed.insert(x);
            for (auto& x : listed) text += x + "\n";
            for (auto& x : extra) names.add(x);
            if (!nf) {
                Directory* d = root_->find_dir("LITHTOOLS");
                if (!d) d = root_->add_dir("LITHTOOLS");
                auto r = std::make_unique<Resource>();
                r->name = "NAMES";
                r->type = type_from_string("TXT");
                nf = d->add(std::move(r));
                all.push_back(nf);
            }
            nf->source = DataSource{};
            nf->source.kind = DataSource::Kind::Memory;
            nf->source.bytes = std::make_shared<const std::vector<uint8_t>>(text.begin(), text.end());
            nf->size = uint32_t(text.size());
        }
    }
    auto hash_of = [&](const Resource& r) {
        if (auto h = unnamed_hash(r)) return *h;
        std::string p = r.path('\\');
        // "NAME~<hash>" marks the second of two names alike but for case
        if (auto t = r.name.rfind('~'); t != std::string::npos && r.source.kind == DataSource::Kind::Archive) {
            auto it = by_offset.find(r.source.offset);
            if (it != by_offset.end() && r.name.substr(t + 1) == std::to_string(it->second->hash)) return it->second->hash;
        }
        return ps2rez::hash(p, ps2->header.weight);
    };
    std::map<uint32_t, ps2rez::Archive::Change> changes;
    std::map<uint32_t, Resource*> seen;
    // Progress: resources gone through, of all; the patching itself can't stop.
    uint64_t done = 0;
    for (Resource* r : all) {
        if (opts.progress && !opts.progress(done++, all.size())) throw Cancelled();
        uint32_t h = hash_of(*r);
        if (!seen.emplace(h, r).second)
            throw Error(r->path() + " and " + seen[h]->path() + " have the same PS2 name hash");
        if (r->source.kind == DataSource::Kind::Archive && r->archive_ == this) {
            auto it = by_offset.find(r->source.offset);
            if (it != by_offset.end() && it->second->size == r->size) {
                if (it->second->hash != h) {   // renamed, moved or copied: the same entries, a new name
                    ps2rez::Archive::Change c;
                    c.kind = ps2rez::Archive::Change::Kind::Copy;
                    c.from = it->second->hash;
                    changes[h] = c;
                }
                continue;
            }
        }
        ps2rez::Archive::Change c;
        auto data = std::make_shared<std::vector<uint8_t>>(r->read());
        r->size = uint32_t(data->size());
        c.data = data;
        changes[h] = c;
    }
    for (auto& [h, e] : by_hash)
        if (!seen.count(h)) {
            ps2rez::Archive::Change c;
            c.kind = ps2rez::Archive::Change::Kind::Remove;
            changes[h] = c;
        }
    if (opts.progress && !opts.progress(done, all.size())) throw Cancelled();
    if (opts.progress) opts.progress(done, 0);   // patching: no stopping now
    // Same file: release our handle while it is patched.
    file_.reset();
    ps2->save(path, changes);
    path_ = path;
    file_ = std::make_unique<File>();
    file_->in.open(fs::u8path(path), std::ios::binary);
    if (!file_->in) throw Error("can't open " + path);
    file_->in.seekg(0, std::ios::end);
    file_->size = uint64_t(file_->in.tellg());
    // the tree now reads from the result
    for (auto& [h, r] : seen) {
        const ps2rez::Entry* e = ps2->find(h);
        if (!e) continue;
        r->id = h;
        r->size = e->size;
        r->source = DataSource{};
        r->source.kind = DataSource::Kind::Archive;
        r->source.offset = ps2->file_offset(*e);
        r->archive_ = this;
    }
}

}  // namespace lithtech::rez
