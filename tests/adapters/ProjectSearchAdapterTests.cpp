// #1003 段 G3（ADR 0167 の決定 2〜4・7）: プロジェクトの全文検索の
// adapter（Win32ProjectSearchAdapter）を 本物のフォルダ・本物のファイル・ワーカー 1
// 本で測る。一時フォルダは temporary_test_directory （#1103）の中に作り、約 100
// ファイルの木（飛ばすフォルダ・隠し属性のフォルダ・16 MiB を超える ファイル・NUL を含むファイル・1
// MiB の塊の境をまたぐ行・BOM・CRLF・長い経路・作れれば
// シンボリックリンクのフォルダ）を置く。当たりの集合が、探すべきファイルの全文に core を 1 回当てた
// 集合と一致すること・取消・新しい要求・1,000 件の打ち切り・ルートの失敗を確かめる。
// 時刻は読まない。待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest
// の制限時間で落ちる。
#include "AbsolutePath.hpp"
#include "FilePath.hpp"
#include "NativePath.hpp"
#include "ProjectChunkHits.hpp"
#include "ProjectHit.hpp"
#include "ProjectHitRows.hpp"
#include "ProjectHits.hpp"
#include "ProjectQuery.hpp"
#include "ProjectScan.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchPhase.hpp"
#include "ProjectSearchRequest.hpp"
#include "ProjectSearchRules.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Utf16.hpp"
#include "Win32ProjectSearchAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <iterator>
#include <semaphore>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::ProjectSearchBatch;
using nenenib::application::ProjectSearchHit;
using nenenib::application::ProjectSearchPhase;
using nenenib::application::ProjectSearchRequest;
using Batches = std::vector<ProjectSearchBatch>;
using Arrivals = std::counting_semaphore<>;
// 当たり 1 つ（ファイルのパスの文字列と当たり）。比べるために並べる。
using Found = std::pair<std::string, core::ProjectHit>;

constexpr std::size_t mebibyte = std::size_t{1} << 20U;

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

std::wstring wide_of(std::string_view text)
{
    return core::to_utf16(text).value_or(std::wstring{});
}

// 木の根（絶対パス・UTF-8）。開く道と同じ形のパスを当たりに期待するため、試験の置き場の中の
// 相対パスを GetFullPathNameW で絶対にした形。
std::string &tree_root()
{
    static std::string root;
    return root;
}

std::string inside(std::string_view relative)
{
    return tree_root() + "\\" + std::string(relative);
}

void make_folder(std::string_view relative)
{
    const auto path = adapters::native_path(wide_of(inside(relative)));
    expect(CreateDirectoryW(path.c_str(), nullptr) != 0, "a temporary folder is created");
}

