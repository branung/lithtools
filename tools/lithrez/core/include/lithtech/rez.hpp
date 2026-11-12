// LithTech resource files (.rez): RezMgr version 1, the one format every
// LithTech game from Shogo (1998) to Tron 2.0 and Contract J.A.C.K. (2003)
// ships. See docs/REZ.md for the layout and the evidence.
//
// An Archive is the whole file in memory as a tree of directories and
// resources; resource data stays in the file (or wherever it came from)
// until it is read or the archive is saved.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lithtech::ps2rez {
class Archive;
}

namespace lithtech::rez {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown by Archive::open when its progress callback asks it to stop.
class Cancelled : public Error {
public:
    Cancelled() : Error("cancelled") {}
};

// Sizes from the file format (rezmgr.cpp, FileMainHeaderStruct).
inline constexpr std::size_t kTitleSize = 60;
inline constexpr std::size_t kHeaderSize = 168;
inline constexpr uint32_t kFormatVersion = 1;

// The text in the first title line of every RezMgr file.
inline constexpr std::string_view kFileTypeTitle =
    "RezMgr Version 1 Copyright (C) 1995 MONOLITH INC.";
// The second line LithRez writes (LithTechUserTitle); Shogo and Blood 2
// files, and some later ones, leave it blank.
inline constexpr std::string_view kLithTechUserTitle = "LithTech Resource File";

// A resource type: up to four characters, the file extension upper-cased,
// stored as a little-endian dword with the characters reversed
// (CRezMgr::StrToType), so "DTX" is 'X','T','D',0.
uint32_t type_from_string(std::string_view ext);
std::string type_to_string(uint32_t type);

class Directory;
class Archive;

// Where a resource's bytes are.
struct DataSource {
    enum class Kind { None, Archive, Memory, File };
    Kind kind = Kind::None;
    uint64_t offset = 0;                              // Archive (or File slice)
    std::shared_ptr<const std::vector<uint8_t>> bytes;  // Memory
    std::string path;                                 // File
    bool slice = false;   // File: `size` bytes at `offset` (e.g. inside another rez)
};

class Resource {
public:
    std::string name;          // without the extension
    uint32_t type = 0;         // type_from_string(extension)
    uint32_t id = 0;
    uint32_t time = 0;         // seconds since 1970, as time_t
    uint32_t size = 0;
    std::string description;   // "comment" in RezMgr; empty in every shipped file
    std::vector<uint32_t> keys;  // empty in every shipped file

    DataSource source;
    // Order of the data in the file (the order RezMgr wrote it).
    uint64_t data_order = 0;

    Directory* parent() const { return parent_; }
    std::string type_string() const { return type_to_string(type); }
    // NAME.EXT (or NAME if the type is empty)
    std::string file_name() const;
    // DIR\SUB\NAME.EXT from the root
    std::string path(char separator = '\\') const;

    // The bytes, from wherever they are.
    std::vector<uint8_t> read() const;
    // A copy of r (name, type, id, time, description, keys) whose data is
    // read from r's file, for moving resources between archives.
    static std::unique_ptr<Resource> copy_of(const Resource& r);

private:
    friend class Directory;
    friend class Archive;
    Directory* parent_ = nullptr;
    const Archive* archive_ = nullptr;
};

class Directory {
public:
    std::string name;   // "" for the root
    uint32_t time = 0;

    Directory* parent() const { return parent_; }
    std::string path(char separator = '\\') const;

    // In the order they were added (or stored in the file).
    const std::vector<std::unique_ptr<Directory>>& dirs() const { return dirs_; }
    const std::vector<std::unique_ptr<Resource>>& resources() const { return res_; }

    // RezMgr's own order: what LithRez lists, extracts and writes. Its hash
    // tables (5 bins by name length for directories, 9 by value for types,
    // 19 by name length for resources) put each new entry first in its bin.
    std::vector<Directory*> dirs_rezmgr_order() const;
    std::vector<Resource*> resources_rezmgr_order() const;

