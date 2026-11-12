// Archive::open's progress callback: it never goes back or past its total,
// it ends at the total for a file with RezMgr's directory gaps (the shipped
// layout), it counts every folder and file, and returning false cancels the
// open with rez::Cancelled. Archive::save's likewise, and a cancelled save
// leaves the file as it was.

#include <lithtech/rez.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
namespace rez = lithtech::rez;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void put(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}

// Opens path with a progress callback and checks what it reports.
static void check_progress(const fs::path& path, bool gaps, uint64_t dirs, uint64_t files) {
    rez::OpenProgress last;
    uint64_t calls = 0;
    bool monotonic = true, within = true;
    rez::OpenOptions o;
    o.progress = [&](const rez::OpenProgress& p) {
        if (calls && p.total == last.total && p.done < last.done) monotonic = false;
        if (p.total && p.done > p.total) within = false;
        last = p;
        ++calls;
        return true;
    };
    auto a = rez::Archive::open(path.string(), o);
    CHECK(calls > 1);
    CHECK(monotonic);
    CHECK(within);
    CHECK(last.dirs == dirs);
    CHECK(last.resources == files);
    if (gaps) {
        CHECK(last.total > 0);
        CHECK(last.done == last.total);
    }
}

int main(int argc, char** argv) {
    fs::path w = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "rez-progress-test";
    fs::remove_all(w);
    fs::path src = w / "src";
    // 12 folders of 3 files under 4 top folders, and 2 files in the root
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                put(src / ("top" + std::to_string(i)) / ("sub" + std::to_string(j)) / ("f" + std::to_string(k) + ".dat"),
                    std::string(100 + k, char('a' + i)));
    put(src / "root1.txt", "one");
    put(src / "root2.txt", "two");
    const uint64_t dirs = 1 + 4 + 12, files = 4 * 3 * 3 + 2;

    rez::CreateOptions co;
    rez::Report rep;
    auto a = rez::create_from_directory(src.string(), co, rep);
    CHECK(rep.errors == 0);

    rez::SaveOptions so;
    so.order = rez::EntryOrder::RezMgr;
    a->save((w / "gaps.rez").string(), so);
    check_progress(w / "gaps.rez", true, dirs, files);

    so.rezmgr_gaps = false;
    a->save((w / "nogaps.rez").string(), so);
    check_progress(w / "nogaps.rez", false, dirs, files);

    // cancelling on the third call
    rez::OpenOptions o;
    int calls = 0;
    o.progress = [&](const rez::OpenProgress&) { return ++calls < 3; };
    bool cancelled = false;
    try {
        rez::Archive::open((w / "gaps.rez").string(), o);
    } catch (const rez::Cancelled&) {
        cancelled = true;
    }
    CHECK(cancelled);
    CHECK(calls == 3);

    // saving: bytes of data up to their total, then the directory (total 0)
    {
        auto b = rez::Archive::open((w / "gaps.rez").string());
        rez::SaveOptions s2;
        uint64_t last_done = 0, data_total = 0, calls2 = 0;
        bool back = false, dir_step = false;
        s2.progress = [&](uint64_t done, uint64_t total) {
            if (done < last_done) back = true;
            if (total) data_total = total;
            else dir_step = true;
            last_done = done;
            ++calls2;
            return true;
        };
        b->save((w / "saved.rez").string(), s2);
        CHECK(calls2 > 2);
        CHECK(!back);
        CHECK(dir_step);
        CHECK(data_total > 0 && last_done == data_total);
    }
    // cancelling a save leaves the file as it was, and no temporary file
    {
        auto before = std::filesystem::file_size(w / "saved.rez");
        auto b = rez::Archive::open((w / "gaps.rez").string());
        rez::SaveOptions s3;
        int n = 0;
        s3.progress = [&](uint64_t, uint64_t) { return ++n < 3; };
        bool cancelled2 = false;
        try {
            b->save((w / "saved.rez").string(), s3);
        } catch (const rez::Cancelled&) {
            cancelled2 = true;
        }
        CHECK(cancelled2);
        CHECK(std::filesystem::file_size(w / "saved.rez") == before);
        CHECK(!std::filesystem::exists(w / "saved.rez.rezmgr~"));
        CHECK(rez::Archive::open((w / "saved.rez").string())->root().count_resources() == files);
    }

    std::printf(fails ? "rez_progress_test: %d failed\n" : "rez_progress_test: all passed\n", fails);
    return fails ? 1 : 0;
}