void write_file(const std::string &path, std::string_view bytes)
{
    const auto native = adapters::native_path(wide_of(path));
    const HANDLE file = CreateFileW(native.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a temporary file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    std::size_t written = 0;
    while (written < bytes.size())
    {
        DWORD done = 0;
        const auto part = std::min<std::size_t>(bytes.size() - written, mebibyte);
        if (WriteFile(file, bytes.data() + written, static_cast<DWORD>(part), &done, nullptr) == 0)
        {
            break;
        }
        written += done;
    }
    expect(written == bytes.size(), "a temporary file is written whole");
    CloseHandle(file);
}

std::string read_file(const std::string &path)
{
    const auto native = adapters::native_path(wide_of(path));
    const HANDLE file = CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::string bytes;
    if (file == INVALID_HANDLE_VALUE)
    {
        return bytes;
    }
    std::string part(mebibyte, '\0');
    DWORD done = 0;
    while (ReadFile(file, part.data(), static_cast<DWORD>(part.size()), &done, nullptr) != 0 &&
           done > 0)
    {
        bytes.append(part, 0, done);
    }
    CloseHandle(file);
    return bytes;
}

// 決まった順の擬似乱数（線形合同法・seed 固定・時刻を読まない）。
std::uint32_t &seed()
{
    static std::uint32_t value = 12345;
    return value;
}

std::uint32_t next_random()
{
    seed() = (seed() * 1103515245U) + 12345U;
    return (seed() >> 8U) & 0xFFFFU;
}

// 当たる行と当たらない行の混ざった本文（大小・UTF-8 の字・空行・CRLF の行を含む）。
std::string random_text(std::size_t lines)
{
    static constexpr std::string_view words[] = {"alpha",
                                                 "needle",
                                                 "NEEDLE",
                                                 "Needles",
                                                 "nee dle",
                                                 "haystack",
                                                 "\xE9\x87\x9D needle",
                                                 "\xE3\x81\x82\xE3\x81\x84",
                                                 "",
                                                 "\t",
                                                 "needl",
                                                 "x"};
    std::string text;
    for (std::size_t line = 0; line < lines; ++line)
    {
        const auto count = next_random() % 4U;
        for (std::uint32_t word = 0; word < count; ++word)
        {
            text += words[next_random() % std::size(words)];
            text += ' ';
        }
        text += (next_random() % 5U) == 0 ? "\r\n" : "\n";
    }
    return text;
}

// 探すファイル（当たりを期待するファイル）の絶対パスを覚える。
std::vector<std::string> &searched_files()
{
    static std::vector<std::string> files;
    return files;
}

void searched_file(std::string_view relative, std::string_view bytes)
{
    const auto path = inside(relative);
    write_file(path, bytes);
    searched_files().push_back(path);
}

void skipped_file(std::string_view relative, std::string_view bytes)
{
    write_file(inside(relative), bytes);
}

// 普通のファイル 90（3 段のフォルダ）。
void make_plain_files()
{
    make_folder("src");
    make_folder("src\\sub");
    make_folder("src\\sub\\deep");
    for (std::size_t index = 0; index < 40; ++index)
    {
        searched_file(std::format("src\\a{}.txt", index), random_text(10 + (index % 7)));
    }
    for (std::size_t index = 0; index < 30; ++index)
    {
        searched_file(std::format("src\\sub\\b{}.cpp", index), random_text(5 + (index % 10)));
    }
    for (std::size_t index = 0; index < 20; ++index)
    {
        searched_file(std::format("src\\sub\\deep\\c{}.md", index), random_text(index % 10));
    }
}

// 1 MiB の塊の境（core::project_search_chunk_bytes）をまたぐ行に当たり、BOM・空・改行で
// 終わらない最後の行。
void make_edge_files()
{
    std::string spanning(core::project_search_chunk_bytes - 3, 'a');
    spanning.insert(0, "needle first\n");
    spanning.resize(core::project_search_chunk_bytes - 3);
    spanning.at(core::project_search_chunk_bytes - 10) = '\n';
    spanning += "needle across the chunk boundary\nlast needle without newline";
    searched_file("src\\spanning.txt", spanning);
    searched_file("src\\bom.txt", "\xEF\xBB\xBFneedle at the very start\r\nsecond needle\r\n");
    searched_file("src\\empty.txt", "");
    // UTF-8 でない行（Shift_JIS）は当たらなくてよい（追補 4）。全文の 1 回と同じかだけを見る。
    searched_file("src\\sjis.txt", "\x82\xA0 needle\nneedle\n");
}

// 飛ばすもの: 表のフォルダ・大小の違う表の名前・隠し属性のフォルダ・16 MiB を超えるファイル・
// 最初の 8 KiB に NUL を含むファイル。
void make_skipped_files()
{
    for (const auto name : {".git", "node_modules", "build", "out", "src\\Build"})
    {
        make_folder(name);
        skipped_file(std::format("{}\\x.txt", name), "needle\n");
    }
    make_folder("hidden");
    skipped_file("hidden\\x.txt", "needle\n");
    SetFileAttributesW(adapters::native_path(wide_of(inside("hidden"))).c_str(),
                       FILE_ATTRIBUTE_HIDDEN);
    std::string large(static_cast<std::size_t>(core::project_search_file_limit) + 1, 'z');
    large.replace(0, 7, "needle\n");
    skipped_file("src\\large.txt", large);
    std::string binary = "needle\n";
    binary += '\0';
    binary += "needle\n";
    skipped_file("src\\binary.bin", binary);
}

// MAX_PATH を超える経路のファイル（`\\?\` の形で歩く）。
void make_long_path_file()
{
    std::string folder = "long";
    make_folder(folder);
    for (std::size_t depth = 0; depth < 3; ++depth)
    {
        folder += "\\" + std::string(100, static_cast<char>('p' + depth));
        make_folder(folder);
    }
    searched_file(folder + "\\deep.txt", "a long path needle\n");
}

// シンボリックリンクのフォルダ（src を指す）は歩かない（循環を避ける）。作れない機械では
// 確かめずに 1 行知らせる（開発者モードでない・権限が無い）。
void make_linked_folder()
{
    const auto link = adapters::native_path(wide_of(inside("link")));
    const auto target = adapters::native_path(wide_of(inside("src")));
    if (CreateSymbolicLinkW(link.c_str(), target.c_str(),
                            SYMBOLIC_LINK_FLAG_DIRECTORY |
                                SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0)
    {
        std::printf("note: a directory symbolic link could not be made (error %lu); the "
                    "reparse point folder is not checked\n",
                    static_cast<unsigned long>(GetLastError()));
    }
}

void make_tree()
{
    make_folder("");
    make_plain_files();
    make_edge_files();
    make_skipped_files();
    make_long_path_file();
    make_linked_folder();
    make_folder("none");
    // 1,000 件を超える当たりの木は、木の根の外の隣（木の全体の検索を打ち切らせない）。
    make_folder("..\\nib-project-search-many");
    std::string many;
    for (std::size_t line = 0; line < core::project_search_hit_limit + 500; ++line)
    {
        many += "needle\n";
    }
    write_file(inside("..\\nib-project-search-many\\lines.txt"), many);
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

core::ProjectQuery query_of(std::string_view text)
{
    return core::ProjectQuery::parse(text).value();
}

bool final_of(const Batches &batches, std::uint64_t ticket)
{
    return std::ranges::any_of(batches,
                               [ticket](const ProjectSearchBatch &batch)
                               {
                                   return batch.ticket == ticket &&
                                          batch.progress.phase != ProjectSearchPhase::searching;
                               });
}

// 合図を待っては受け取り、券 ticket の最後の batch が来るまで続ける。
Batches until_final(adapters::Win32ProjectSearchAdapter &search, Arrivals &arrived,
                    std::uint64_t ticket)
{
    Batches seen;
    while (!final_of(seen, ticket))
    {
        arrived.acquire();
        for (auto &batch : search.collect())
        {
            seen.push_back(std::move(batch));
        }
    }
    return seen;
}

Batches of_ticket(const Batches &batches, std::uint64_t ticket)
{
    Batches mine;
    std::ranges::copy_if(batches, std::back_inserter(mine),
                         [ticket](const ProjectSearchBatch &batch)
                         { return batch.ticket == ticket; });
    return mine;
}

std::vector<Found> found_of(const Batches &batches)
{
    std::vector<Found> found;
    for (const auto &batch : batches)
    {
        for (const ProjectSearchHit &hit : batch.hits)
        {
            found.emplace_back(std::string(hit.file.text()), hit.hit);
        }
    }
    return found;
}

void sort_found(std::vector<Found> &found)
{
    std::ranges::sort(found,
                      [](const Found &left, const Found &right)
                      {
                          return std::tie(left.first, left.second.line.value) <
                                 std::tie(right.first, right.second.line.value);
                      });
}

// folder の下の探すべきファイルの全文に core を 1 回当てた当たり（決定 7 の比べる相手）。
std::vector<Found> expected_hits(std::string_view folder, const core::ProjectQuery &query)
{
    std::vector<Found> found;
    for (const auto &path : searched_files())
    {
        if (!path.starts_with(std::string(folder) + "\\"))
        {
            continue;
        }
        const auto bytes = read_file(path);
        const auto room = core::project_search_hit_limit;
        auto whole = core::project_hits_in(core::ProjectScan{}, bytes, query, room);
        const auto last = core::project_hits_finished(whole.next, query, room);
        whole.hits.insert(whole.hits.end(), last.hits.begin(), last.hits.end());
        for (auto &hit : whole.hits)
        {
            found.emplace_back(path, std::move(hit));
        }
    }
    sort_found(found);
    return found;
}

// 1 つの要求を最後まで流した batch。
Batches searched(std::string_view root, std::string_view pattern)
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32ProjectSearchAdapter search(worker);
    search.bind([&arrived] { arrived.release(); });
    search.request(ProjectSearchRequest{path_of(root), query_of(pattern), 1});
    auto batches = until_final(search, arrived, 1);
    expect(search.collect().empty(), "nothing is left after the last batch");
    return batches;
}

void verify_same_hits_as_whole_files(std::string_view pattern, const char *message)
{
    const auto batches = searched(inside("src"), pattern);
    auto found = found_of(batches);
    sort_found(found);
    expect(found == expected_hits(inside("src"), query_of(pattern)), message);
}

void verify_tree()
{
    const auto batches = searched(tree_root(), "needle");
    const auto &last = batches.back().progress;
    expect(last.phase == ProjectSearchPhase::complete && last.searched == searched_files().size() &&
               last.found == last.searched,
           "the whole tree is searched and counts every searched file once");
    auto found = found_of(batches);
    sort_found(found);
    expect(found == expected_hits(tree_root(), query_of("needle")),
           "the tree's hits equal one whole-file pass of core over every searched file");
    const auto outside = [](const Found &hit)
    {
        const std::string_view file = hit.first;
        return file.find("\\.git\\") != std::string_view::npos ||
               file.find("\\node_modules\\") != std::string_view::npos ||
               file.find("\\build\\") != std::string_view::npos ||
               file.find("\\Build\\") != std::string_view::npos ||
               file.find("\\out\\") != std::string_view::npos ||
               file.find("\\hidden\\") != std::string_view::npos ||
               file.find("\\link\\") != std::string_view::npos || file.ends_with("large.txt") ||
               file.ends_with("binary.bin");
    };
    expect(std::ranges::none_of(found, outside),
           "skipped folders, hidden and linked folders, large and binary files give no hits");
    expect(std::ranges::all_of(batches, [](const ProjectSearchBatch &batch)
                               { return batch.progress.searched <= batch.progress.found; }),
           "searched never exceeds found");
}

void verify_whole_files()
{
    verify_same_hits_as_whole_files(
        "needle", "the literal hits under src equal one whole-file pass of core per file");
    verify_same_hits_as_whole_files(
        "/ne\\+dle", "the pattern hits under src equal one whole-file pass of core per file");
    const auto batches = searched(inside("src"), "needle");
    const auto &last = batches.back().progress;
    // src の下の探すファイルは src の外の長い経路の 1 つを除いた全部（飛ばしたものは数えない）。
    const auto expected_files = searched_files().size() - 1;
    expect(last.phase == ProjectSearchPhase::complete && last.searched == expected_files &&
               last.found == expected_files,
           "a complete search counts every searched file and none of the skipped ones");
}

// 当たりのパスは開く道（起動引数の absolute_file_path）と同じ形で、ルートからの相対パスが
// 区切り `\` で出る（V-1764 の L-5）。ルートが区切りで終わっても区切りを重ねない。
void verify_hit_paths()
{
    const auto plain = found_of(searched(tree_root() + "\\long", "needle"));
    const auto ended = found_of(searched(tree_root() + "\\long\\", "needle"));
    expect(plain.size() == 1 && ended.size() == 1 && plain.front().first == ended.front().first,
           "a root with a trailing separator gives the same file path");
    expect(!plain.empty() && plain.front().first.size() > MAX_PATH,
           "a file beyond MAX_PATH is searched");
    const auto all = found_of(searched(inside("src"), "needle"));
    const auto opened = [](const Found &hit)
    {
        const auto absolute = adapters::absolute_file_path(wide_of(hit.first));
        return absolute.has_value() && absolute.value().text() == hit.first;
    };
    expect(!all.empty() && std::ranges::all_of(all, opened),
           "every hit path is already the form the open path makes");
    const auto relative = core::project_relative_path(path_of(inside("src")),
                                                      path_of(inside("src\\sub\\deep\\c3.md")));
    expect(relative == "sub\\deep\\c3.md", "the relative path uses the Windows separator");
}

void verify_ends()
{
    const auto none = searched(inside("none"), "needle");
    expect(none.size() == 1 && none.back().progress.phase == ProjectSearchPhase::complete &&
               none.back().hits.empty(),
           "an empty folder gives one empty complete batch");
    const auto missing = searched(inside("missing"), "needle");
    expect(missing.back().progress.phase == ProjectSearchPhase::failed,
           "a root that cannot be opened fails");
    const auto many = searched(inside("..\\nib-project-search-many"), "needle");
    expect(many.back().progress.phase == ProjectSearchPhase::truncated &&
               found_of(many).size() == core::project_search_hit_limit,
           "1,000 hits end the search as truncated, not complete (V-1764 L-1)");
}

// 取消と新しい要求（決定 4）。取り消した券・置き換えられた券は最後の batch を出さない。古い券の
// 取消は今の要求を止めない。
void verify_cancel()
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32ProjectSearchAdapter search(worker);
    search.bind([&arrived] { arrived.release(); });
    search.request(ProjectSearchRequest{path_of(tree_root()), query_of("needle"), 1});
    search.cancel(1);
    search.request(ProjectSearchRequest{path_of(tree_root()), query_of("needle"), 2});
    search.request(ProjectSearchRequest{path_of(inside("src")), query_of("needle"), 3});
    search.cancel(2);
    const auto batches = until_final(search, arrived, 3);
    expect(!final_of(batches, 1) && !final_of(batches, 2),
           "a cancelled or replaced request never sends its last batch");
    const auto mine = of_ticket(batches, 3);
    expect(!mine.empty() && mine.back().progress.phase == ProjectSearchPhase::complete,
           "cancelling an old ticket does not stop the current request");
    search.cancel(3);
    expect(search.collect().empty(), "nothing arrives after the last batch");
}

void verify_never_started()
{
    adapters::Win32Worker worker;
    {
        adapters::Win32ProjectSearchAdapter search(worker);
        expect(search.collect().empty(), "nothing is collected before a request");
    }
    expect(!worker.started(), "a worker without a request has no thread");
}

int run_tests()
{
    wchar_t full[MAX_PATH]{};
    if (GetFullPathNameW(L"nib-project-search", MAX_PATH, full, nullptr) == 0)
    {
        std::fprintf(stderr, "FAIL: the tree root has no full path\n");
        return 1;
    }
    tree_root() = core::to_utf8(std::wstring_view(full)).value_or(std::string{});
    make_tree();
    verify_never_started();
    verify_whole_files();
    verify_tree();
    verify_hit_paths();
    verify_ends();
    verify_cancel();
    std::printf("Project search adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"project-search");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    const int result = run_tests();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