    Directory* find_dir(std::string_view name, bool ignore_case = true) const;
    Resource* find(std::string_view name, uint32_t type, bool ignore_case = true) const;
    // A path such as "CHARS\\SKINS\\FOO.DTX" or "chars/skins/foo.dtx".
    Resource* find_path(std::string_view path, bool ignore_case = true) const;
    Directory* find_dir_path(std::string_view path, bool ignore_case = true) const;

    Directory* add_dir(std::string dir_name, uint32_t time = 0);
    Resource* add(std::unique_ptr<Resource> r);
    std::unique_ptr<Directory> take_dir(Directory* d);
    std::unique_ptr<Resource> take(Resource* r);
    void adopt(std::unique_ptr<Directory> d);

    std::size_t count_resources() const;  // recursively
    std::size_t count_dirs() const;       // recursively, not counting this one

    Archive* archive() const { return archive_; }

private:
    friend class Archive;
    Directory* parent_ = nullptr;
    Archive* archive_ = nullptr;
    std::vector<std::unique_ptr<Directory>> dirs_;
    std::vector<std::unique_ptr<Resource>> res_;
    // The order types were first used in this directory (RezMgr keeps a
    // type until the directory goes, even when its resources are removed).
    std::vector<uint32_t> types_;
};

// The header fields that aren't derived from the tree.
struct Header {
    std::string file_type{kFileTypeTitle};   // first title line (trailing spaces removed)
    std::string user_title;                  // second title line (trailing spaces removed)
    uint32_t version = kFormatVersion;
    uint32_t root_dir_time = 0;   // 0 from LithRez 1.00; uninitialised memory from 1.10
    uint32_t time = 0;            // last time anything was modified
    uint32_t largest_key_array = 0;
    uint32_t largest_dir_name = 0;
    uint32_t largest_rez_name = 0;
    uint32_t largest_comment = 0;
    bool sorted = true;
    // As stored, for inspection.
    uint32_t root_dir_pos = 0, root_dir_size = 0, next_write_pos = 0;
};

enum class EntryOrder {
    Stored,   // each directory's entries in the order they were added / read
    RezMgr,   // the hash-table order RezMgr writes (LithRez's create output)
};

struct SaveOptions {
    EntryOrder order = EntryOrder::Stored;
    // Leave a zero-filled gap the size of each directory block after it, as
    // RezMgr does (WriteAllDirs advances the write position by the block's
    // size after WriteDirBlock already has). Every shipped file has them.
    bool rezmgr_gaps = true;
    // Recompute the largest-name fields from the tree (RezMgr's way).
    // Off, the header's values are written as they are.
    bool recompute_largest = true;
    // Called as the data is written (from the calling thread), with the bytes
    // of resource data written so far and their total (0 while the last step,
    // the directory or a PS2 archive's patching, is being written). Returning
    // false stops the save, which then throws Cancelled and leaves the file
    // as it was; in that last step it can no longer stop, and the return is
    // ignored.
    std::function<bool(uint64_t done, uint64_t total)> progress;
};

// How far Archive::open has got, for a progress bar: done of total, in
// whatever units the archive offers (directory bytes for a RezMgr file,
// entries for a PS2 one); total is 0 while it isn't known.
struct OpenProgress {
    uint64_t done = 0;
    uint64_t total = 0;
    uint64_t dirs = 0;       // directories read so far
    uint64_t resources = 0;  // resources read so far
};

struct OpenOptions {
    // Accept files whose version isn't 1, and directory blocks that run past
    // the end of the file, reading what can be read.
    bool lenient = false;
    // Called as the directory is read (from the calling thread); returning
    // false stops the open, which then throws Cancelled.
    std::function<bool(const OpenProgress&)> progress;
};

class Archive {
public:
    Archive();
    ~Archive();
    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;

    // A RezMgr file, or a PS2 LITH_PS2.REZ (or a PS2 disc image holding
    // one), whose files are named from the recovered names (ps2rez.hpp).
    static std::unique_ptr<Archive> open(const std::string& path, OpenOptions opts = {});
    // Whether the file starts like a RezMgr file or is a PS2 archive.
    static bool probe(const std::string& path);

    // Set for a PS2 archive: its tables, which save() updates from the tree
    // (ps2rez::Archive::save) instead of writing a RezMgr file. Resource IDs
    // are the name hashes; files without a recovered name are
    // _UNNAMED\<HASH>.<ext>.
    std::shared_ptr<ps2rez::Archive> ps2;
    bool is_ps2() const { return ps2 != nullptr; }

    Header header;
    Directory& root() { return *root_; }
    const Directory& root() const { return *root_; }
    const std::string& file_path() const { return path_; }

    // Read size bytes at offset of the archive file.
    std::vector<uint8_t> read_at(uint64_t offset, uint32_t size) const;

    // Write the archive to path (to a temporary file first, so path may be
    // the file it was read from). Data is written in data_order, then each
    // directory block after its subdirectories, the root last, as RezMgr
    // does.
    void save(const std::string& path, const SaveOptions& opts = {});

    // RezMgr's Compact ("lithrez s"): rewrite with each directory's data
    // together, subdirectories first, in RezMgr order.
    void compact_order();

    // Warnings met while reading (lenient mode).
    std::vector<std::string> warnings;

    uint64_t next_data_order() { return ++data_counter_; }

private:
    std::string path_;
    std::unique_ptr<Directory> root_;
    struct File;
    std::unique_ptr<File> file_;
    uint64_t data_counter_ = 0;
    void attach(Directory& d);
    void open_ps2(const std::string& path, const OpenOptions& opts);
    void save_ps2(const std::string& path, const SaveOptions& opts);
    friend class Directory;
};

// ---------------------------------------------------------------------------
// LithRez's commands, as library calls.

struct Report {
    int dirs = 0, resources = 0, warnings = 0, errors = 0;
    std::function<void(const std::string&)> out;     // normal output
    void say(const std::string& s) const { if (out) out(s); }
    void warn(const std::string& s) { ++warnings; say("WARNING! " + s + "\n"); }
    void error(const std::string& s) { ++errors; say("ERROR! " + s + "\n"); }
};

struct CreateOptions {
    bool lower_case_ok = false;     // 'l': keep names' case
    bool warn_zero_length = false;  // 'z'
    bool verbose = false;           // 'v'
    // LithRez 1.10: only these extensions ("*.dtx;*.ltb"; "*.*" for all).
    std::string extensions = "*.*";
    std::string user_title{kLithTechUserTitle};
    // LithRez 1.02 (2002) and later write 0 for every ID (their RezMgr has
    // no IDs); 1.00 and 1.01 number them (all-digit names, else 10000000 up).
    bool zero_ids = false;
    // The time stamped on everything (RezMgr uses the clock); nullopt for
    // the clock. SOURCE_DATE_EPOCH, if set, is used when this is nullopt.
    std::optional<uint32_t> time;
};

// "lithrez c": a new archive from a directory tree, as LithRez builds it
// (the directory listing in Windows' order, names upper-cased, IDs from
// all-digit names else counting from 10000000 in each directory).
std::unique_ptr<Archive> create_from_directory(const std::string& dir, const CreateOptions& opts,
                                               Report& report);

// Add the files and directories under dir to `into` (WinRez's Insert and
// "lithrez f"): existing resources with the same name and type are
// replaced. Returns the number of resources added or replaced.
int add_from_directory(Archive& archive, Directory& into, const std::string& dir,
                       const CreateOptions& opts, Report& report, bool freshen_only_newer = false);

// Add one file as a resource of `into`; replaces a resource of the same
// name and type. Returns it, or nullptr (with an error reported).
Resource* add_file(Archive& archive, Directory& into, const std::string& file,
                   const CreateOptions& opts, Report& report);

// "lithrez x": write dir's tree under target (directories made as needed).
void extract(const Directory& dir, const std::string& target, Report& report, bool verbose);
bool extract_resource(const Resource& r, const std::string& target_dir, Report& report);

// The time to stamp: opts.time, else SOURCE_DATE_EPOCH, else now.
uint32_t current_time(const std::optional<uint32_t>& fixed);

// Names of known files in Shogo/Blood 2 archives, whose header is blank
// (LithRez's CheckLithHeader accepts those).
bool looks_like_lithtech(const Archive& a);

}  // namespace lithtech::rez
